// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/io_completion_port_drainer.h"

#include <windows.h>

#include <stdint.h>

#include <atomic>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/message_loop/message_pump_for_io.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/bind_post_task.h"
#include "base/task/current_thread.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/unguessable_token.h"
#include "base/win/scoped_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

class BlockingCompletionHandler : public base::MessagePumpForIO::IOHandler {
 public:
  BlockingCompletionHandler() : base::MessagePumpForIO::IOHandler(FROM_HERE) {}

  BlockingCompletionHandler(const BlockingCompletionHandler&) = delete;
  BlockingCompletionHandler& operator=(const BlockingCompletionHandler&) =
      delete;

  ~BlockingCompletionHandler() override = default;

  void Initialize(scoped_refptr<base::SingleThreadTaskRunner> io_task_runner) {
    const std::wstring pipe_name = base::ASCIIToWide(
        base::StrCat({"\\\\.\\pipe\\remoting.iocp_drainer_test.",
                      base::NumberToString(::GetCurrentProcessId()), ".",
                      base::UnguessableToken::Create().ToString()}));
    constexpr DWORD kOpenMode = PIPE_ACCESS_OUTBOUND | FILE_FLAG_OVERLAPPED |
                                FILE_FLAG_FIRST_PIPE_INSTANCE;
    constexpr DWORD kPipeMode =
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_REJECT_REMOTE_CLIENTS;
    write_handle_.Set(::CreateNamedPipeW(
        pipe_name.c_str(), kOpenMode, kPipeMode, /*nMaxInstances=*/1,
        /*nOutBufferSize=*/64, /*nInBufferSize=*/64, /*nDefaultTimeOut=*/0,
        /*lpSecurityAttributes=*/nullptr));
    ASSERT_TRUE(write_handle_.is_valid());

    constexpr DWORD kClientFlags =
        SECURITY_SQOS_PRESENT | SECURITY_ANONYMOUS | FILE_FLAG_OVERLAPPED;
    read_handle_.Set(::CreateFileW(
        pipe_name.c_str(), GENERIC_READ, /*dwShareMode=*/0,
        /*lpSecurityAttributes=*/nullptr, OPEN_EXISTING, kClientFlags,
        /*hTemplateFile=*/nullptr));
    ASSERT_TRUE(read_handle_.is_valid());
    ASSERT_FALSE(::ConnectNamedPipe(write_handle_.Get(), nullptr));
    ASSERT_EQ(::GetLastError(), static_cast<DWORD>(ERROR_PIPE_CONNECTED));

    bool registered = false;
    base::RunLoop run_loop;
    io_task_runner->PostTaskAndReply(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          registered = base::CurrentIOThread::Get()->RegisterIOHandler(
              write_handle_.Get(), this);
        }),
        run_loop.QuitClosure());
    run_loop.Run();
    ASSERT_TRUE(registered);
  }

  void QueuePacket() {
    ASSERT_FALSE(packet_queued_);
    packet_queued_ = true;
    const BOOL ok = ::WriteFile(write_handle_.Get(), &byte_, sizeof(byte_),
                                nullptr, io_context_.GetOverlapped());
    if (!ok && ::GetLastError() != ERROR_IO_PENDING) {
      packet_queued_ = false;
      FAIL() << "WriteFile failed: " << ::GetLastError();
    }
  }

  void WaitUntilEntered() {
    base::ScopedAllowBaseSyncPrimitivesForTesting allow_wait;
    ASSERT_TRUE(entered_event_.TimedWait(TestTimeouts::action_timeout()));
  }

  void Release() { release_event_.Signal(); }

  bool completed() const { return completed_.load(); }

  // `base::MessagePumpForIO::IOHandler` implementation.
  void OnIOCompleted(base::MessagePumpForIO::IOContext* context,
                     DWORD bytes_transferred,
                     DWORD error) override {
    entered_event_.Signal();
    base::ScopedAllowBaseSyncPrimitivesForTesting allow_wait;
    CHECK(release_event_.TimedWait(TestTimeouts::action_timeout()));
    completed_.store(true);
  }

 private:
  base::win::ScopedHandle write_handle_;
  base::win::ScopedHandle read_handle_;
  base::MessagePumpForIO::IOContext io_context_;
  uint8_t byte_ = 1;
  bool packet_queued_ = false;
  std::atomic<bool> completed_ = false;
  base::WaitableEvent entered_event_;
  base::WaitableEvent release_event_;
};

}  // namespace

