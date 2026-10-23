// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/installer/win/setup/registry_work_items.h"

#include <windows.h>

#include <string>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/strcat.h"
#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
#include "remoting/base/registry_test_helper_win.h"
#include "remoting/host/installer/win/setup/installer_constants.h"
#include "remoting/host/installer/win/setup/native_messaging_entries.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting::installer {

class RegistryWorkItemsTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_NO_FATAL_FAILURE(
        registry_override_manager_.OverrideRegistry(HKEY_LOCAL_MACHINE));
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    install_dir_ = temp_dir_.GetPath().Append(L"install");
  }

  registry_util::RegistryOverrideManager registry_override_manager_;
  base::ScopedTempDir temp_dir_;
  base::FilePath install_dir_;
};

TEST_F(RegistryWorkItemsTest, HostRegistryWorkItem_SuccessAndRollback) {
  HostRegistryWorkItem item(install_dir_);

  EXPECT_TRUE(item.Do());

  for (const auto& entry : kNativeMessagingEntries) {
    std::wstring key_path =
        base::StrCat({entry.registry_parent_path, L"\\", entry.host_name});
    for (REGSAM view : kBothRegistryViews) {
      EXPECT_TRUE(RegistryTestHelperWin::KeyExists(key_path, view));
    }
  }

  item.Rollback();

  for (const auto& entry : kNativeMessagingEntries) {
    std::wstring key_path =
        base::StrCat({entry.registry_parent_path, L"\\", entry.host_name});
    for (REGSAM view : kBothRegistryViews) {
      EXPECT_FALSE(RegistryTestHelperWin::KeyExists(key_path, view));
    }
  }
}

TEST_F(RegistryWorkItemsTest,
       HostRegistryWorkItem_UpdateModePreservesRegistryOnRollback) {
  HostRegistryWorkItem item(install_dir_, /*is_update=*/true);

  EXPECT_TRUE(item.Do());

  for (const auto& entry : kNativeMessagingEntries) {
    std::wstring key_path =
        base::StrCat({entry.registry_parent_path, L"\\", entry.host_name});
    for (REGSAM view : kBothRegistryViews) {
      EXPECT_TRUE(RegistryTestHelperWin::KeyExists(key_path, view));
    }
  }

  item.Rollback();

  for (const auto& entry : kNativeMessagingEntries) {
    std::wstring key_path =
        base::StrCat({entry.registry_parent_path, L"\\", entry.host_name});
    for (REGSAM view : kBothRegistryViews) {
      EXPECT_TRUE(RegistryTestHelperWin::KeyExists(key_path, view));
    }
  }
}

TEST_F(RegistryWorkItemsTest,
       HostRegistryWorkItem_AutoDetectsExistingInstallAndPreservesOnRollback) {
  std::wstring chromoting_key =
      base::StrCat({L"SOFTWARE\\", kChromotingKeyPath});
  {
    base::win::RegKey existing_key;
    ASSERT_EQ(existing_key.Create(HKEY_LOCAL_MACHINE, chromoting_key.c_str(),
                                  KEY_WRITE | KEY_WOW64_64KEY),
              ERROR_SUCCESS);
  }
  HostRegistryWorkItem item(install_dir_, /*is_update=*/false);

  EXPECT_TRUE(item.Do());
  item.Rollback();

  EXPECT_TRUE(RegistryTestHelperWin::KeyExists(chromoting_key));
  for (const auto& entry : kNativeMessagingEntries) {
    std::wstring key_path =
        base::StrCat({entry.registry_parent_path, L"\\", entry.host_name});
    for (REGSAM view : kBothRegistryViews) {
      EXPECT_TRUE(RegistryTestHelperWin::KeyExists(key_path, view));
    }
  }
}

TEST_F(RegistryWorkItemsTest, UnregisterRegistryWorkItem_RemovesEntries) {
  HostRegistryWorkItem register_item(install_dir_);
  ASSERT_TRUE(register_item.Do());

  UnregisterRegistryWorkItem unregister_item;
  EXPECT_TRUE(unregister_item.Do());

  for (const auto& entry : kNativeMessagingEntries) {
    std::wstring key_path =
        base::StrCat({entry.registry_parent_path, L"\\", entry.host_name});
    for (REGSAM view : kBothRegistryViews) {
      EXPECT_FALSE(RegistryTestHelperWin::KeyExists(key_path, view));
    }
  }
}

}  // namespace remoting::installer
