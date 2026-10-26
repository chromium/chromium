// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/installer/win/setup/service_work_items.h"

#include <windows.h>

#include <string>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
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

}  // namespace

class ServiceWorkItemsTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    install_dir_ = temp_dir_.GetPath().Append(L"install");
    config_dir_ = temp_dir_.GetPath().Append(L"config");

    SetStartServiceCallbackForTesting(ServiceControlCallback());
    SetStopServiceCallbackForTesting(ServiceControlCallback());
    SetInstallOrUpdateServiceCallbackForTesting(ServiceControlCallback());
    SetUninstallServiceCallbackForTesting(ServiceControlCallback());
  }

  void TearDown() override {
    SetStartServiceCallbackForTesting(ServiceControlCallback());
    SetStopServiceCallbackForTesting(ServiceControlCallback());
    SetInstallOrUpdateServiceCallbackForTesting(ServiceControlCallback());
    SetUninstallServiceCallbackForTesting(ServiceControlCallback());
  }

  base::ScopedTempDir temp_dir_;
  base::FilePath install_dir_;
  base::FilePath config_dir_;
};

TEST_F(ServiceWorkItemsTest, StopServiceWorkItem_RollbackRestartsIfWasRunning) {
  bool stop_called = false;
  bool start_called = false;

  SetStopServiceCallbackForTesting(base::BindLambdaForTesting([&]() {
    stop_called = true;
    return true;
  }));

  SetStartServiceCallbackForTesting(base::BindLambdaForTesting([&]() {
    start_called = true;
    return true;
  }));

  StopServiceWorkItem item;
  EXPECT_TRUE(item.Do());
  EXPECT_TRUE(stop_called);
  EXPECT_FALSE(start_called);

  item.Rollback();
  EXPECT_TRUE(start_called);
}

TEST_F(ServiceWorkItemsTest,
       InstallOrUpdateServiceWorkItem_RollbackUninstallsWhenNewlyCreated) {
  bool install_called = false;
  bool uninstall_called = false;

  SetInstallOrUpdateServiceCallbackForTesting(base::BindLambdaForTesting([&]() {
    install_called = true;
    return true;
  }));

  SetUninstallServiceCallbackForTesting(base::BindLambdaForTesting([&]() {
    uninstall_called = true;
    return true;
  }));

  InstallOrUpdateServiceWorkItem item(install_dir_.Append(L"test_host.exe"),
                                      config_dir_.Append(L"host.json"));
  EXPECT_TRUE(item.Do());
  EXPECT_TRUE(install_called);
  EXPECT_FALSE(uninstall_called);

  item.Rollback();
  EXPECT_TRUE(uninstall_called);
}

TEST_F(ServiceWorkItemsTest,
       InstallOrUpdateServiceWorkItem_RollbackPreservesExistingService) {
  bool install_called = false;
  bool uninstall_called = false;

  SetInstallOrUpdateServiceCallbackForTesting(base::BindLambdaForTesting([&]() {
    install_called = true;
    return true;
  }));

  SetUninstallServiceCallbackForTesting(base::BindLambdaForTesting([&]() {
    uninstall_called = true;
    return true;
  }));

  InstallOrUpdateServiceWorkItem item(install_dir_.Append(L"test_host.exe"),
                                      config_dir_.Append(L"host.json"),
                                      /*existed_previously_for_testing=*/true);
  EXPECT_TRUE(item.Do());
  EXPECT_TRUE(install_called);

  item.Rollback();
  EXPECT_FALSE(uninstall_called);
}

TEST_F(ServiceWorkItemsTest, StartServiceWorkItem_RollbackStops) {
  bool start_called = false;
  bool stop_called = false;

  SetStartServiceCallbackForTesting(base::BindLambdaForTesting([&]() {
    start_called = true;
    return true;
  }));

  SetStopServiceCallbackForTesting(base::BindLambdaForTesting([&]() {
    stop_called = true;
    return true;
  }));

  StartServiceWorkItem item;
  EXPECT_TRUE(item.Do());
  EXPECT_TRUE(start_called);
  EXPECT_FALSE(stop_called);

  item.Rollback();
  EXPECT_TRUE(stop_called);
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

}  // namespace remoting::installer