class IoCompletionPortDrainerTest : public testing::Test {
 public:
  IoCompletionPortDrainerTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI),
        io_thread_("IoCompletionPortDrainerTest IO") {}

  void SetUp() override {
    base::Thread::Options options;
    options.message_pump_type = base::MessagePumpType::IO;
    ASSERT_TRUE(io_thread_.StartWithOptions(std::move(options)));
  }

  void TearDown() override {
    for (const auto& handler : blocking_handlers_) {
      handler->Release();
    }
    io_thread_.Stop();
    blocking_handlers_.clear();
  }

 protected:
  BlockingCompletionHandler* CreateBlockingCompletionHandler() {
    auto handler = std::make_unique<BlockingCompletionHandler>();
    handler->Initialize(io_thread_.task_runner());
    if (HasFatalFailure()) {
      return nullptr;
    }
    BlockingCompletionHandler* raw_handler = handler.get();
    blocking_handlers_.push_back(std::move(handler));
    return raw_handler;
  }

  void FlushCallerThread() {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  base::test::TaskEnvironment task_environment_;
  base::Thread io_thread_;
  std::vector<std::unique_ptr<BlockingCompletionHandler>> blocking_handlers_;
};

TEST_F(IoCompletionPortDrainerTest, DrainsEmptyCompletionPort) {
  std::unique_ptr<IoCompletionPortDrainer> drainer =
      IoCompletionPortDrainer::Create();
  ASSERT_TRUE(drainer);

  base::RunLoop register_loop;
  bool registered = false;
  io_thread_.task_runner()->PostTaskAndReply(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        registered = drainer->RegisterOnIoThread();
      }),
      register_loop.QuitClosure());
  register_loop.Run();
  ASSERT_TRUE(registered);

  scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner =
      base::SingleThreadTaskRunner::GetCurrentDefault();
  base::RunLoop drained_loop;
  bool drained = false;
  io_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        // Destroy `drainer` synchronously inside `on_drained` on the I/O thread
        // to verify `OnIOCompleted()` safely tolerates self-destruction.
        IoCompletionPortDrainer* raw_drainer = drainer.get();
        raw_drainer->Drain(base::BindOnce(
            [](std::unique_ptr<IoCompletionPortDrainer> owned_drainer,
               base::OnceClosure notify_caller) {
              owned_drainer.reset();
              std::move(notify_caller).Run();
            },
            std::move(drainer),
            base::BindPostTask(caller_task_runner,
                               base::BindLambdaForTesting([&]() {
                                 drained = true;
                                 drained_loop.Quit();
                               }))));
      }));

  drained_loop.Run();
  EXPECT_TRUE(drained);
}

