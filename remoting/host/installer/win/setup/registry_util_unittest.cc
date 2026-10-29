// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/installer/win/setup/registry_util.h"

#include <windows.h>

#include <aclapi.h>
#include <stdint.h>

#include <array>
#include <memory>
#include <optional>
#include <string>

#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
#include "base/win/scoped_localalloc.h"
#include "chrome/installer/util/work_item.h"
#include "chrome/installer/util/work_item_list.h"
#include "remoting/base/registry_test_helper_win.h"
#include "remoting/host/win/chromoting_lib.h"
#include "remoting/host/win/security_descriptor.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting::installer {

namespace {

inline constexpr wchar_t kTestKeyPath[] = L"SOFTWARE\\TestRegistryUtil";

}  // namespace

class RegistryUtilTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_NO_FATAL_FAILURE(
        registry_override_manager_.OverrideRegistry(HKEY_LOCAL_MACHINE));
  }

  registry_util::RegistryOverrideManager registry_override_manager_;
};

TEST_F(RegistryUtilTest,
       AddCreateKeyAndSetDefaultValueWorkItems_CreatesAndRollsBack) {
  std::unique_ptr<WorkItemList> list(WorkItem::CreateWorkItemList());
  AddCreateKeyAndSetDefaultValueWorkItems(list.get(), HKEY_LOCAL_MACHINE,
                                          kTestKeyPath, L"test_default_value");

  EXPECT_TRUE(list->Do());
  EXPECT_EQ(RegistryTestHelperWin::ReadStringValue(kTestKeyPath),
            L"test_default_value");

  list->Rollback();
  EXPECT_FALSE(RegistryTestHelperWin::KeyExists(kTestKeyPath));
}

TEST_F(RegistryUtilTest,
       SetRegBinaryValueWorkItem_CreatesAndRollsBackNewValue) {
  base::win::RegKey key;
  ASSERT_EQ(key.Create(HKEY_LOCAL_MACHINE, kTestKeyPath, KEY_READ | KEY_WRITE),
            ERROR_SUCCESS);

  constexpr auto kTestBytes = std::to_array<uint8_t>({0x01, 0x02, 0x03, 0x04});
  SetRegBinaryValueWorkItem item(HKEY_LOCAL_MACHINE, kTestKeyPath,
                                 WorkItem::kWow64Default, L"BinaryVal",
                                 kTestBytes);

  EXPECT_TRUE(item.Do());

  DWORD size = 0;
  DWORD type = 0;
  ASSERT_EQ(key.ReadValue(L"BinaryVal", nullptr, &size, &type), ERROR_SUCCESS);
  EXPECT_EQ(type, static_cast<DWORD>(REG_BINARY));
  EXPECT_EQ(size, 4u);

  item.Rollback();
  EXPECT_EQ(key.ReadValue(L"BinaryVal", nullptr, &size, &type),
            ERROR_FILE_NOT_FOUND);
}

TEST_F(RegistryUtilTest,
       SetRegBinaryValueWorkItem_OverwritesAndRestoresPreviousValueOnRollback) {
  base::win::RegKey key;
  ASSERT_EQ(key.Create(HKEY_LOCAL_MACHINE, kTestKeyPath, KEY_READ | KEY_WRITE),
            ERROR_SUCCESS);

  constexpr auto kInitialBytes = std::to_array<uint8_t>({0xAA, 0xBB});
  ASSERT_EQ(
      key.WriteValue(L"BinaryVal", kInitialBytes.data(),
                     static_cast<DWORD>(kInitialBytes.size()), REG_BINARY),
      ERROR_SUCCESS);

  constexpr auto kUpdatedBytes =
      std::to_array<uint8_t>({0x10, 0x20, 0x30, 0x40});
  SetRegBinaryValueWorkItem item(HKEY_LOCAL_MACHINE, kTestKeyPath,
                                 WorkItem::kWow64Default, L"BinaryVal",
                                 kUpdatedBytes);

  EXPECT_TRUE(item.Do());

  std::array<uint8_t, 4> read_updated = {};
  DWORD size = static_cast<DWORD>(read_updated.size());
  DWORD type = 0;
  ASSERT_EQ(key.ReadValue(L"BinaryVal", read_updated.data(), &size, &type),
            ERROR_SUCCESS);
  EXPECT_EQ(read_updated, kUpdatedBytes);

  item.Rollback();

  std::array<uint8_t, 2> read_restored = {};
  size = static_cast<DWORD>(read_restored.size());
  ASSERT_EQ(key.ReadValue(L"BinaryVal", read_restored.data(), &size, &type),
            ERROR_SUCCESS);
  EXPECT_EQ(size, 2u);
  EXPECT_EQ(read_restored, kInitialBytes);
}

