// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/wts_session_process_delegate.h"

#include <windows.h>

#include <stdint.h>

#include <atomic>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/message_loop/message_pump_for_io.h"
#include "base/message_loop/message_pump_type.h"
#include "base/process/process.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/bind_post_task.h"
#include "base/task/current_thread.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/multiprocess_test.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "base/win/scoped_handle.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "remoting/host/win/io_completion_port_drainer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

namespace remoting {

namespace {

// Overlapped pipe completion handler registered on the I/O thread's completion
// port that pauses the I/O thread inside `OnIOCompleted()` until released by
// the test, allowing deterministic ordering of completion-port packets.
class BlockingCompletionHandler : public base::MessagePumpForIO::IOHandler {
 public:
  BlockingCompletionHandler() : base::MessagePumpForIO::IOHandler(FROM_HERE) {}

  BlockingCompletionHandler(const BlockingCompletionHandler&) = delete;
  BlockingCompletionHandler& operator=(const BlockingCompletionHandler&) =
      delete;

  ~BlockingCompletionHandler() override = default;

  void Initialize(scoped_refptr<base::SingleThreadTaskRunner> io_task_runner) {
    const std::wstring pipe_name = base::ASCIIToWide(
        base::StrCat({"\\\\.\\pipe\\remoting.wts_delegate_test.",
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

  // `base::MessagePumpForIO::IOHandler` implementation.
  void OnIOCompleted(base::MessagePumpForIO::IOContext* context,
                     DWORD bytes_transferred,
                     DWORD error) override {
    entered_event_.Signal();
    base::ScopedAllowBaseSyncPrimitivesForTesting allow_wait;
    CHECK(release_event_.TimedWait(TestTimeouts::action_timeout()));
  }

 private:
  base::win::ScopedHandle write_handle_;
  base::win::ScopedHandle read_handle_;
  base::MessagePumpForIO::IOContext io_context_;
  uint8_t byte_ = 1;
  bool packet_queued_ = false;
  base::WaitableEvent entered_event_;
  base::WaitableEvent release_event_;
};

// Test double for `IoCompletionPortDrainer` that holds `on_drained` until
// explicitly completed by the test, allowing deterministic unit testing of
// `WtsSessionProcessDelegate`'s drain state machine without OS pipes or
// multiple child processes.
class FakeCompletionPortDrainer : public IoCompletionPortDrainer {
 public:
  explicit FakeCompletionPortDrainer(bool register_result = true)
      : register_result_(register_result) {}

  ~FakeCompletionPortDrainer() override { CHECK(!on_drained_); }

  bool RegisterOnIoThread() override {
    EXPECT_FALSE(register_called_.exchange(true));
    return register_result_;
  }

  void SetOnDrainStartedCallback(base::OnceClosure callback) {
    on_drain_started_ = std::move(callback);
  }

  void Drain(base::OnceClosure on_drained) override {
    EXPECT_TRUE(register_called_.load());
    EXPECT_FALSE(drain_called_.exchange(true));
    on_drained_ = std::move(on_drained);
    drain_started_event_.Signal();
    if (on_drain_started_) {
      std::move(on_drain_started_).Run();
    }
  }

  void WaitUntilDrainStarted() {
    base::ScopedAllowBaseSyncPrimitivesForTesting allow_wait;
    ASSERT_TRUE(drain_started_event_.TimedWait(TestTimeouts::action_timeout()));
  }

  void CompleteDrainOnIoThread(
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner) {
    io_task_runner->PostTask(
        FROM_HERE, base::BindOnce(&FakeCompletionPortDrainer::RunDrainCallback,
                                  base::Unretained(this)));
  }

  bool register_called() const { return register_called_.load(); }
  bool drain_called() const { return drain_called_.load(); }

 private:
  void RunDrainCallback() {
    CHECK(on_drained_);
    std::move(on_drained_).Run();
  }

  const bool register_result_;
  std::atomic<bool> register_called_ = false;
  std::atomic<bool> drain_called_ = false;
  base::OnceClosure on_drained_;
  base::OnceClosure on_drain_started_;
  base::WaitableEvent drain_started_event_;
};

// Assigns `process` to an auxiliary job object after it has already been
// assigned to the delegate's job object, so `WaitUntilProcessExitedJob()` can
// confirm kernel job teardown has completed for `process`.
base::win::ScopedHandle CreateObserverJobForProcess(
    base::ProcessHandle process) {
  base::win::ScopedHandle observer_job(::CreateJobObject(nullptr, nullptr));
  if (!observer_job.is_valid() ||
      !::AssignProcessToJobObject(observer_job.Get(), process)) {
    return {};
  }
  return observer_job;
}

void WaitUntilProcessExitedJob(base::Process& process,
                               const base::win::ScopedHandle& observer_job) {
  ASSERT_TRUE(observer_job.is_valid());
  int exit_code = 0;
  ASSERT_TRUE(process.WaitForExitWithTimeout(TestTimeouts::action_timeout(),
                                             &exit_code));
  // Sleep rather than pumping the caller thread: `observer_job` is not bound to
  // a completion port, and callers rely on `caller_task_runner_` staying idle
  // while `io_thread_` is paused inside a blocking completion handler.
  const base::TimeTicks deadline =
      base::TimeTicks::Now() + TestTimeouts::action_timeout();
  while (base::TimeTicks::Now() < deadline) {
    JOBOBJECT_BASIC_ACCOUNTING_INFORMATION info = {};
    ASSERT_TRUE(::QueryInformationJobObject(observer_job.Get(),
                                            JobObjectBasicAccountingInformation,
                                            &info, sizeof(info), nullptr));
    if (info.ActiveProcesses == 0) {
      return;
    }
    base::PlatformThread::Sleep(base::Milliseconds(1));
  }
  FAIL() << "Timed out waiting for process to complete kernel job teardown.";
}

}  // namespace

class WtsSessionProcessDelegateTest : public testing::Test {
 public:
  WtsSessionProcessDelegateTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI),
        io_thread_("WtsSessionProcessDelegateTest IO") {}

  void SetUp() override {
    base::Thread::Options options;
    options.message_pump_type = base::MessagePumpType::IO;
    ASSERT_TRUE(io_thread_.StartWithOptions(std::move(options)));
  }

  void TearDown() override {
    // Unblock any stalled completion handlers and join `io_thread_` before
    // destroying `blocking_handlers_` so completion keys remain valid even if a
    // test exits early on an assertion failure.
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

  void SimulateRawJobNotificationOnIoThread(
      base::MessagePumpForIO::IOHandler* job_io_handler,
      DWORD event,
      base::ProcessId pid) {
    base::RunLoop run_loop;
    io_thread_.task_runner()->PostTaskAndReply(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          job_io_handler->OnIOCompleted(
              reinterpret_cast<base::MessagePumpForIO::IOContext*>(
                  static_cast<uintptr_t>(pid)),
              event, 0);
        }),
        run_loop.QuitClosure());
    run_loop.Run();
  }

  void FlushIoThread() {
    base::RunLoop run_loop;
    io_thread_.task_runner()->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                               run_loop.QuitClosure());
    run_loop.Run();
  }

