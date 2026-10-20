// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/registry_util_win.h"

#include <windows.h>

#include <string>

#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
#include "remoting/base/registry_test_helper_win.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

constexpr wchar_t kTestKeyPath[] = L"Software\\ChromotingTest\\RegistryUtil";
constexpr wchar_t kTestValue[] = L"TestRegistryValue";

}  // namespace

class RegistryUtilWinTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_NO_FATAL_FAILURE(
        registry_override_manager_.OverrideRegistry(HKEY_CURRENT_USER));
    ASSERT_NO_FATAL_FAILURE(
        registry_override_manager_.OverrideRegistry(HKEY_LOCAL_MACHINE));
  }

  registry_util::RegistryOverrideManager registry_override_manager_;
};

TEST_F(RegistryUtilWinTest, WriteDefaultStringValue) {
  EXPECT_TRUE(
      WriteDefaultStringValue(HKEY_CURRENT_USER, kTestKeyPath, kTestValue));

  EXPECT_EQ(
      RegistryTestHelperWin::ReadStringValue(HKEY_CURRENT_USER, kTestKeyPath),
      kTestValue);
}

TEST_F(RegistryUtilWinTest, WriteDefaultStringValueOverwritesExistingValue) {
  ASSERT_TRUE(
      WriteDefaultStringValue(HKEY_CURRENT_USER, kTestKeyPath, L"Initial"));

  EXPECT_TRUE(
      WriteDefaultStringValue(HKEY_CURRENT_USER, kTestKeyPath, kTestValue));

  EXPECT_EQ(
      RegistryTestHelperWin::ReadStringValue(HKEY_CURRENT_USER, kTestKeyPath),
      kTestValue);
}

TEST_F(RegistryUtilWinTest, WriteDefaultStringValueBothViews) {
  EXPECT_TRUE(WriteDefaultStringValue(HKEY_LOCAL_MACHINE, kTestKeyPath,
                                      kTestValue,
                                      KEY_WOW64_32KEY | KEY_WOW64_64KEY));

  for (REGSAM view : kBothRegistryViews) {
    EXPECT_EQ(
        RegistryTestHelperWin::ReadStringValue(HKEY_LOCAL_MACHINE, kTestKeyPath,
                                               /*value_name=*/nullptr, view),
        kTestValue);
  }
}

TEST_F(RegistryUtilWinTest, DeleteKey) {
  ASSERT_TRUE(
      WriteDefaultStringValue(HKEY_CURRENT_USER, kTestKeyPath, kTestValue));

  DeleteKey(HKEY_CURRENT_USER, kTestKeyPath);

  EXPECT_FALSE(
      RegistryTestHelperWin::KeyExists(HKEY_CURRENT_USER, kTestKeyPath));
}

TEST_F(RegistryUtilWinTest, DeleteKeySingleComponentWithSubkeys) {
  ASSERT_TRUE(WriteDefaultStringValue(
      HKEY_CURRENT_USER, L"TopLevelTestKey\\NestedChild", kTestValue));

  DeleteKey(HKEY_CURRENT_USER, L"TopLevelTestKey");

  EXPECT_FALSE(
      RegistryTestHelperWin::KeyExists(HKEY_CURRENT_USER, L"TopLevelTestKey"));
}

TEST_F(RegistryUtilWinTest, DeleteKeyNonExistentKeyIsNoOp) {
  DeleteKey(HKEY_CURRENT_USER, kTestKeyPath);

  EXPECT_FALSE(
      RegistryTestHelperWin::KeyExists(HKEY_CURRENT_USER, kTestKeyPath));
}

TEST_F(RegistryUtilWinTest, DeleteKeyBothViews) {
  ASSERT_TRUE(WriteDefaultStringValue(HKEY_LOCAL_MACHINE, kTestKeyPath,
                                      kTestValue,
                                      KEY_WOW64_32KEY | KEY_WOW64_64KEY));

  DeleteKey(HKEY_LOCAL_MACHINE, kTestKeyPath,
            KEY_WOW64_32KEY | KEY_WOW64_64KEY);

  for (REGSAM view : kBothRegistryViews) {
    EXPECT_FALSE(RegistryTestHelperWin::KeyExists(HKEY_LOCAL_MACHINE,
                                                  kTestKeyPath, view));
  }
}

TEST_F(RegistryUtilWinTest, DeleteKeyIfEmptyNonExistentKey) {
  EXPECT_TRUE(DeleteKeyIfEmpty(HKEY_CURRENT_USER, kTestKeyPath));
}

TEST_F(RegistryUtilWinTest, DeleteKeyIfEmptyWithDefaultValue) {
  ASSERT_TRUE(
      WriteDefaultStringValue(HKEY_CURRENT_USER, kTestKeyPath, kTestValue));

  EXPECT_TRUE(DeleteKeyIfEmpty(HKEY_CURRENT_USER, kTestKeyPath));

  EXPECT_TRUE(
      RegistryTestHelperWin::KeyExists(HKEY_CURRENT_USER, kTestKeyPath));
}

