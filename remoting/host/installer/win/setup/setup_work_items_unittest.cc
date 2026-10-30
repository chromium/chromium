// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/installer/win/setup/setup_work_items.h"

#include <windows.h>

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_reg_util_win.h"
#include "base/unguessable_token.h"
#include "base/win/scoped_handle.h"
#include "chrome/installer/util/work_item_list.h"
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

  bool InstallOrUpdateService(const base::FilePath& host_binary_path,
                              const base::FilePath& host_config_path) override {
    install_called_ = true;
    if (state_ == ServiceStateQueryResult::kNotInstalled) {
      state_ = ServiceStateQueryResult::kStopped;
    }
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
  bool start_succeeds_ = true;
  bool install_called_ = false;
  bool start_called_ = false;
  bool stop_called_ = false;
  bool uninstall_called_ = false;
};

}  // namespace

class SetupWorkItemsTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_NO_FATAL_FAILURE(
        registry_override_manager_.OverrideRegistry(HKEY_LOCAL_MACHINE));
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    source_dir_ = temp_dir_.GetPath().Append(L"source");
    install_dir_ = temp_dir_.GetPath().Append(L"install");
    config_dir_ = temp_dir_.GetPath().Append(L"config");

    ASSERT_TRUE(base::CreateDirectory(source_dir_));
    ASSERT_TRUE(base::CreateDirectory(config_dir_));

    for (const wchar_t* file_name : kPayloadFiles) {
      base::FilePath file_path = source_dir_.Append(file_name);
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
  base::FilePath source_dir_;
  base::FilePath install_dir_;
  base::FilePath config_dir_;
  FakeServiceManager fake_service_manager_;
};

TEST_F(SetupWorkItemsTest, CreateInstallWorkItemList_Success) {
  fake_service_manager_.set_state(ServiceStateQueryResult::kRunning);

  std::unique_ptr<WorkItemList> list = CreateInstallWorkItemList(
      source_dir_, install_dir_, config_dir_.Append(L"host.json"));
  ASSERT_TRUE(list);

  EXPECT_TRUE(list->Do());
  EXPECT_TRUE(fake_service_manager_.stop_called());
  EXPECT_TRUE(fake_service_manager_.install_called());
  EXPECT_TRUE(fake_service_manager_.start_called());
  EXPECT_TRUE(base::PathExists(install_dir_.Append(kHostBinaryName)));
}

TEST_F(SetupWorkItemsTest, CreateInstallWorkItemList_FailureRollback) {
  fake_service_manager_.set_start_succeeds(false);

  std::unique_ptr<WorkItemList> list = CreateInstallWorkItemList(
      source_dir_, install_dir_, config_dir_.Append(L"host.json"));
  ASSERT_TRUE(list);

  EXPECT_FALSE(list->Do());
  list->Rollback();

  EXPECT_TRUE(fake_service_manager_.uninstall_called());
  EXPECT_FALSE(base::PathExists(install_dir_.Append(kHostBinaryName)));
  EXPECT_FALSE(base::DirectoryExists(install_dir_));
}

TEST_F(SetupWorkItemsTest, CreateUpdatePendingWorkItemList_Success) {
  std::wstring event_name =
      CreateUniqueEventName(L"Local\\TestUpdatePendingPipeline");
  base::win::ScopedHandle event(::CreateEventW(nullptr, /*bManualReset=*/TRUE,
                                               /*bInitialState=*/FALSE,
                                               event_name.c_str()));
  ASSERT_TRUE(event.is_valid());

  std::unique_ptr<WorkItemList> list =
      CreateUpdatePendingWorkItemList(source_dir_, install_dir_, event_name);
  ASSERT_TRUE(list);

  EXPECT_TRUE(list->Do());
  EXPECT_EQ(::WaitForSingleObject(event.Get(), 0),
            static_cast<DWORD>(WAIT_OBJECT_0));
  EXPECT_TRUE(base::PathExists(install_dir_.Append(kHostBinaryName)));
}

TEST_F(SetupWorkItemsTest, CreateUninstallWorkItemList_Success) {
  fake_service_manager_.set_state(ServiceStateQueryResult::kRunning);

  ASSERT_TRUE(base::CreateDirectory(install_dir_));
  ASSERT_TRUE(
      base::WriteFile(install_dir_.Append(kHostBinaryName), "binary_data"));

  std::unique_ptr<WorkItemList> list =
      CreateUninstallWorkItemList(install_dir_);
  ASSERT_TRUE(list);

  EXPECT_TRUE(list->Do());
  EXPECT_TRUE(fake_service_manager_.stop_called());
  EXPECT_TRUE(fake_service_manager_.uninstall_called());
  EXPECT_FALSE(base::PathExists(install_dir_.Append(kHostBinaryName)));
}

}  // namespace remoting::installer