TEST_F(RegistryUtilTest,
       CreateProtectedRegKeyWorkItem_CreatesWithProtectedDaclAndRollsBack) {
  std::wstring protected_path = L"SOFTWARE\\TestProtectedKey";
  CreateProtectedRegKeyWorkItem item(HKEY_LOCAL_MACHINE, protected_path,
                                     WorkItem::kWow64Default,
                                     std::string(kPairingRegistrySecretsSddl));

  EXPECT_TRUE(item.Do());
  EXPECT_TRUE(RegistryTestHelperWin::KeyExists(protected_path));

  {
    base::win::RegKey key(HKEY_LOCAL_MACHINE, protected_path.c_str(),
                          KEY_READ | READ_CONTROL);
    ASSERT_TRUE(key.Valid());

    PSECURITY_DESCRIPTOR raw_sd = nullptr;
    PACL dacl = nullptr;
    ASSERT_EQ(::GetSecurityInfo(key.Handle(), SE_REGISTRY_KEY,
                                DACL_SECURITY_INFORMATION,
                                /*ppsidOwner=*/nullptr, /*ppsidGroup=*/nullptr,
                                &dacl, /*ppSacl=*/nullptr, &raw_sd),
              static_cast<DWORD>(ERROR_SUCCESS));
    base::win::ScopedLocalAlloc scoped_sd = base::win::TakeLocalAlloc(raw_sd);
    ASSERT_NE(dacl, nullptr);

    SECURITY_DESCRIPTOR_CONTROL control = 0;
    DWORD revision = 0;
    ASSERT_TRUE(
        ::GetSecurityDescriptorControl(scoped_sd.get(), &control, &revision));
    EXPECT_NE(control & SE_DACL_PROTECTED, 0);
  }

  item.Rollback();
  EXPECT_FALSE(RegistryTestHelperWin::KeyExists(protected_path));
}

TEST_F(RegistryUtilTest,
       CreateProtectedRegKeyWorkItem_RollbackPreservesPreExistingKey) {
  std::wstring protected_path = L"SOFTWARE\\TestExistingProtectedKey";
  {
    base::win::RegKey existing_key;
    ASSERT_EQ(existing_key.Create(HKEY_LOCAL_MACHINE, protected_path.c_str(),
                                  KEY_WRITE),
              ERROR_SUCCESS);
    ASSERT_EQ(existing_key.WriteValue(L"ExistingVal", L"keep"), ERROR_SUCCESS);
  }

  CreateProtectedRegKeyWorkItem item(HKEY_LOCAL_MACHINE, protected_path,
                                     WorkItem::kWow64Default,
                                     std::string(kPairingRegistrySecretsSddl));

  EXPECT_TRUE(item.Do());
  item.Rollback();

  EXPECT_TRUE(RegistryTestHelperWin::KeyExists(protected_path));
  EXPECT_EQ(
      RegistryTestHelperWin::ReadStringValue(protected_path, L"ExistingVal"),
      L"keep");
}

TEST_F(RegistryUtilTest, FormatClsidString) {
  std::wstring clsid_str = FormatClsidString(CLSID_RdpDesktopSession);
  EXPECT_FALSE(clsid_str.empty());
  EXPECT_EQ(clsid_str.front(), L'{');
  EXPECT_EQ(clsid_str.back(), L'}');
}

}  // namespace remoting::installer
