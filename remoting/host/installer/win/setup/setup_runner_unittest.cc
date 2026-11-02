// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/installer/win/setup/setup_runner.h"

#include <windows.h>

#include <optional>
#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_reg_util_win.h"
#include "base/unguessable_token.h"
#include "base/win/scoped_handle.h"
#include "remoting/host/installer/win/setup/file_util.h"
#include "remoting/host/installer/win/setup/installer_constants.h"
#include "remoting/host/installer/win/setup/service_work_items.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting::installer {

namespace {

std::wstring CreateUniqueEventName(const wchar_t* prefix) {
  return base::StrCat(
      {prefix, L"_",
       base::UTF8ToWide(base::UnguessableToken::Create().ToString())});
}

class FakeServiceManager : public ServiceManager {
 public:
  ServiceStateQueryResult GetServiceState() override { return state_; }

  std::optional<std::wstring> GetServiceBinaryPathName() override {
    return binary_path_name_;
  }

  bool InstallOrUpdateService(const base::FilePath& host_binary_path,
                              const base::FilePath& host_config_path) override {
    install_called_ = true;
    if (state_ == ServiceStateQueryResult::kNotInstalled) {
      state_ = ServiceStateQueryResult::kStopped;
    }
    binary_path_name_ = host_binary_path.value();
    return true;
  }

  bool SetServiceBinaryPathName(const std::wstring& binary_path_name) override {
    binary_path_name_ = binary_path_name;
    return true;
  }

  bool StartService() override {
    if (!start_succeeds_) {
      return false;
    }
    start_called_ = true;
    state_ = ServiceStateQueryResult::kRunning;
    return true;
  }

  bool StopService() override {
    stop_called_ = true;
    state_ = ServiceStateQueryResult::kStopped;
    return true;
  }

  bool UninstallService() override {
    uninstall_called_ = true;
    state_ = ServiceStateQueryResult::kNotInstalled;
    binary_path_name_.reset();
    return true;
  }

  void set_state(ServiceStateQueryResult state) { state_ = state; }
  void set_start_succeeds(bool succeeds) { start_succeeds_ = succeeds; }

  bool install_called() const { return install_called_; }
  bool start_called() const { return start_called_; }
  bool stop_called() const { return stop_called_; }
  bool uninstall_called() const { return uninstall_called_; }

 private:
  ServiceStateQueryResult state_ = ServiceStateQueryResult::kNotInstalled;
  std::optional<std::wstring> binary_path_name_;
  bool start_succeeds_ = true;
  bool install_called_ = false;
  bool start_called_ = false;
  bool stop_called_ = false;
  bool uninstall_called_ = false;
};

}  // namespace

class SetupRunnerTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_NO_FATAL_FAILURE(
        registry_override_manager_.OverrideRegistry(HKEY_LOCAL_MACHINE));
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    config_.source_dir = temp_dir_.GetPath().Append(L"source");
    config_.install_dir = temp_dir_.GetPath().Append(L"install");
    config_.host_config_path =
        temp_dir_.GetPath().Append(L"config").Append(kHostConfigFileName);
    config_.session_active_event_name =
        CreateUniqueEventName(L"Local\\TestSetupRunnerSessionActive");
    config_.update_pending_event_name =
        CreateUniqueEventName(L"Local\\TestSetupRunnerUpdatePending");

    ASSERT_TRUE(base::CreateDirectory(config_.source_dir));
    for (const wchar_t* file_name : kPayloadFiles) {
      base::FilePath file_path = config_.source_dir.Append(file_name);
      if (!base::PathExists(file_path)) {
        ASSERT_TRUE(base::WriteFile(file_path, "data"));
      }
    }

    SetServiceManagerForTesting(&fake_service_manager_);
    SetMoveFileExCallbackForTesting(base::BindRepeating(
        [](const wchar_t* existing, const wchar_t* new_name, DWORD flags) {
          if (flags & MOVEFILE_DELAY_UNTIL_REBOOT) {
            return TRUE;
          }
          return ::MoveFileExW(existing, new_name, flags);
        }));
  }

  void TearDown() override {
    SetServiceManagerForTesting(nullptr);
    SetMoveFileExCallbackForTesting(MoveFileExCallback());
  }

  registry_util::RegistryOverrideManager registry_override_manager_;
  base::ScopedTempDir temp_dir_;
  SetupConfig config_;
  FakeServiceManager fake_service_manager_;
};

