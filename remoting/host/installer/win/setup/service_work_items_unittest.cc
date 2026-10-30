// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/installer/win/setup/service_work_items.h"

#include <windows.h>

#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/unguessable_token.h"
#include "base/win/scoped_handle.h"
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
    set_binary_path_called_ = true;
    binary_path_name_ = binary_path_name;
    return true;
  }

  bool StartService() override {
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
  ServiceStateQueryResult state() const { return state_; }

  void set_binary_path_name(std::optional<std::wstring> binary_path_name) {
    binary_path_name_ = std::move(binary_path_name);
  }
  const std::optional<std::wstring>& binary_path_name() const {
    return binary_path_name_;
  }

  bool install_called() const { return install_called_; }
  bool set_binary_path_called() const { return set_binary_path_called_; }
  bool start_called() const { return start_called_; }
  bool stop_called() const { return stop_called_; }
  bool uninstall_called() const { return uninstall_called_; }

 private:
  ServiceStateQueryResult state_ = ServiceStateQueryResult::kNotInstalled;
  std::optional<std::wstring> binary_path_name_;
  bool install_called_ = false;
  bool set_binary_path_called_ = false;
  bool start_called_ = false;
  bool stop_called_ = false;
  bool uninstall_called_ = false;
};

}  // namespace

class ServiceWorkItemsTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    install_dir_ = temp_dir_.GetPath().Append(L"install");
    config_dir_ = temp_dir_.GetPath().Append(L"config");
    SetServiceManagerForTesting(&fake_service_manager_);
  }

  void TearDown() override { SetServiceManagerForTesting(nullptr); }

  base::ScopedTempDir temp_dir_;
  base::FilePath install_dir_;
  base::FilePath config_dir_;
  FakeServiceManager fake_service_manager_;
};

TEST_F(ServiceWorkItemsTest, StopServiceWorkItem_RollbackRestartsIfWasRunning) {
  fake_service_manager_.set_state(ServiceStateQueryResult::kRunning);

  StopServiceWorkItem item;
  EXPECT_TRUE(item.Do());
  EXPECT_TRUE(fake_service_manager_.stop_called());
  EXPECT_FALSE(fake_service_manager_.start_called());
  EXPECT_EQ(fake_service_manager_.state(), ServiceStateQueryResult::kStopped);

  item.Rollback();
  EXPECT_TRUE(fake_service_manager_.start_called());
  EXPECT_EQ(fake_service_manager_.state(), ServiceStateQueryResult::kRunning);
}

TEST_F(ServiceWorkItemsTest,
       StopServiceWorkItem_DoesNotRestartOnRollbackWhenAlreadyStopped) {
  fake_service_manager_.set_state(ServiceStateQueryResult::kStopped);

  StopServiceWorkItem item;
  EXPECT_TRUE(item.Do());
  EXPECT_FALSE(fake_service_manager_.stop_called());

  item.Rollback();
  EXPECT_FALSE(fake_service_manager_.start_called());
  EXPECT_EQ(fake_service_manager_.state(), ServiceStateQueryResult::kStopped);
}

TEST_F(ServiceWorkItemsTest,
       StopServiceWorkItem_SucceedsWhenServiceNotInstalled) {
  fake_service_manager_.set_state(ServiceStateQueryResult::kNotInstalled);

  StopServiceWorkItem item;
  EXPECT_TRUE(item.Do());
  EXPECT_FALSE(fake_service_manager_.stop_called());

  item.Rollback();
  EXPECT_FALSE(fake_service_manager_.start_called());
}

