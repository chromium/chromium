// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/registry_util_win.h"

#include <windows.h>

#include <string>

#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
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

  base::win::RegKey key(HKEY_CURRENT_USER, kTestKeyPath, KEY_READ);
  ASSERT_TRUE(key.Valid());
  std::wstring read_value;
  EXPECT_EQ(key.ReadValue(nullptr, &read_value), ERROR_SUCCESS);
  EXPECT_EQ(read_value, kTestValue);
}

TEST_F(RegistryUtilWinTest, WriteDefaultStringValueBothViews) {
  EXPECT_TRUE(WriteDefaultStringValue(HKEY_LOCAL_MACHINE, kTestKeyPath,
                                      kTestValue,
                                      KEY_WOW64_32KEY | KEY_WOW64_64KEY));

  for (REGSAM view : {KEY_WOW64_32KEY, KEY_WOW64_64KEY}) {
    base::win::RegKey key(HKEY_LOCAL_MACHINE, kTestKeyPath, KEY_READ | view);
    ASSERT_TRUE(key.Valid());
    std::wstring read_value;
    EXPECT_EQ(key.ReadValue(nullptr, &read_value), ERROR_SUCCESS);
    EXPECT_EQ(read_value, kTestValue);
  }
}

TEST_F(RegistryUtilWinTest, DeleteKey) {
  EXPECT_TRUE(
      WriteDefaultStringValue(HKEY_CURRENT_USER, kTestKeyPath, kTestValue));

  DeleteKey(HKEY_CURRENT_USER, kTestKeyPath);

  base::win::RegKey key(HKEY_CURRENT_USER, kTestKeyPath, KEY_READ);
  EXPECT_FALSE(key.Valid());
}

TEST_F(RegistryUtilWinTest, DeleteKeyBothViews) {
  EXPECT_TRUE(WriteDefaultStringValue(HKEY_LOCAL_MACHINE, kTestKeyPath,
                                      kTestValue,
                                      KEY_WOW64_32KEY | KEY_WOW64_64KEY));

  DeleteKey(HKEY_LOCAL_MACHINE, kTestKeyPath,
            KEY_WOW64_32KEY | KEY_WOW64_64KEY);

  for (REGSAM view : {KEY_WOW64_32KEY, KEY_WOW64_64KEY}) {
    base::win::RegKey key(HKEY_LOCAL_MACHINE, kTestKeyPath, KEY_READ | view);
    EXPECT_FALSE(key.Valid());
  }
}

TEST_F(RegistryUtilWinTest, DeleteKeyIfEmptyNonExistentKey) {
  EXPECT_TRUE(DeleteKeyIfEmpty(HKEY_CURRENT_USER, kTestKeyPath));
}

TEST_F(RegistryUtilWinTest, DeleteKeyIfEmptyWithValues) {
  EXPECT_TRUE(
      WriteDefaultStringValue(HKEY_CURRENT_USER, kTestKeyPath, kTestValue));

  // Should not delete because the key has a default value.
  EXPECT_TRUE(DeleteKeyIfEmpty(HKEY_CURRENT_USER, kTestKeyPath));

  base::win::RegKey key(HKEY_CURRENT_USER, kTestKeyPath, KEY_READ);
  EXPECT_TRUE(key.Valid());
}

TEST_F(RegistryUtilWinTest, DeleteKeyIfEmptyWithSubkeys) {
  base::win::RegKey subkey;
  std::wstring child_path = std::wstring(kTestKeyPath) + L"\\ChildKey";
  ASSERT_EQ(subkey.Create(HKEY_CURRENT_USER, child_path.c_str(), KEY_WRITE),
            ERROR_SUCCESS);

  // Should not delete because the parent has a subkey.
  EXPECT_TRUE(DeleteKeyIfEmpty(HKEY_CURRENT_USER, kTestKeyPath));

  base::win::RegKey key(HKEY_CURRENT_USER, kTestKeyPath, KEY_READ);
  EXPECT_TRUE(key.Valid());
}

TEST_F(RegistryUtilWinTest, DeleteKeyIfEmptyWhenTrulyEmpty) {
  base::win::RegKey key;
  ASSERT_EQ(key.Create(HKEY_CURRENT_USER, kTestKeyPath, KEY_WRITE),
            ERROR_SUCCESS);
  key.Close();

  EXPECT_TRUE(DeleteKeyIfEmpty(HKEY_CURRENT_USER, kTestKeyPath));

  base::win::RegKey verify_key(HKEY_CURRENT_USER, kTestKeyPath, KEY_READ);
  EXPECT_FALSE(verify_key.Valid());
}

TEST_F(RegistryUtilWinTest, DeleteKeyIfEmptyWithWow64Access) {
  for (REGSAM view : {KEY_WOW64_32KEY, KEY_WOW64_64KEY}) {
    base::win::RegKey key;
    ASSERT_EQ(key.Create(HKEY_LOCAL_MACHINE, kTestKeyPath, KEY_WRITE | view),
              ERROR_SUCCESS);
    key.Close();

    EXPECT_TRUE(DeleteKeyIfEmpty(HKEY_LOCAL_MACHINE, kTestKeyPath, view));

    base::win::RegKey verify_key(HKEY_LOCAL_MACHINE, kTestKeyPath,
                                 KEY_READ | view);
    EXPECT_FALSE(verify_key.Valid());
  }
}

TEST_F(RegistryUtilWinTest, DeleteKeyIfEmptyBothViews) {
  for (REGSAM view : {KEY_WOW64_32KEY, KEY_WOW64_64KEY}) {
    base::win::RegKey key;
    ASSERT_EQ(key.Create(HKEY_LOCAL_MACHINE, kTestKeyPath, KEY_WRITE | view),
              ERROR_SUCCESS);
    key.Close();
  }

  EXPECT_TRUE(DeleteKeyIfEmpty(HKEY_LOCAL_MACHINE, kTestKeyPath,
                               KEY_WOW64_32KEY | KEY_WOW64_64KEY));

  for (REGSAM view : {KEY_WOW64_32KEY, KEY_WOW64_64KEY}) {
    base::win::RegKey verify_key(HKEY_LOCAL_MACHINE, kTestKeyPath,
                                 KEY_READ | view);
    EXPECT_FALSE(verify_key.Valid());
  }
}

}  // namespace remoting