TEST_F(IoCompletionPortDrainerTest,
       DrainWaitsBehindQueuedCompletionPacketsUnlikePostTask) {
  std::unique_ptr<IoCompletionPortDrainer> drainer =
      IoCompletionPortDrainer::Create();
  ASSERT_TRUE(drainer);

  base::RunLoop register_loop;
  bool registered = false;
  io_thread_.task_runner()->PostTaskAndReply(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        registered = drainer->RegisterOnIoThread();
      }),
      register_loop.QuitClosure());
  register_loop.Run();
  ASSERT_TRUE(registered);

  BlockingCompletionHandler* handler_1 = CreateBlockingCompletionHandler();
  BlockingCompletionHandler* handler_2 = CreateBlockingCompletionHandler();
  BlockingCompletionHandler* handler_3 = CreateBlockingCompletionHandler();
  BlockingCompletionHandler* handler_4 = CreateBlockingCompletionHandler();
  ASSERT_TRUE(handler_1);
  ASSERT_TRUE(handler_2);
  ASSERT_TRUE(handler_3);
  ASSERT_TRUE(handler_4);

  // Block the I/O thread inside `handler_1` while queueing `handler_2`,
  // `handler_3`, and `handler_4` behind it on the completion port.
  ASSERT_NO_FATAL_FAILURE(handler_1->QueuePacket());
  ASSERT_NO_FATAL_FAILURE(handler_1->WaitUntilEntered());
  ASSERT_NO_FATAL_FAILURE(handler_2->QueuePacket());
  ASSERT_NO_FATAL_FAILURE(handler_3->QueuePacket());
  ASSERT_NO_FATAL_FAILURE(handler_4->QueuePacket());

  scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner =
      base::SingleThreadTaskRunner::GetCurrentDefault();
  bool regular_task_ran = false;
  bool drainer_completed = false;
  base::RunLoop drained_loop;

  // Post `Drain()` first and a regular task second. `MessagePumpForIO` runs one
  // task and then one completion packet per loop iteration, so after
  // `handler_1`, `handler_2`, and `handler_3` complete:
  //   iter 1: `Drain()` runs, then `handler_2`
  //   iter 2: `regular_task` runs, then `handler_3`
  //   iter 3: any task posted by `Drain()` would run BEFORE `handler_4`
  // A task-based `Drain()` would therefore complete while `handler_4` is still
  // blocked, whereas the sentinel packet stays behind `handler_4` in the IOCP.
  io_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        drainer->Drain(base::BindPostTask(caller_task_runner,
                                          base::BindLambdaForTesting([&]() {
                                            EXPECT_TRUE(handler_1->completed());
                                            EXPECT_TRUE(handler_2->completed());
                                            EXPECT_TRUE(handler_3->completed());
                                            EXPECT_TRUE(handler_4->completed());
                                            drainer_completed = true;
                                            drained_loop.Quit();
                                          })));
      }));
  io_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindPostTask(caller_task_runner, base::BindLambdaForTesting([&]() {
                           regular_task_ran = true;
                         })));

  // Release `handler_1`, `handler_2`, and `handler_3` and pause inside
  // `handler_4`. `regular_task` has already overtaken `handler_3`/`handler_4`,
  // while `drainer` must still wait behind `handler_4`.
  handler_1->Release();
  ASSERT_NO_FATAL_FAILURE(handler_2->WaitUntilEntered());
  handler_2->Release();
  ASSERT_NO_FATAL_FAILURE(handler_3->WaitUntilEntered());
  handler_3->Release();
  ASSERT_NO_FATAL_FAILURE(handler_4->WaitUntilEntered());
  FlushCallerThread();
  EXPECT_TRUE(regular_task_ran);
  EXPECT_FALSE(drainer_completed);

  // Release `handler_4`; `drainer` completes strictly after `handler_4`.
  handler_4->Release();
  drained_loop.Run();
  EXPECT_TRUE(drainer_completed);
}

TEST_F(IoCompletionPortDrainerTest, DestroyAfterRegisterWithoutDrainCleansUp) {
  std::unique_ptr<IoCompletionPortDrainer> drainer =
      IoCompletionPortDrainer::Create();
  ASSERT_TRUE(drainer);

  base::RunLoop register_loop;
  bool registered = false;
  io_thread_.task_runner()->PostTaskAndReply(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        registered = drainer->RegisterOnIoThread();
      }),
      register_loop.QuitClosure());
  register_loop.Run();
  ASSERT_TRUE(registered);

  drainer.reset();
}

}  // namespace remoting