TEST_F(ServiceWorkItemsTest,
       InstallOrUpdateServiceWorkItem_RollbackUninstallsWhenNewlyCreated) {
  fake_service_manager_.set_state(ServiceStateQueryResult::kNotInstalled);

  InstallOrUpdateServiceWorkItem item(install_dir_.Append(L"test_host.exe"),
                                      config_dir_.Append(L"host.json"));
  EXPECT_TRUE(item.Do());
  EXPECT_TRUE(fake_service_manager_.install_called());
  EXPECT_FALSE(fake_service_manager_.uninstall_called());

  item.Rollback();
  EXPECT_TRUE(fake_service_manager_.uninstall_called());
  EXPECT_EQ(fake_service_manager_.state(),
            ServiceStateQueryResult::kNotInstalled);
}

TEST_F(
    ServiceWorkItemsTest,
    InstallOrUpdateServiceWorkItem_RollbackRestoresPreviousBinaryPathWhenExisted) {
  const std::wstring kOldBinaryPath =
      L"\"C:\\Old\\remoting_host.exe\" --type=daemon";
  fake_service_manager_.set_state(ServiceStateQueryResult::kStopped);
  fake_service_manager_.set_binary_path_name(kOldBinaryPath);

  base::FilePath new_host_exe = install_dir_.Append(L"test_host.exe");
  InstallOrUpdateServiceWorkItem item(new_host_exe,
                                      config_dir_.Append(L"host.json"));
  EXPECT_TRUE(item.Do());
  EXPECT_TRUE(fake_service_manager_.install_called());
  EXPECT_EQ(fake_service_manager_.binary_path_name(), new_host_exe.value());

  item.Rollback();
  EXPECT_FALSE(fake_service_manager_.uninstall_called());
  EXPECT_TRUE(fake_service_manager_.set_binary_path_called());
  EXPECT_EQ(fake_service_manager_.binary_path_name(), kOldBinaryPath);
}

TEST_F(ServiceWorkItemsTest,
       InstallOrUpdateServiceWorkItem_RollbackPreservesServiceWhenQueryFails) {
  fake_service_manager_.set_state(ServiceStateQueryResult::kError);

  InstallOrUpdateServiceWorkItem item(install_dir_.Append(L"test_host.exe"),
                                      config_dir_.Append(L"host.json"));
  EXPECT_TRUE(item.Do());
  EXPECT_TRUE(fake_service_manager_.install_called());

  item.Rollback();
  EXPECT_FALSE(fake_service_manager_.uninstall_called());
}

TEST_F(ServiceWorkItemsTest, StartServiceWorkItem_RollbackStops) {
  fake_service_manager_.set_state(ServiceStateQueryResult::kStopped);

  StartServiceWorkItem item;
  EXPECT_TRUE(item.Do());
  EXPECT_TRUE(fake_service_manager_.start_called());
  EXPECT_FALSE(fake_service_manager_.stop_called());
  EXPECT_EQ(fake_service_manager_.state(), ServiceStateQueryResult::kRunning);

  item.Rollback();
  EXPECT_TRUE(fake_service_manager_.stop_called());
  EXPECT_EQ(fake_service_manager_.state(), ServiceStateQueryResult::kStopped);
}

TEST_F(ServiceWorkItemsTest,
       StartServiceWorkItem_DoesNotStopOnRollbackWhenAlreadyRunning) {
  fake_service_manager_.set_state(ServiceStateQueryResult::kRunning);

  StartServiceWorkItem item;
  EXPECT_TRUE(item.Do());
  EXPECT_FALSE(fake_service_manager_.start_called());

  item.Rollback();
  EXPECT_FALSE(fake_service_manager_.stop_called());
  EXPECT_EQ(fake_service_manager_.state(), ServiceStateQueryResult::kRunning);
}

TEST_F(ServiceWorkItemsTest,
       StartServiceWorkItem_DoesNotStopOnRollbackWhenStartPending) {
  fake_service_manager_.set_state(ServiceStateQueryResult::kStartPending);

  StartServiceWorkItem item;
  EXPECT_TRUE(item.Do());
  EXPECT_FALSE(fake_service_manager_.start_called());

  item.Rollback();
  EXPECT_FALSE(fake_service_manager_.stop_called());
  EXPECT_EQ(fake_service_manager_.state(),
            ServiceStateQueryResult::kStartPending);
}

