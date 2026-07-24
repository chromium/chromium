// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/wts_session_process_delegate.h"

#include <windows.h>

#include <memory>
#include <tuple>

#include "base/command_line.h"
#include "base/functional/callback_helpers.h"
#include "base/message_loop/message_pump_type.h"
#include "base/process/process.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/multiprocess_test.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread.h"
#include "base/win/scoped_handle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

namespace remoting {

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

  void TearDown() override { io_thread_.Stop(); }

 protected:
  void FlushIoThread() {
    base::RunLoop run_loop;
    io_thread_.task_runner()->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                               run_loop.QuitClosure());
    run_loop.Run();
  }

  base::test::TaskEnvironment task_environment_;
  base::Thread io_thread_;
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
  ASSERT_TRUE(delegate->AssignProcessToJobForTesting(child.Handle()));

  bool core_deleted = false;
  base::RunLoop run_loop;
  delegate->SetCoreDeletedCallbackForTesting(base::BindLambdaForTesting([&]() {
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
  ASSERT_TRUE(delegate->AssignProcessToJobForTesting(child1.Handle()));

  base::Process child2 = base::SpawnMultiProcessTestChild(
      "WtsSessionProcessDelegateTestChild",
      base::GetMultiProcessTestChildBaseCommandLine(), {});
  ASSERT_TRUE(child2.IsValid());
  ASSERT_TRUE(delegate->AssignProcessToJobForTesting(child2.Handle()));

  // Terminate child1 so a JOB_OBJECT_MSG_EXIT_PROCESS / ACTIVE_PROCESS_ZERO is
  // generated, but child2 remains active in the job object.
  child1.Terminate(0, false);
  FlushIoThread();

  bool core_deleted = false;
  base::RunLoop run_loop;
  delegate->SetCoreDeletedCallbackForTesting(base::BindLambdaForTesting([&]() {
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

MULTIPROCESS_TEST_MAIN(WtsSessionProcessDelegateTestChild) {
  // Block until the parent terminates this process via the job object.
  ::Sleep(INFINITE);
  return 0;
}

}  // namespace remoting