TEST_F(RegistryUtilWinTest, DeleteKeyIfEmptyWithNamedValue) {
  {
    base::win::RegKey key;
    ASSERT_EQ(key.Create(HKEY_CURRENT_USER, kTestKeyPath, KEY_SET_VALUE),
              ERROR_SUCCESS);
    ASSERT_EQ(key.WriteValue(L"NamedValue", kTestValue), ERROR_SUCCESS);
  }

  EXPECT_TRUE(DeleteKeyIfEmpty(HKEY_CURRENT_USER, kTestKeyPath));

  EXPECT_TRUE(
      RegistryTestHelperWin::KeyExists(HKEY_CURRENT_USER, kTestKeyPath));
}

TEST_F(RegistryUtilWinTest, DeleteKeyIfEmptyWithSubkeys) {
  std::wstring child_path = std::wstring(kTestKeyPath) + L"\\ChildKey";
  {
    base::win::RegKey subkey;
    ASSERT_EQ(subkey.Create(HKEY_CURRENT_USER, child_path.c_str(), KEY_WRITE),
              ERROR_SUCCESS);
  }

  EXPECT_TRUE(DeleteKeyIfEmpty(HKEY_CURRENT_USER, kTestKeyPath));

  EXPECT_TRUE(
      RegistryTestHelperWin::KeyExists(HKEY_CURRENT_USER, kTestKeyPath));
}

TEST_F(RegistryUtilWinTest, DeleteKeyIfEmptyWhenTrulyEmpty) {
  {
    base::win::RegKey key;
    ASSERT_EQ(key.Create(HKEY_CURRENT_USER, kTestKeyPath, KEY_WRITE),
              ERROR_SUCCESS);
  }

  EXPECT_TRUE(DeleteKeyIfEmpty(HKEY_CURRENT_USER, kTestKeyPath));

  EXPECT_FALSE(
      RegistryTestHelperWin::KeyExists(HKEY_CURRENT_USER, kTestKeyPath));
}

TEST_F(RegistryUtilWinTest, DeleteKeyIfEmptyWithWow64Access) {
  for (REGSAM view : kBothRegistryViews) {
    {
      base::win::RegKey key;
      ASSERT_EQ(key.Create(HKEY_LOCAL_MACHINE, kTestKeyPath, KEY_WRITE | view),
                ERROR_SUCCESS);
    }

    EXPECT_TRUE(DeleteKeyIfEmpty(HKEY_LOCAL_MACHINE, kTestKeyPath, view));

    EXPECT_FALSE(RegistryTestHelperWin::KeyExists(HKEY_LOCAL_MACHINE,
                                                  kTestKeyPath, view));
  }
}

TEST_F(RegistryUtilWinTest, DeleteKeyIfEmptyBothViews) {
  for (REGSAM view : kBothRegistryViews) {
    base::win::RegKey key;
    ASSERT_EQ(key.Create(HKEY_LOCAL_MACHINE, kTestKeyPath, KEY_WRITE | view),
              ERROR_SUCCESS);
  }

  EXPECT_TRUE(DeleteKeyIfEmpty(HKEY_LOCAL_MACHINE, kTestKeyPath,
                               KEY_WOW64_32KEY | KEY_WOW64_64KEY));

  for (REGSAM view : kBothRegistryViews) {
    EXPECT_FALSE(RegistryTestHelperWin::KeyExists(HKEY_LOCAL_MACHINE,
                                                  kTestKeyPath, view));
  }
}

TEST_F(RegistryUtilWinTest, DeleteKeyPreservesPredefinedKeyOverride) {
  std::wstring key1 = L"Software\\ChromotingTest\\DeleteTarget";
  std::wstring key2 = L"Software\\ChromotingTest\\PreserveTarget";
  ASSERT_TRUE(WriteDefaultStringValue(HKEY_LOCAL_MACHINE, key1, L"Val1"));
  ASSERT_TRUE(WriteDefaultStringValue(HKEY_LOCAL_MACHINE, key2, L"Val2"));

  DeleteKey(HKEY_LOCAL_MACHINE, key1);

  EXPECT_FALSE(RegistryTestHelperWin::KeyExists(HKEY_LOCAL_MACHINE, key1));
  EXPECT_EQ(RegistryTestHelperWin::ReadStringValue(HKEY_LOCAL_MACHINE, key2),
            L"Val2");
}

TEST_F(RegistryUtilWinTest, DeleteKeyIfEmptyPreservesPredefinedKeyOverride) {
  std::wstring empty_key = L"Software\\ChromotingTest\\EmptyKey";
  std::wstring preserve_key = L"Software\\ChromotingTest\\PreserveKey";
  {
    base::win::RegKey key;
    ASSERT_EQ(key.Create(HKEY_LOCAL_MACHINE, empty_key.c_str(), KEY_WRITE),
              ERROR_SUCCESS);
  }
  ASSERT_TRUE(
      WriteDefaultStringValue(HKEY_LOCAL_MACHINE, preserve_key, L"Val"));

  EXPECT_TRUE(DeleteKeyIfEmpty(HKEY_LOCAL_MACHINE, empty_key));

  EXPECT_FALSE(RegistryTestHelperWin::KeyExists(HKEY_LOCAL_MACHINE, empty_key));
  EXPECT_EQ(
      RegistryTestHelperWin::ReadStringValue(HKEY_LOCAL_MACHINE, preserve_key),
      L"Val");
}

}  // namespace remoting
