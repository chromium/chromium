// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/scoped_handle.h"
#include "build/build_config.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/sandbox_factory.h"
#include "sandbox/win/src/target_services.h"
#include "sandbox/win/tests/common/controller.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {

// Loads and or unloads a DLL passed in the second parameter of argv.
// The first parameter of argv is 'L' = load, 'U' = unload or 'B' for both.
SBOX_TESTS_COMMAND int UseOneDLL(int argc, wchar_t** argv) {
  if (argc != 2)
    return SBOX_TEST_FAILED_TO_RUN_TEST;
  int rv = SBOX_TEST_FAILED_TO_RUN_TEST;

  wchar_t option = (argv[0])[0];
  if ((option == L'L') || (option == L'B')) {
    HMODULE module1 = ::LoadLibraryW(argv[1]);
    rv = (!module1) ? SBOX_TEST_FAILED : SBOX_TEST_SUCCEEDED;
  }

  if ((option == L'U') || (option == L'B')) {
    HMODULE module2 = ::GetModuleHandleW(argv[1]);
    rv = ::FreeLibrary(module2) ? SBOX_TEST_SUCCEEDED : SBOX_TEST_FAILED;
  }
  return rv;
}

// Opens an HKLM registry key passed as the first parameter of argv.
SBOX_TESTS_COMMAND int SimpleOpenKey(int argc, wchar_t** argv) {
  if (argc != 1)
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;
  HKEY handle;
  if (::RegOpenKeyEx(HKEY_LOCAL_MACHINE, argv[0], 0, KEY_ENUMERATE_SUB_KEYS,
                     &handle)) {
    return SBOX_TEST_FAILED;
  }
  ::CloseHandle(handle);
  return SBOX_TEST_SUCCEEDED;
}

// Fails on Windows ARM64: https://crbug.com/905526
#if defined(ARCH_CPU_ARM64)
#define MAYBE_BaselineAvicapDll DISABLED_BaselineAvicapDll
#else
#define MAYBE_BaselineAvicapDll BaselineAvicapDll
#endif
TEST(UnloadDllTest, MAYBE_BaselineAvicapDll) {
  TestRunner runner;
  runner.SetTestState(BEFORE_REVERT);
  runner.SetTimeout(2000);
  // Add a registry rule, because that ensures that the interception agent has
  // more than one item in its internal table.
  EXPECT_TRUE(runner.AddRule(TargetPolicy::SUBSYS_REGISTRY,
                             TargetPolicy::REG_ALLOW_ANY,
                             L"HKEY_LOCAL_MACHINE\\Software\\Microsoft"));

  // Note for the puzzled: avicap32.dll is a 64-bit dll in 64-bit versions of
  // windows so this test and the others just work.
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"UseOneDLL L avicap32.dll"));
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"UseOneDLL B avicap32.dll"));
}

TEST(UnloadDllTest, UnloadAviCapDllNoPatching) {
  TestRunner runner;
  runner.SetTestState(BEFORE_REVERT);
  runner.SetTimeout(2000);
  sandbox::TargetPolicy* policy = runner.GetPolicy();
  policy->AddDllToUnload(L"avicap32.dll");
  EXPECT_EQ(SBOX_TEST_FAILED, runner.RunTest(L"UseOneDLL L avicap32.dll"));
  EXPECT_EQ(SBOX_TEST_FAILED, runner.RunTest(L"UseOneDLL B avicap32.dll"));
}

TEST(UnloadDllTest, UnloadAviCapDllWithPatching) {
  TestRunner runner;
  runner.SetTimeout(2000);
  runner.SetTestState(BEFORE_REVERT);
  sandbox::TargetPolicy* policy = runner.GetPolicy();
  policy->AddDllToUnload(L"avicap32.dll");

  // Add a couple of rules that ensures that the interception agent add EAT
  // patching on the client which makes sure that the unload dll record does
  // not interact badly with them.
  EXPECT_TRUE(runner.AddRule(TargetPolicy::SUBSYS_REGISTRY,
                             TargetPolicy::REG_ALLOW_READONLY,
                             L"HKEY_LOCAL_MACHINE"));
  EXPECT_TRUE(runner.AddRule(TargetPolicy::SUBSYS_REGISTRY,
                             TargetPolicy::REG_ALLOW_ANY,
                             L"HKEY_LOCAL_MACHINE\\Software\\Microsoft"));

  EXPECT_EQ(SBOX_TEST_FAILED, runner.RunTest(L"UseOneDLL L avicap32.dll"));

  runner.SetTestState(AFTER_REVERT);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED,
            runner.RunTest(L"SimpleOpenKey software\\microsoft"));
}

}  // namespace sandbox