TEST_F(SetupRunnerTest, ConflictingSwitchesReturnsInvalidArguments) {
  base::CommandLine cmd_line(base::CommandLine::NO_PROGRAM);
  cmd_line.AppendSwitch(kInstallSwitch);
  cmd_line.AppendSwitch(kUninstallSwitch);

  EXPECT_EQ(RunSetupWithConfig(cmd_line, config_),
            SetupExitCode::kInvalidArguments);
}

TEST_F(SetupRunnerTest, InstallWhenHostIdleRunsFullInstallPipeline) {
  base::CommandLine cmd_line(base::CommandLine::NO_PROGRAM);
  cmd_line.AppendSwitch(kInstallSwitch);

  EXPECT_EQ(RunSetupWithConfig(cmd_line, config_), SetupExitCode::kSuccess);
  EXPECT_TRUE(fake_service_manager_.install_called());
  EXPECT_TRUE(fake_service_manager_.start_called());
  EXPECT_TRUE(base::PathExists(config_.install_dir.Append(kHostBinaryName)));
}

TEST_F(SetupRunnerTest, DefaultNoSwitchRunsInstallPipeline) {
  base::CommandLine cmd_line(base::CommandLine::NO_PROGRAM);

  EXPECT_EQ(RunSetupWithConfig(cmd_line, config_), SetupExitCode::kSuccess);
  EXPECT_TRUE(fake_service_manager_.install_called());
  EXPECT_TRUE(fake_service_manager_.start_called());
  EXPECT_TRUE(base::PathExists(config_.install_dir.Append(kHostBinaryName)));
}

TEST_F(SetupRunnerTest, UpdateWhenSessionActiveRunsUpdatePendingPipeline) {
  base::win::ScopedHandle session_event(
      ::CreateEventW(nullptr, /*bManualReset=*/TRUE, /*bInitialState=*/TRUE,
                     config_.session_active_event_name.c_str()));
  ASSERT_TRUE(session_event.is_valid());

  base::win::ScopedHandle update_pending_event(
      ::CreateEventW(nullptr, /*bManualReset=*/TRUE, /*bInitialState=*/FALSE,
                     config_.update_pending_event_name.c_str()));
  ASSERT_TRUE(update_pending_event.is_valid());

  fake_service_manager_.set_state(ServiceStateQueryResult::kRunning);

  base::CommandLine cmd_line(base::CommandLine::NO_PROGRAM);
  cmd_line.AppendSwitch(kInstallSwitch);

  EXPECT_EQ(RunSetupWithConfig(cmd_line, config_), SetupExitCode::kSuccess);
  // Active session must not be stopped; update-pending event must be signaled.
  EXPECT_FALSE(fake_service_manager_.stop_called());
  EXPECT_EQ(::WaitForSingleObject(update_pending_event.Get(), 0),
            static_cast<DWORD>(WAIT_OBJECT_0));
  EXPECT_TRUE(base::PathExists(config_.install_dir.Append(kHostBinaryName)));
}

TEST_F(SetupRunnerTest, InstallFailureRollsBackAndReturnsInstallFailed) {
  fake_service_manager_.set_start_succeeds(false);

  base::CommandLine cmd_line(base::CommandLine::NO_PROGRAM);
  cmd_line.AppendSwitch(kInstallSwitch);

  EXPECT_EQ(RunSetupWithConfig(cmd_line, config_),
            SetupExitCode::kInstallFailed);
  EXPECT_TRUE(fake_service_manager_.uninstall_called());
  EXPECT_FALSE(base::PathExists(config_.install_dir.Append(kHostBinaryName)));
}

TEST_F(SetupRunnerTest, UninstallRemovesFilesAndService) {
  base::CommandLine install_cmd(base::CommandLine::NO_PROGRAM);
  install_cmd.AppendSwitch(kInstallSwitch);
  ASSERT_EQ(RunSetupWithConfig(install_cmd, config_), SetupExitCode::kSuccess);

  base::CommandLine uninstall_cmd(base::CommandLine::NO_PROGRAM);
  uninstall_cmd.AppendSwitch(kUninstallSwitch);
  EXPECT_EQ(RunSetupWithConfig(uninstall_cmd, config_),
            SetupExitCode::kSuccess);
  EXPECT_TRUE(fake_service_manager_.uninstall_called());
  EXPECT_FALSE(base::PathExists(config_.install_dir.Append(kHostBinaryName)));
}

}  // namespace remoting::installer