  void FlushCallerThread() {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  void ResetDelegateAndWaitForCoreDeletion(
      std::unique_ptr<WtsSessionProcessDelegate> delegate) {
    bool core_deleted = false;
    base::RunLoop deleted_loop;
    WtsSessionProcessDelegate::TestApi(delegate.get())
        .SetCoreDeletedCallback(base::BindLambdaForTesting([&]() {
          core_deleted = true;
          deleted_loop.Quit();
        }));
    delegate.reset();
    if (!core_deleted) {
      deleted_loop.Run();
    }
    EXPECT_TRUE(core_deleted);
  }

  base::test::TaskEnvironment task_environment_;
  base::Thread io_thread_;
  std::vector<std::unique_ptr<BlockingCompletionHandler>> blocking_handlers_;
};

// Verifies that the delegate's internal Core stays alive while a worker process
// is still assigned to the job object, and is only released once the I/O thread
// has reported that the job has no remaining active processes.
TEST_F(WtsSessionProcessDelegateTest, CoreOutlivesJobNotifications) {
  auto target_command =
      std::make_unique<base::CommandLine>(base::CommandLine::NO_PROGRAM);
  auto delegate = std::make_unique<WtsSessionProcessDelegate>(
      io_thread_.task_runner(), std::move(target_command),
      /*launch_elevated=*/true,
      /*channel_security=*/std::string());

  // Initialize() creates the job object and registers it with the I/O thread's
  // completion port, but is expected to fail to create a session token when the
  // test is not running with the required privileges. The job-object plumbing
  // is set up regardless of the return value, which is what this test relies
  // on, so the result is intentionally ignored.
  std::ignore = delegate->Initialize(WTSGetActiveConsoleSessionId());

  // Allow the asynchronous job-object initialization to complete.
  FlushIoThread();

  // Launch a child process and assign it to the delegate's job object so that
  // the job will post completion-port notifications when the child exits.
  base::Process child = base::SpawnMultiProcessTestChild(
      "WtsSessionProcessDelegateTestChild",
      base::GetMultiProcessTestChildBaseCommandLine(), {});
  ASSERT_TRUE(child.IsValid());
  ASSERT_TRUE(WtsSessionProcessDelegate::TestApi(delegate.get())
                  .AssignProcessToJob(child.Handle()));

  bool core_deleted = false;
  base::RunLoop run_loop;
  WtsSessionProcessDelegate::TestApi(delegate.get())
      .SetCoreDeletedCallback(base::BindLambdaForTesting([&]() {
        core_deleted = true;
        run_loop.Quit();
      }));

  // Destroying the delegate calls Stop(), which terminates the job. The Core
  // must remain alive until the I/O thread has delivered the final job
  // notifications.
  delegate.reset();
  EXPECT_FALSE(core_deleted);

  // Wait for the I/O thread to deliver all job-object notifications and for the
  // Core to be released. Any premature release of Core would result in
  // OnIOCompleted() being invoked on a freed object.
  run_loop.Run();
  EXPECT_TRUE(core_deleted);

  // The job object terminates the child as part of Stop().
  int exit_code = 0;
  EXPECT_TRUE(
      child.WaitForExitWithTimeout(TestTimeouts::action_timeout(), &exit_code));
}

TEST_F(WtsSessionProcessDelegateTest,
       StaleJobNotificationIgnoredIfNewProcessAssigned) {
  auto target_command =
      std::make_unique<base::CommandLine>(base::CommandLine::NO_PROGRAM);
  auto delegate = std::make_unique<WtsSessionProcessDelegate>(
      io_thread_.task_runner(), std::move(target_command),
      /*launch_elevated=*/true,
      /*channel_security=*/std::string());

  std::ignore = delegate->Initialize(WTSGetActiveConsoleSessionId());
  FlushIoThread();

  base::Process child1 = base::SpawnMultiProcessTestChild(
      "WtsSessionProcessDelegateTestChild",
      base::GetMultiProcessTestChildBaseCommandLine(), {});
  ASSERT_TRUE(child1.IsValid());
  ASSERT_TRUE(WtsSessionProcessDelegate::TestApi(delegate.get())
                  .AssignProcessToJob(child1.Handle()));

  base::Process child2 = base::SpawnMultiProcessTestChild(
      "WtsSessionProcessDelegateTestChild",
      base::GetMultiProcessTestChildBaseCommandLine(), {});
  ASSERT_TRUE(child2.IsValid());
  ASSERT_TRUE(WtsSessionProcessDelegate::TestApi(delegate.get())
                  .AssignProcessToJob(child2.Handle()));

  // Terminate child1 so a JOB_OBJECT_MSG_EXIT_PROCESS / ACTIVE_PROCESS_ZERO is
  // generated, but child2 remains active in the job object.
  child1.Terminate(0, false);
  FlushIoThread();

  bool core_deleted = false;
  base::RunLoop run_loop;
  WtsSessionProcessDelegate::TestApi(delegate.get())
      .SetCoreDeletedCallback(base::BindLambdaForTesting([&]() {
        core_deleted = true;
        run_loop.Quit();
      }));

  // Destroying delegate stops the job. Since child2 is still in the job, Core
  // must stay alive until child2 is terminated.
  delegate.reset();
  EXPECT_FALSE(core_deleted);

  run_loop.Run();
  EXPECT_TRUE(core_deleted);

  int exit_code = 0;
  EXPECT_TRUE(child2.WaitForExitWithTimeout(TestTimeouts::action_timeout(),
                                            &exit_code));
}

TEST_F(WtsSessionProcessDelegateTest, AbnormalWorkerExitClearsWorkerPid) {
  auto target_command =
      std::make_unique<base::CommandLine>(base::CommandLine::NO_PROGRAM);
  auto delegate = std::make_unique<WtsSessionProcessDelegate>(
      io_thread_.task_runner(), std::move(target_command),
      /*launch_elevated=*/true,
      /*channel_security=*/std::string());

  std::ignore = delegate->Initialize(WTSGetActiveConsoleSessionId());
  FlushIoThread();

  constexpr base::ProcessId kLauncherPid = 1000;
  constexpr base::ProcessId kWorkerPid = 2000;

  {
    WtsSessionProcessDelegate::TestApi test_api(delegate.get());
    test_api.SetElevatedLauncherPid(kLauncherPid);
    test_api.SetWorkerProcessPid(kWorkerPid);
    EXPECT_EQ(test_api.GetWorkerProcessPid(), kWorkerPid);

    // Simulate JOB_OBJECT_MSG_ABNORMAL_EXIT_PROCESS for the worker process.
    test_api.SimulateJobNotification(JOB_OBJECT_MSG_ABNORMAL_EXIT_PROCESS,
                                     kWorkerPid);
    FlushIoThread();

    EXPECT_EQ(test_api.GetWorkerProcessPid(), base::kNullProcessId);
  }
  ResetDelegateAndWaitForCoreDeletion(std::move(delegate));
}

TEST_F(WtsSessionProcessDelegateTest, NormalWorkerExitClearsWorkerPid) {
  auto target_command =
      std::make_unique<base::CommandLine>(base::CommandLine::NO_PROGRAM);
  auto delegate = std::make_unique<WtsSessionProcessDelegate>(
      io_thread_.task_runner(), std::move(target_command),
      /*launch_elevated=*/true,
      /*channel_security=*/std::string());

  std::ignore = delegate->Initialize(WTSGetActiveConsoleSessionId());
  FlushIoThread();

  constexpr base::ProcessId kLauncherPid = 1000;
  constexpr base::ProcessId kWorkerPid = 2000;

  {
    WtsSessionProcessDelegate::TestApi test_api(delegate.get());
    test_api.SetElevatedLauncherPid(kLauncherPid);
    test_api.SetWorkerProcessPid(kWorkerPid);
    EXPECT_EQ(test_api.GetWorkerProcessPid(), kWorkerPid);

    // Simulate JOB_OBJECT_MSG_EXIT_PROCESS for the worker process.
    test_api.SimulateJobNotification(JOB_OBJECT_MSG_EXIT_PROCESS, kWorkerPid);
    FlushIoThread();

    EXPECT_EQ(test_api.GetWorkerProcessPid(), base::kNullProcessId);
  }
  ResetDelegateAndWaitForCoreDeletion(std::move(delegate));
}

TEST_F(WtsSessionProcessDelegateTest,
       LauncherExitAfterAbnormalWorkerExitDoesNotReportStalePid) {
  auto target_command =
      std::make_unique<base::CommandLine>(base::CommandLine::NO_PROGRAM);
  auto delegate = std::make_unique<WtsSessionProcessDelegate>(
      io_thread_.task_runner(), std::move(target_command),
      /*launch_elevated=*/true,
      /*channel_security=*/std::string());

  std::ignore = delegate->Initialize(WTSGetActiveConsoleSessionId());
  FlushIoThread();

  constexpr base::ProcessId kLauncherPid = 1000;
  constexpr base::ProcessId kWorkerPid = 2000;

  {
    WtsSessionProcessDelegate::TestApi test_api(delegate.get());
    test_api.SetElevatedLauncherPid(kLauncherPid);
    test_api.SetWorkerProcessPid(kWorkerPid);

    // 1. Worker process crashes abnormally.
    test_api.SimulateJobNotification(JOB_OBJECT_MSG_ABNORMAL_EXIT_PROCESS,
                                     kWorkerPid);
    FlushIoThread();

    EXPECT_EQ(test_api.GetWorkerProcessPid(), base::kNullProcessId);

    // 2. Intermediate launcher exits without launching a new worker.
    test_api.SimulateJobNotification(JOB_OBJECT_MSG_EXIT_PROCESS, kLauncherPid);
    FlushIoThread();

    // The stale worker PID must not be retained or processed.
    EXPECT_EQ(test_api.GetWorkerProcessPid(), base::kNullProcessId);
  }
  ResetDelegateAndWaitForCoreDeletion(std::move(delegate));
}

TEST_F(WtsSessionProcessDelegateTest,
       LauncherExitWithActiveWorkerTriggersLaunchDetection) {
  auto target_command =
      std::make_unique<base::CommandLine>(base::CommandLine::NO_PROGRAM);
  auto delegate = std::make_unique<WtsSessionProcessDelegate>(
      io_thread_.task_runner(), std::move(target_command),
      /*launch_elevated=*/true,
      /*channel_security=*/std::string());

  std::ignore = delegate->Initialize(WTSGetActiveConsoleSessionId());
  FlushIoThread();

  mojo::NamedPlatformChannel::Options options;
  mojo::NamedPlatformChannel channel(options);
  base::Process child;
  {
    WtsSessionProcessDelegate::TestApi test_api(delegate.get());
    test_api.SetElevatedServerEndpoint(channel.TakeServerEndpoint());

    constexpr base::ProcessId kLauncherPid = 1000;
    test_api.SetElevatedLauncherPid(kLauncherPid);

    // Spawn a child process and assign it to the job object.
    child = base::SpawnMultiProcessTestChild(
        "WtsSessionProcessDelegateTestChild",
        base::GetMultiProcessTestChildBaseCommandLine(), {});
    ASSERT_TRUE(child.IsValid());
    ASSERT_TRUE(test_api.AssignProcessToJob(child.Handle()));
    FlushIoThread();
    EXPECT_EQ(test_api.GetWorkerProcessPid(), child.Pid());

    bool process_launched = false;
    base::RunLoop run_loop;
    test_api.SetProcessLaunchedCallback(base::BindLambdaForTesting([&]() {
      process_launched = true;
      run_loop.Quit();
    }));

    // Launcher exits after spawning worker.
    test_api.SimulateJobNotification(JOB_OBJECT_MSG_EXIT_PROCESS, kLauncherPid);

    // Wait for OnProcessLaunchDetected to run on main thread.
    run_loop.Run();
    EXPECT_TRUE(process_launched);
  }

  // Clean up child process and drain `Core` before `TearDown()` stops
  // `io_thread_`.
  child.Terminate(0, false);
  int exit_code = 0;
  EXPECT_TRUE(
      child.WaitForExitWithTimeout(TestTimeouts::action_timeout(), &exit_code));
  ResetDelegateAndWaitForCoreDeletion(std::move(delegate));
}

TEST_F(WtsSessionProcessDelegateTest, ProcessNotInJobRejectedInLaunchDetected) {
  auto target_command =
      std::make_unique<base::CommandLine>(base::CommandLine::NO_PROGRAM);
  auto delegate = std::make_unique<WtsSessionProcessDelegate>(
      io_thread_.task_runner(), std::move(target_command),
      /*launch_elevated=*/true,
      /*channel_security=*/std::string());

  std::ignore = delegate->Initialize(WTSGetActiveConsoleSessionId());
  FlushIoThread();

  mojo::NamedPlatformChannel::Options options;
  mojo::NamedPlatformChannel channel(options);
  base::Process unassigned_child;
  {
    WtsSessionProcessDelegate::TestApi test_api(delegate.get());
    test_api.SetElevatedServerEndpoint(channel.TakeServerEndpoint());

    // Spawn a child process that is NOT assigned to the job object.
    unassigned_child = base::SpawnMultiProcessTestChild(
        "WtsSessionProcessDelegateTestChild",
        base::GetMultiProcessTestChildBaseCommandLine(), {});
    ASSERT_TRUE(unassigned_child.IsValid());

    bool fatal_error_reported = false;
    base::RunLoop run_loop;
    test_api.SetFatalErrorCallback(base::BindLambdaForTesting([&]() {
      fatal_error_reported = true;
      run_loop.Quit();
    }));

    // Trigger OnProcessLaunchDetected with the unassigned child's PID.
    test_api.OnProcessLaunchDetected(unassigned_child.Pid());

    // Wait for ReportFatalError to be called.
    run_loop.Run();
    EXPECT_TRUE(fatal_error_reported);
  }

  // Clean up child process and drain `Core` before `TearDown()` stops
  // `io_thread_`.
  unassigned_child.Terminate(0, false);
  int exit_code = 0;
  EXPECT_TRUE(unassigned_child.WaitForExitWithTimeout(
      TestTimeouts::action_timeout(), &exit_code));
  ResetDelegateAndWaitForCoreDeletion(std::move(delegate));
}

// Verifies in isolation using `FakeCompletionPortDrainer` that `Core` stays
// alive while `IoCompletionPortDrainer` has an in-flight drain even when
// multiple stale `JOB_OBJECT_MSG_ACTIVE_PROCESS_ZERO` and
// `JOB_OBJECT_MSG_EXIT_PROCESS` notifications arrive after `job_` is closed,
// and is destroyed once the drainer completes.
TEST_F(WtsSessionProcessDelegateTest,
       DrainHoldsCoreAliveAcrossStaleNotificationsUntilDrainerCompletes) {
  auto target_command =
      std::make_unique<base::CommandLine>(base::CommandLine::NO_PROGRAM);
  auto delegate = std::make_unique<WtsSessionProcessDelegate>(
      io_thread_.task_runner(), std::move(target_command),
      /*launch_elevated=*/true,
      /*channel_security=*/std::string());

  bool core_deleted = false;
  base::RunLoop deleted_loop;
  auto drainer = std::make_unique<FakeCompletionPortDrainer>();
  FakeCompletionPortDrainer* fake_drainer = drainer.get();
  bool drain_completed = false;
  base::ScopedClosureRunner cleanup_drain(base::BindLambdaForTesting([&]() {
    if (!drain_completed && fake_drainer && fake_drainer->drain_called()) {
      FakeCompletionPortDrainer* drainer_to_complete =
          std::exchange(fake_drainer, nullptr);
      drainer_to_complete->CompleteDrainOnIoThread(io_thread_.task_runner());
      deleted_loop.Run();
    }
  }));

  base::MessagePumpForIO::IOHandler* job_io_handler = nullptr;
  {
    WtsSessionProcessDelegate::TestApi test_api(delegate.get());
    test_api.SetCompletionPortDrainer(std::move(drainer));
    test_api.SetCoreDeletedCallback(base::BindLambdaForTesting([&]() {
      core_deleted = true;
      deleted_loop.Quit();
    }));
    job_io_handler = test_api.GetJobIoHandler();
  }

  std::ignore = delegate->Initialize(WTSGetActiveConsoleSessionId());
  FlushIoThread();
  EXPECT_TRUE(fake_drainer->register_called());
  EXPECT_FALSE(fake_drainer->drain_called());

  // Destroying `delegate` calls `Stop()`, closes `job_`, and starts draining
  // via `fake_drainer`.
  delegate.reset();
  ASSERT_NO_FATAL_FAILURE(fake_drainer->WaitUntilDrainStarted());
  FlushCallerThread();
  EXPECT_FALSE(core_deleted);

  // Dispatch multiple raw job notifications directly through the completion-key
  // `IOHandler*` (without holding a `scoped_refptr<Core>`) while `fake_drainer`
  // keeps the drain open.
  SimulateRawJobNotificationOnIoThread(
      job_io_handler, JOB_OBJECT_MSG_EXIT_PROCESS, /*pid=*/1001);
  SimulateRawJobNotificationOnIoThread(
      job_io_handler, JOB_OBJECT_MSG_ACTIVE_PROCESS_ZERO, /*pid=*/0);
  SimulateRawJobNotificationOnIoThread(
      job_io_handler, JOB_OBJECT_MSG_ACTIVE_PROCESS_ZERO, /*pid=*/0);
  FlushCallerThread();
  EXPECT_FALSE(core_deleted);

  // Completing the drain releases the final `Core` reference on the caller
  // thread.
  job_io_handler = nullptr;
  drain_completed = true;
  FakeCompletionPortDrainer* drainer_to_complete =
      std::exchange(fake_drainer, nullptr);
  drainer_to_complete->CompleteDrainOnIoThread(io_thread_.task_runner());
  deleted_loop.Run();
  EXPECT_TRUE(core_deleted);
}

// Verifies that if `IoCompletionPortDrainer::RegisterOnIoThread()` fails, job
// registration is aborted without setting `self_` or leaking `Core`.
TEST_F(WtsSessionProcessDelegateTest,
       DrainerRegistrationFailureDoesNotLeakCore) {
  auto target_command =
      std::make_unique<base::CommandLine>(base::CommandLine::NO_PROGRAM);
  auto delegate = std::make_unique<WtsSessionProcessDelegate>(
      io_thread_.task_runner(), std::move(target_command),
      /*launch_elevated=*/true,
      /*channel_security=*/std::string());

  bool core_deleted = false;
  {
    WtsSessionProcessDelegate::TestApi test_api(delegate.get());
    test_api.SetCompletionPortDrainer(
        std::make_unique<FakeCompletionPortDrainer>(/*register_result=*/false));
    test_api.SetCoreDeletedCallback(
        base::BindLambdaForTesting([&]() { core_deleted = true; }));
  }

  std::ignore = delegate->Initialize(WTSGetActiveConsoleSessionId());
  FlushIoThread();
  FlushCallerThread();

  delegate.reset();
  EXPECT_TRUE(core_deleted);
}

// Verifies that a pending launch receives `ReportFatalError()` when
// `IoCompletionPortDrainer::RegisterOnIoThread()` fails instead of stalling
// indefinitely.
TEST_F(WtsSessionProcessDelegateTest,
       DrainerRegistrationFailureReportsFatalErrorForPendingLaunch) {
  auto target_command =
      std::make_unique<base::CommandLine>(base::CommandLine::NO_PROGRAM);
  auto delegate = std::make_unique<WtsSessionProcessDelegate>(
      io_thread_.task_runner(), std::move(target_command),
      /*launch_elevated=*/true,
      /*channel_security=*/std::string());

  bool fatal_error_reported = false;
  base::RunLoop fatal_error_loop;
  {
    WtsSessionProcessDelegate::TestApi test_api(delegate.get());
    test_api.SetCompletionPortDrainer(
        std::make_unique<FakeCompletionPortDrainer>(/*register_result=*/false));
    test_api.SetFatalErrorCallback(base::BindLambdaForTesting([&]() {
      fatal_error_reported = true;
      fatal_error_loop.Quit();
    }));
  }

  std::ignore = delegate->Initialize(WTSGetActiveConsoleSessionId());
  delegate->LaunchProcess(/*event_handler=*/nullptr);

  fatal_error_loop.Run();
  EXPECT_TRUE(fatal_error_reported);

  delegate.reset();
}

// Verifies that launching after `InitializeJob()` has already failed reports a
// fatal error immediately instead of stalling.
TEST_F(WtsSessionProcessDelegateTest,
       LaunchAfterDrainerRegistrationFailureReportsFatalError) {
  auto target_command =
      std::make_unique<base::CommandLine>(base::CommandLine::NO_PROGRAM);
  auto delegate = std::make_unique<WtsSessionProcessDelegate>(
      io_thread_.task_runner(), std::move(target_command),
      /*launch_elevated=*/true,
      /*channel_security=*/std::string());

  bool fatal_error_reported = false;
  {
    WtsSessionProcessDelegate::TestApi test_api(delegate.get());
    test_api.SetCompletionPortDrainer(
        std::make_unique<FakeCompletionPortDrainer>(/*register_result=*/false));
    test_api.SetFatalErrorCallback(
        base::BindLambdaForTesting([&]() { fatal_error_reported = true; }));
  }

  std::ignore = delegate->Initialize(WTSGetActiveConsoleSessionId());
  FlushIoThread();
  FlushCallerThread();
  EXPECT_FALSE(fatal_error_reported);

  delegate->LaunchProcess(/*event_handler=*/nullptr);
  EXPECT_TRUE(fatal_error_reported);

  delegate.reset();
}

// Verifies that a non-elevated delegate (`launch_elevated=false`) creates no
// job object or drainer and destroys `Core` synchronously on `Stop()`.
TEST_F(WtsSessionProcessDelegateTest, NonElevatedStopDeletesCoreImmediately) {
  auto target_command =
      std::make_unique<base::CommandLine>(base::CommandLine::NO_PROGRAM);
  auto delegate = std::make_unique<WtsSessionProcessDelegate>(
      io_thread_.task_runner(), std::move(target_command),
      /*launch_elevated=*/false,
      /*channel_security=*/std::string());

  bool core_deleted = false;
  WtsSessionProcessDelegate::TestApi(delegate.get())
      .SetCoreDeletedCallback(
          base::BindLambdaForTesting([&]() { core_deleted = true; }));

  std::ignore = delegate->Initialize(WTSGetActiveConsoleSessionId());
  FlushIoThread();

  delegate.reset();
  EXPECT_TRUE(core_deleted);
}

// Verifies that `active_process_poll_timer_` drives job teardown and initiates
// completion-port draining when `JOB_OBJECT_MSG_ACTIVE_PROCESS_ZERO` is never
// delivered.
TEST_F(WtsSessionProcessDelegateTest,
       FallbackPollTimerDrainsWhenActiveProcessZeroIsNotDelivered) {
  int query_count = 0;
  auto target_command =
      std::make_unique<base::CommandLine>(base::CommandLine::NO_PROGRAM);
  auto delegate = std::make_unique<WtsSessionProcessDelegate>(
      io_thread_.task_runner(), std::move(target_command),
      /*launch_elevated=*/true,
      /*channel_security=*/std::string());

  bool core_deleted = false;
  base::RunLoop deleted_loop;
  base::RunLoop drain_started_loop;
  auto drainer = std::make_unique<FakeCompletionPortDrainer>();
  drainer->SetOnDrainStartedCallback(
      base::BindPostTask(base::SingleThreadTaskRunner::GetCurrentDefault(),
                         drain_started_loop.QuitClosure()));
  FakeCompletionPortDrainer* fake_drainer = drainer.get();
  bool drain_completed = false;
  base::ScopedClosureRunner cleanup_drain(base::BindLambdaForTesting([&]() {
    if (!drain_completed && fake_drainer && fake_drainer->drain_called()) {
      FakeCompletionPortDrainer* drainer_to_complete =
          std::exchange(fake_drainer, nullptr);
      drainer_to_complete->CompleteDrainOnIoThread(io_thread_.task_runner());
      deleted_loop.Run();
    }
  }));

  {
    WtsSessionProcessDelegate::TestApi test_api(delegate.get());
    test_api.SetCompletionPortDrainer(std::move(drainer));
    test_api.SetActiveProcessCountOverride(
        base::BindLambdaForTesting([&]() -> DWORD {
          // Return non-zero for `Stop()` and the first poll tick, then 0 on the
          // second poll tick.
          return ++query_count < 3 ? 1 : 0;
        }));
    test_api.SetCoreDeletedCallback(base::BindLambdaForTesting([&]() {
      core_deleted = true;
      deleted_loop.Quit();
    }));
  }

  std::ignore = delegate->Initialize(WTSGetActiveConsoleSessionId());
  FlushIoThread();
  FlushCallerThread();
  ASSERT_TRUE(fake_drainer->register_called());

  // Destroying `delegate` calls `Stop()`, observes `ActiveProcesses == 1`, and
  // arms `active_process_poll_timer_` without starting the drain yet.
  delegate.reset();
  FlushCallerThread();
  EXPECT_EQ(query_count, 1);
  EXPECT_FALSE(fake_drainer->drain_called());
  EXPECT_FALSE(core_deleted);

  // Pump the caller thread until `active_process_poll_timer_` fires twice and
  // observes `ActiveProcesses == 0`, triggering `fake_drainer->Drain()` without
  // any `JOB_OBJECT_MSG_ACTIVE_PROCESS_ZERO` notification.
  drain_started_loop.Run();
  EXPECT_EQ(query_count, 3);
  EXPECT_TRUE(fake_drainer->drain_called());
  EXPECT_FALSE(core_deleted);

  drain_completed = true;
  FakeCompletionPortDrainer* drainer_to_complete =
      std::exchange(fake_drainer, nullptr);
  drainer_to_complete->CompleteDrainOnIoThread(io_thread_.task_runner());
  deleted_loop.Run();
  EXPECT_TRUE(core_deleted);
}

// End-to-end regression test for crbug.com/587008025: after a worker relaunch
// followed by `Stop()`, a stale `JOB_OBJECT_MSG_ACTIVE_PROCESS_ZERO`
// notification from an earlier worker generation must not cause `Core` to be
// freed while job packets from the newer generation remain queued on the I/O
// completion port behind other handlers' packets.
TEST_F(WtsSessionProcessDelegateTest,
       StaleActiveProcessZeroAfterRelaunchAndStop) {
  auto target_command =
      std::make_unique<base::CommandLine>(base::CommandLine::NO_PROGRAM);
  auto delegate = std::make_unique<WtsSessionProcessDelegate>(
      io_thread_.task_runner(), std::move(target_command),
      /*launch_elevated=*/true,
      /*channel_security=*/std::string());

  std::ignore = delegate->Initialize(WTSGetActiveConsoleSessionId());
  FlushIoThread();

  BlockingCompletionHandler* handler_a = CreateBlockingCompletionHandler();
  BlockingCompletionHandler* handler_c = CreateBlockingCompletionHandler();
  BlockingCompletionHandler* handler_d = CreateBlockingCompletionHandler();
  ASSERT_TRUE(handler_a);
  ASSERT_TRUE(handler_c);
  ASSERT_TRUE(handler_d);

  // Generation 1: assign `child1` to the delegate's job object.
  base::Process child1 = base::SpawnMultiProcessTestChild(
      "WtsSessionProcessDelegateTestChild",
      base::GetMultiProcessTestChildBaseCommandLine(), {});
  ASSERT_TRUE(child1.IsValid());
  ASSERT_TRUE(WtsSessionProcessDelegate::TestApi(delegate.get())
                  .AssignProcessToJob(child1.Handle()));
  base::win::ScopedHandle observer_job1 =
      CreateObserverJobForProcess(child1.Handle());
  ASSERT_TRUE(observer_job1.is_valid());
  FlushIoThread();

  // Pause the I/O thread inside `handler_a` while we queue multi-generation job
  // packets and intermediate completion packets.
  ASSERT_NO_FATAL_FAILURE(handler_a->QueuePacket());
  ASSERT_NO_FATAL_FAILURE(handler_a->WaitUntilEntered());

  // Let generation 1 exit so its `EXIT_PROCESS` and `ACTIVE_PROCESS_ZERO`
  // packets are queued at the head of the completion port.
  child1.Terminate(0, false);
  ASSERT_NO_FATAL_FAILURE(WaitUntilProcessExitedJob(child1, observer_job1));

  // Generation 2: relaunch by assigning `child2` to the same job object, then
  // queue completion packets for `handler_c` and `handler_d` ahead of
  // `child2`'s exit notifications.
  base::Process child2 = base::SpawnMultiProcessTestChild(
      "WtsSessionProcessDelegateTestChild",
      base::GetMultiProcessTestChildBaseCommandLine(), {});
  ASSERT_TRUE(child2.IsValid());
  ASSERT_TRUE(WtsSessionProcessDelegate::TestApi(delegate.get())
                  .AssignProcessToJob(child2.Handle()));
  base::win::ScopedHandle observer_job2 =
      CreateObserverJobForProcess(child2.Handle());
  ASSERT_TRUE(observer_job2.is_valid());

  ASSERT_NO_FATAL_FAILURE(handler_c->QueuePacket());
  ASSERT_NO_FATAL_FAILURE(handler_d->QueuePacket());

  bool core_deleted = false;
  base::RunLoop deleted_loop;
  WtsSessionProcessDelegate::TestApi(delegate.get())
      .SetCoreDeletedCallback(base::BindLambdaForTesting([&]() {
        core_deleted = true;
        deleted_loop.Quit();
      }));

  // Destroy the delegate (`Stop()` -> `TerminateJobObject()`), queuing
  // `child2`'s `EXIT_PROCESS` and `ACTIVE_PROCESS_ZERO` packets behind
  // `handler_c` and `handler_d`.
  delegate.reset();
  ASSERT_NO_FATAL_FAILURE(WaitUntilProcessExitedJob(child2, observer_job2));

  // Release `handler_a`: the I/O thread dequeues generation 1's stale
  // `ACTIVE_PROCESS_ZERO` (posting `OnActiveProcessZero()`), dequeues
  // `child2`'s `NEW_PROCESS`, and blocks inside `handler_c`.
  handler_a->Release();
  ASSERT_NO_FATAL_FAILURE(handler_c->WaitUntilEntered());
  FlushCallerThread();
  EXPECT_FALSE(core_deleted);

  // Release `handler_c`: the I/O thread runs one posted task (queuing the drain
  // sentinel behind `child2`'s packets) and blocks inside `handler_d` while
  // `child2`'s `EXIT_PROCESS` and `ACTIVE_PROCESS_ZERO` are still queued.
  handler_c->Release();
  ASSERT_NO_FATAL_FAILURE(handler_d->WaitUntilEntered());
  FlushCallerThread();
  EXPECT_FALSE(core_deleted);

  // Release `handler_d`: `child2`'s remaining job notifications and the drain
  // sentinel are dequeued cleanly before `Core` is destroyed.
  handler_d->Release();
  deleted_loop.Run();
  EXPECT_TRUE(core_deleted);
}

// Verifies that destroying the delegate before `InitializeJobCompleted()` runs
// on the caller thread still drains the completion port and deletes `Core`
// cleanly.
TEST_F(WtsSessionProcessDelegateTest, StopBeforeJobInitializedDrainsCleanly) {
  auto target_command =
      std::make_unique<base::CommandLine>(base::CommandLine::NO_PROGRAM);
  auto delegate = std::make_unique<WtsSessionProcessDelegate>(
      io_thread_.task_runner(), std::move(target_command),
      /*launch_elevated=*/true,
      /*channel_security=*/std::string());

  std::ignore = delegate->Initialize(WTSGetActiveConsoleSessionId());

  bool core_deleted = false;
  base::RunLoop deleted_loop;
  WtsSessionProcessDelegate::TestApi(delegate.get())
      .SetCoreDeletedCallback(base::BindLambdaForTesting([&]() {
        core_deleted = true;
        deleted_loop.Quit();
      }));

  // Destroy immediately before pumping tasks so `Stop()` runs while
  // `InitializeJob()` / `InitializeJobCompleted()` are still in flight.
  delegate.reset();
  deleted_loop.Run();
  EXPECT_TRUE(core_deleted);
}

MULTIPROCESS_TEST_MAIN(WtsSessionProcessDelegateTestChild) {
  // Block until the parent terminates this process via the job object.
  ::Sleep(INFINITE);
  return 0;
}

}  // namespace remoting