TEST_F(ServiceWorkItemsTest,
       StartServiceWorkItem_StartsWhenStopPendingAndStopsOnRollback) {
  fake_service_manager_.set_state(ServiceStateQueryResult::kStopPending);

  StartServiceWorkItem item;
  EXPECT_TRUE(item.Do());
  EXPECT_TRUE(fake_service_manager_.start_called());
  EXPECT_EQ(fake_service_manager_.state(), ServiceStateQueryResult::kRunning);

  item.Rollback();
  EXPECT_TRUE(fake_service_manager_.stop_called());
  EXPECT_EQ(fake_service_manager_.state(), ServiceStateQueryResult::kStopped);
}

TEST_F(ServiceWorkItemsTest, SignalUpdatePendingWorkItem_SignalsAndResets) {
  std::wstring event_name = CreateUniqueEventName(L"Local\\TestSignalWorkItem");
  base::win::ScopedHandle event(::CreateEventW(nullptr, /*bManualReset=*/TRUE,
                                               /*bInitialState=*/FALSE,
                                               event_name.c_str()));
  ASSERT_TRUE(event.is_valid());

  SignalUpdatePendingWorkItem item(event_name);
  EXPECT_TRUE(item.best_effort());
  EXPECT_TRUE(item.Do());
  EXPECT_EQ(::WaitForSingleObject(event.Get(), 0),
            static_cast<DWORD>(WAIT_OBJECT_0));

  item.Rollback();
  EXPECT_EQ(::WaitForSingleObject(event.Get(), 0),
            static_cast<DWORD>(WAIT_TIMEOUT));
}

TEST_F(ServiceWorkItemsTest,
       SignalUpdatePendingWorkItem_PreservesPreSignaledEventOnRollback) {
  std::wstring event_name =
      CreateUniqueEventName(L"Local\\TestPreSignaledWorkItem");
  base::win::ScopedHandle event(::CreateEventW(nullptr, /*bManualReset=*/TRUE,
                                               /*bInitialState=*/TRUE,
                                               event_name.c_str()));
  ASSERT_TRUE(event.is_valid());

  SignalUpdatePendingWorkItem item(event_name);
  EXPECT_TRUE(item.Do());
  EXPECT_EQ(::WaitForSingleObject(event.Get(), 0),
            static_cast<DWORD>(WAIT_OBJECT_0));

  // Rollback must not reset the event since it was already signaled before Do()
  // executed.
  item.Rollback();
  EXPECT_EQ(::WaitForSingleObject(event.Get(), 0),
            static_cast<DWORD>(WAIT_OBJECT_0));
}

TEST_F(ServiceWorkItemsTest,
       SignalUpdatePendingWorkItem_SucceedsWhenEventDoesNotExist) {
  std::wstring event_name =
      CreateUniqueEventName(L"Local\\NonExistentEventWorkItem");

  SignalUpdatePendingWorkItem item(event_name);
  EXPECT_TRUE(item.Do());
}

TEST_F(ServiceWorkItemsTest, ConditionEventExists_TrueWhenEventExists) {
  std::wstring event_name =
      CreateUniqueEventName(L"Local\\ConditionEventExists");
  base::win::ScopedHandle event(::CreateEventW(nullptr, /*bManualReset=*/TRUE,
                                               /*bInitialState=*/FALSE,
                                               event_name.c_str()));
  ASSERT_TRUE(event.is_valid());

  ConditionEventExists condition(event_name);
  EXPECT_TRUE(condition.ShouldRun());
}

TEST_F(ServiceWorkItemsTest, ConditionEventExists_FalseWhenEventDoesNotExist) {
  std::wstring event_name =
      CreateUniqueEventName(L"Local\\ConditionEventMissing");

  ConditionEventExists condition(event_name);
  EXPECT_FALSE(condition.ShouldRun());
}

}  // namespace remoting::installer
