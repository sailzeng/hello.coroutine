//�������������һ���������ӣ�������һ���߳��лָ�Э�̡�


#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS   1
#endif
#ifndef _CRT_SECURE_NO_DEPRECATE
#define _CRT_SECURE_NO_DEPRECATE  1
#endif

#include <coroutine>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <future> 
#include <chrono>
#include <thread>

//!coro_ret Э�̺����ķ���ֵ���ڲ�����promise_type����ŵ����
template <typename T>
struct coro_ret
{
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;
    //! Э�̾��
    handle_type coro_handle_;

    coro_ret(handle_type h)
        : coro_handle_(h)
    {
    }
    coro_ret(const coro_ret&) = delete;
    coro_ret(coro_ret&& s)
        : coro_handle_(s.coro_)
    {
        s.coro_handle_ = nullptr;
    }
    ~coro_ret()
    {
        //!��������
        if (coro_handle_)
            coro_handle_.destroy();
    }
    coro_ret& operator=(const coro_ret&) = delete;
    coro_ret& operator=(coro_ret&& s)
    {
        coro_handle_ = s.coro_handle_;
        s.coro_handle_ = nullptr;
        return *this;
    }

    //!�ָ�Э�̣������Ƿ����
    bool move_next()
    {
        coro_handle_.resume();
        return coro_handle_.done();
    }
    //!ͨ��promise��ȡ���ݣ�����ֵ
    T get()
    {
        return coro_handle_.promise().return_data_;
    }
    //!promise_type���ǳ�ŵ���󣬳�ŵ��������Э�����⽻��
    struct promise_type
    {
        promise_type() = default;
        ~promise_type() = default;

        //!����Э�̷���ֵ
        auto get_return_object()
        {
            return coro_ret<T>{handle_type::from_promise(*this)};
        }

        //! ע���������,���صľ���awaiter
        //! �������std::suspend_never{}���Ͳ�����
        //! ����std::suspend_always{} ����
        //! ��Ȼ��Ҳ���Է�������awaiter
        auto initial_suspend()
        {
            return std::suspend_never{};
            //return std::suspend_always{};
        }
        //!co_return ����������ᱻ����
        void return_value(T v)
        {
            return_data_ = v;
            return;
        }
        //!
        auto yield_value(T v)
        {
            std::cout << "yield_value invoked." << std::endl;
            return_data_ = v;
            return std::suspend_always{};
        }
        //! ��Э������˳�����õĽӿڡ�
        //! �� final_suspend ���� std::suspend_always ����Ҫ�û����е���
        //! handle.destroy() �������٣���ע��final_suspend������ʱЭ���Ѿ�����
        //! ����std::suspend_always���������Э�̣�ʵ�� VSC++ 2022��
        auto final_suspend() noexcept
        {
            std::cout << "final_suspend invoked." << std::endl;
            return std::suspend_always{};
        }
        //
        void unhandled_exception()
        {
            std::exit(1);
        }
        //����ֵ
        T return_data_;
    };
};

int read_file(const char* filename,
              char* buffer,
              size_t buf_len,
              size_t* read_len,
              std::coroutine_handle<> coro_hdl)
{
    int result = 0;
    size_t len = 0;
    *read_len = 0;
    //���ļ�
    FILE* fd = ::fopen(filename, "r+");
    if (nullptr == fd)
    {
        result = -1;
        goto READ_FILE_END;
    }
    //��ȡ����
    len = ::fread(buffer, 1, buf_len, fd);
    ::fclose(fd);
    if (len <= 0)
    {
        result = -1;
        goto READ_FILE_END;
    }

    *read_len = len;
    result = 0;

    //�������һ��
READ_FILE_END:
    //�仯�㣺��AIO���߳�����ָ�Э�̡�
    coro_hdl.resume();
    return result;
}


struct await_read_file
{
    await_read_file(const char* filename,
                    char* buffer,
                    size_t buf_len,
                    size_t* read_len)
    {
        filename_ = filename;
        buffer_ = buffer;
        buf_len_ = buf_len;
        read_len_ = read_len;
    };
    ~await_read_file() = default;

    bool await_ready()
    {
        return false;
    }
    //����Ĳ����������첽���ļ�������Ȼ��ȴ�����
    void await_suspend(std::coroutine_handle<> awaiting)
    {
        fur_ = std::async(std::launch::async,
                          &read_file,
                          filename_,
                          buffer_,
                          buf_len_,
                          read_len_,
                          awaiting);
        //����������ط����еȴ���
    }
    //���ؽ��
    int await_resume()
    {
        result_ = fur_.get();
        return result_;
    }

    //���ļ��Ĳ���������ֵ
    int result_ = -1;
    const char* filename_ = nullptr;
    char* buffer_ = nullptr;
    size_t buf_len_ = 0;
    size_t* read_len_ = nullptr;

    std::future<int> fur_;

    //!Э�̵ľ��
    std::coroutine_handle<> awaiting_;
};



//�����һ��Э�̺���
coro_ret<int> coroutine_await(const char* filename,
                              char* buffer,
                              size_t buf_len,
                              size_t* read_len)
{
    int ret = co_await await_read_file(filename,
                                       buffer,
                                       buf_len,
                                       read_len);
    //������ʵû��ִ�е���
    std::cout << "coroutine_await ret= " << ret << std::endl;
    if (ret == 0)
    {
        std::cout << "await_read_file read_len= " << *read_len << std::endl;
    }
    co_return 0;
}

int main(int argc, char* argv[])
{
    using namespace std::chrono_literals;
    std::cout << "Start coroutine_await ()\n";
    //����Э��
    char buffer[1024];
    size_t read_len = 0;
    std::cout << "Start coroutine_await ()\n";
    auto c_r = coroutine_await("E:/TEST001/aio_test_001.txt",
                               buffer,
                               1024,
                               &read_len);
    std::cout << "End coroutine_await ()\n";
    std::this_thread::sleep_for(20000ms);
    return 0;
}


