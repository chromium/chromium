// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/terminal_process_monitor_linux.h"

#include <memory>
#include <optional>
#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/mock_callback.h"
#include "base/test/multiprocess_test.h"
#include "base/test/task_environment.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

using ::testing::_;
using ::testing::Eq;
using ::testing::InSequence;
using ::testing::Ne;
using ::testing::StrictMock;

namespace remoting {

class TerminalProcessMonitorLinuxTest : public ::testing::Test {
 public:
  TerminalProcessMonitorLinuxTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~TerminalProcessMonitorLinuxTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(TerminalProcessMonitorLinuxTest,
       ReportsInitialStateOnStartWithInvalidPid) {
  StrictMock<
      base::MockCallback<TerminalProcessMonitorLinux::ProcessInfoCallback>>
      mock_callback;

  EXPECT_CALL(mock_callback,
              Run(/*is_active=*/false, /*process_name=*/Eq(std::nullopt)))
      .Times(1);

  auto monitor = std::make_unique<TerminalProcessMonitorLinux>(
      /*shell_pid=*/0, mock_callback.Get());

  monitor->StartPolling();
  task_environment_.FastForwardBy(base::Milliseconds(250));
  monitor->StopPolling();
}

TEST_F(TerminalProcessMonitorLinuxTest, ReportsCurrentProcessState) {
  StrictMock<
      base::MockCallback<TerminalProcessMonitorLinux::ProcessInfoCallback>>
      mock_callback;

  EXPECT_CALL(mock_callback,
              Run(/*is_active=*/_, /*process_name=*/Ne(std::nullopt)))
      .Times(1);

  auto monitor = std::make_unique<TerminalProcessMonitorLinux>(
      base::GetCurrentProcId(), mock_callback.Get());

  monitor->StartPolling();
  task_environment_.FastForwardBy(base::Milliseconds(250));
  monitor->StopPolling();
}

TEST_F(TerminalProcessMonitorLinuxTest, DeduplicatesUnchangedPollingResults) {
  StrictMock<
      base::MockCallback<TerminalProcessMonitorLinux::ProcessInfoCallback>>
      mock_callback;

  // Despite 10 polling intervals (2 seconds), callback must be invoked only
  // once.
  EXPECT_CALL(mock_callback, Run(/*is_active=*/false, /*process_name=*/_))
      .Times(1);

  auto monitor = std::make_unique<TerminalProcessMonitorLinux>(
      /*shell_pid=*/0, mock_callback.Get());

  monitor->StartPolling();
  task_environment_.FastForwardBy(base::Seconds(2));
  monitor->StopPolling();
}

TEST_F(TerminalProcessMonitorLinuxTest, StopsPollingOnStop) {
  StrictMock<
      base::MockCallback<TerminalProcessMonitorLinux::ProcessInfoCallback>>
      mock_callback;

  EXPECT_CALL(mock_callback, Run(/*is_active=*/false, /*process_name=*/_))
      .Times(1);

  auto monitor = std::make_unique<TerminalProcessMonitorLinux>(
      /*shell_pid=*/0, mock_callback.Get());

  monitor->StartPolling();
  task_environment_.FastForwardBy(base::Milliseconds(250));

  monitor->StopPolling();

  // Fast forward by 5 more seconds - no additional callbacks should fire.
  task_environment_.FastForwardBy(base::Seconds(5));
}

TEST_F(TerminalProcessMonitorLinuxTest, DestructionWhilePollingDoesNotCrash) {
  StrictMock<
      base::MockCallback<TerminalProcessMonitorLinux::ProcessInfoCallback>>
      mock_callback;

  auto monitor = std::make_unique<TerminalProcessMonitorLinux>(
      /*shell_pid=*/0, mock_callback.Get());

  monitor->StartPolling();
  // Immediately destroy monitor before background task completes.
  monitor.reset();

  // Fast forward to flush all background tasks. Must not crash or invoke
  // callback.
  task_environment_.FastForwardBy(base::Seconds(1));
}

TEST_F(TerminalProcessMonitorLinuxTest, RestartMonitorResendsInitialState) {
  StrictMock<
      base::MockCallback<TerminalProcessMonitorLinux::ProcessInfoCallback>>
      mock_callback;

  // First start receives initial report.
  EXPECT_CALL(mock_callback, Run(/*is_active=*/false, /*process_name=*/_))
      .Times(1);

  auto monitor = std::make_unique<TerminalProcessMonitorLinux>(
      /*shell_pid=*/0, mock_callback.Get());

  monitor->StartPolling();
  task_environment_.FastForwardBy(base::Milliseconds(250));
  monitor->StopPolling();

  // Second start receives initial report again.
  EXPECT_CALL(mock_callback, Run(/*is_active=*/false, /*process_name=*/_))
      .Times(1);

  monitor->StartPolling();
  task_environment_.FastForwardBy(base::Milliseconds(250));
  monitor->StopPolling();
}

TEST_F(TerminalProcessMonitorLinuxTest, SafeWithNullCallback) {
  auto monitor = std::make_unique<TerminalProcessMonitorLinux>(
      /*shell_pid=*/0, TerminalProcessMonitorLinux::ProcessInfoCallback());

  monitor->StartPolling();
  task_environment_.FastForwardBy(base::Seconds(1));
  monitor->StopPolling();
}

TEST_F(TerminalProcessMonitorLinuxTest, ReportsChildProcessLaunchAndExit) {
  InSequence s;
  base::RunLoop run_loop_initial;
  base::RunLoop run_loop_child_active;
  base::RunLoop run_loop_child_exit;

  StrictMock<
      base::MockCallback<TerminalProcessMonitorLinux::ProcessInfoCallback>>
      mock_callback;

  EXPECT_CALL(mock_callback, Run(/*is_active=*/false, /*process_name=*/_))
      .WillOnce([&run_loop_initial]() { run_loop_initial.Quit(); });
  EXPECT_CALL(mock_callback, Run(/*is_active=*/true, /*process_name=*/_))
      .WillOnce([&run_loop_child_active]() { run_loop_child_active.Quit(); });
  EXPECT_CALL(mock_callback, Run(/*is_active=*/false, /*process_name=*/_))
      .WillOnce([&run_loop_child_exit]() { run_loop_child_exit.Quit(); });

  auto monitor = std::make_unique<TerminalProcessMonitorLinux>(
      base::GetCurrentProcId(), mock_callback.Get());

  monitor->StartPolling();
  run_loop_initial.Run();

  base::Process child = base::SpawnMultiProcessTestChild(
      "TerminalChildProcess", base::GetMultiProcessTestChildBaseCommandLine(),
      base::LaunchOptions());
  ASSERT_TRUE(child.IsValid());

  task_environment_.FastForwardBy(base::Milliseconds(250));
  run_loop_child_active.Run();

  child.Terminate(0, /*wait=*/false);
  int exit_code = 0;
  EXPECT_TRUE(child.WaitForExit(&exit_code));

  task_environment_.FastForwardBy(base::Milliseconds(250));
  run_loop_child_exit.Run();
  monitor->StopPolling();
}

TEST_F(TerminalProcessMonitorLinuxTest, ReportsMultipleChildrenLaunchAndExit) {
  InSequence s;
  base::RunLoop run_loop_initial;
  base::RunLoop run_loop_child_active;
  base::RunLoop run_loop_all_children_exit;

  StrictMock<
      base::MockCallback<TerminalProcessMonitorLinux::ProcessInfoCallback>>
      mock_callback;

  EXPECT_CALL(mock_callback, Run(/*is_active=*/false, /*process_name=*/_))
      .WillOnce([&run_loop_initial]() { run_loop_initial.Quit(); });
  EXPECT_CALL(mock_callback, Run(/*is_active=*/true, /*process_name=*/_))
      .WillOnce([&run_loop_child_active]() { run_loop_child_active.Quit(); });
  EXPECT_CALL(mock_callback, Run(/*is_active=*/false, /*process_name=*/_))
      .WillOnce([&run_loop_all_children_exit]() {
        run_loop_all_children_exit.Quit();
      });

  auto monitor = std::make_unique<TerminalProcessMonitorLinux>(
      base::GetCurrentProcId(), mock_callback.Get());

  monitor->StartPolling();
  run_loop_initial.Run();

  base::Process child1 = base::SpawnMultiProcessTestChild(
      "TerminalChildProcess", base::GetMultiProcessTestChildBaseCommandLine(),
      base::LaunchOptions());
  ASSERT_TRUE(child1.IsValid());

  task_environment_.FastForwardBy(base::Milliseconds(250));
  run_loop_child_active.Run();

  base::Process child2 = base::SpawnMultiProcessTestChild(
      "TerminalChildProcess", base::GetMultiProcessTestChildBaseCommandLine(),
      base::LaunchOptions());
  ASSERT_TRUE(child2.IsValid());

  task_environment_.FastForwardBy(base::Milliseconds(250));

  // Terminate child2 first; child1 is still running so state remains active.
  child2.Terminate(0, /*wait=*/false);
  int exit_code2 = 0;
  EXPECT_TRUE(child2.WaitForExit(&exit_code2));

  task_environment_.FastForwardBy(base::Milliseconds(250));

  // Terminate child1; now all children have exited so state becomes inactive.
  child1.Terminate(0, /*wait=*/false);
  int exit_code1 = 0;
  EXPECT_TRUE(child1.WaitForExit(&exit_code1));

  task_environment_.FastForwardBy(base::Milliseconds(250));
  run_loop_all_children_exit.Run();
  monitor->StopPolling();
}

MULTIPROCESS_TEST_MAIN(TerminalChildProcess) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO);
  base::RunLoop().Run();
  return 0;
}

}  // namespace remoting
