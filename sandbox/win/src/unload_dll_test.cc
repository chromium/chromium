// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "base/time/time.h"
#include "build/build_config.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/tests/common/controller.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {

namespace {
constexpr wchar_t kLoad[] = L"Load";
constexpr wchar_t kUnload[] = L"Unload";
constexpr wchar_t kBoth[] = L"Both";
}  // namespace

// Loads and or unloads a DLL passed in the second parameter of argv.
// The first parameter of args is "Load", "Unload" or "Both".
SBOX_TEST_COMMAND(UseOneDLL) {
  if (args.size() != 2) {
    return SBOX_TEST_FAILED_TO_RUN_TEST;
  }

  bool do_load = args[0] == kLoad || args[0] == kBoth;
  bool do_unload = args[0] == kUnload || args[0] == kBoth;

  if (!do_load && !do_unload) {
    return SBOX_TEST_INVALID_PARAMETER;
  }

  int rv = SBOX_TEST_FAILED_TO_RUN_TEST;

  if (do_load) {
    HMODULE module = ::LoadLibraryW(args[1].c_str());
    rv = module ? SBOX_TEST_SUCCEEDED : SBOX_TEST_FAILED;
  }

  if (do_unload) {
    HMODULE module = ::GetModuleHandleW(args[1].c_str());
    rv = ::FreeLibrary(module) ? SBOX_TEST_SUCCEEDED : SBOX_TEST_FAILED;
  }

  return rv;
}

// Opens the current executable's path.
SBOX_TEST_COMMAND(OpenExecutablePath) {
  WCHAR full_path[MAX_PATH];
  if (!::GetModuleFileName(nullptr, full_path, MAX_PATH)) {
    return SBOX_TEST_FIRST_ERROR;
  }
  if (::GetFileAttributes(full_path) == INVALID_FILE_ATTRIBUTES) {
    return SBOX_TEST_FAILED;
  }
  return SBOX_TEST_SUCCEEDED;
}

std::unique_ptr<UseOneDLLTestRunner> BaselineAvicapRunner() {
  auto runner = std::make_unique<UseOneDLLTestRunner>();
  runner->SetTestState(BEFORE_REVERT);
  runner->SetTimeout(base::Milliseconds(2000));
  // Add a file rule, because that ensures that the interception agent has
  // more than one item in its internal table.
  runner->AllowFileAccess(FileSemantics::kAllowReadonly, L"\\??\\*.exe");
  return runner;
}

// Fails on Windows ARM64: https://crbug.com/905526
#if defined(ARCH_CPU_ARM64)
#define MAYBE_BaselineAvicapDll DISABLED_BaselineAvicapDll
#else
#define MAYBE_BaselineAvicapDll BaselineAvicapDll
#endif
TEST(UnloadDllTest, MAYBE_BaselineAvicapDll) {
  // Note for the puzzled: avicap32.dll is a 64-bit dll in 64-bit versions of
  // windows so this test and the others just work.
  EXPECT_EQ(SBOX_TEST_SUCCEEDED,
            BaselineAvicapRunner()->RunTest(kLoad, L"avicap32.dll"));
  EXPECT_EQ(SBOX_TEST_SUCCEEDED,
            BaselineAvicapRunner()->RunTest(kBoth, L"avicap32.dll"));
}

std::unique_ptr<UseOneDLLTestRunner> UnloadAvicapNoPatchingRunner() {
  auto runner = std::make_unique<UseOneDLLTestRunner>();
  runner->SetTestState(BEFORE_REVERT);
  runner->SetTimeout(base::Milliseconds(2000));
  runner->GetConfig()->AddDllToUnload(L"avicap32.dll");
  return runner;
}

TEST(UnloadDllTest, UnloadAviCapDllNoPatching) {
  EXPECT_EQ(SBOX_TEST_FAILED,
            UnloadAvicapNoPatchingRunner()->RunTest(kLoad, L"avicap32.dll"));
  EXPECT_EQ(SBOX_TEST_FAILED,
            UnloadAvicapNoPatchingRunner()->RunTest(kBoth, L"avicap32.dll"));
}

template <typename T>
void ConfigAvicapWithPatching(T& runner) {
  runner.SetTimeout(base::Milliseconds(2000));
  runner.GetConfig()->AddDllToUnload(L"avicap32.dll");
  // Add a couple of rules that ensures that the interception agent add EAT
  // patching on the client which makes sure that the unload dll record does
  // not interact badly with them.
  runner.AllowFileAccess(FileSemantics::kAllowReadonly, L"\\??\\*.exe");
  runner.AllowFileAccess(FileSemantics::kAllowReadonly, L"\\??\\*.log");
}

TEST(UnloadDllTest, UnloadAviCapDllWithPatching) {
  UseOneDLLTestRunner runner;
  ConfigAvicapWithPatching(runner);
  runner.SetTestState(BEFORE_REVERT);
  EXPECT_EQ(SBOX_TEST_FAILED, runner.RunTest(kLoad, L"avicap32.dll"));

  OpenExecutablePathTestRunner runner2;
  ConfigAvicapWithPatching(runner2);
  runner2.SetTestState(AFTER_REVERT);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner2.RunTest());
}

}  // namespace sandbox
