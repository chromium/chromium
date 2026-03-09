// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/390223051): Remove C-library calls to fix the errors.
#pragma allow_unsafe_libc_calls
#endif

// These tests have been added to specifically tests issues arising from (A)LPC
// lock down.

#include <windows.h>

#include <winioctl.h>

#include <algorithm>

#include "base/containers/heap_array.h"
#include "base/strings/string_number_conversions_win.h"
#include "build/build_config.h"
#include "sandbox/win/src/heap_helper.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/sandbox_factory.h"
#include "sandbox/win/src/sandbox_policy.h"
#include "sandbox/win/tests/common/controller.h"
#include "sandbox/win/tests/common/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {

namespace {

bool CsrssDisconnectSupported() {
#if defined(_WIN64) && !defined(ADDRESS_SANITIZER)
  return true;
#else
  return false;
#endif  // defined(_WIN64) && !defined(ADDRESS_SANITIZER)
}

}  // namespace
SBOX_TEST_COMMAND(Lpc_GetUserDefaultLangID) {
  if (args.size() != 1) {
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;
  }
  unsigned int langid;
  if (!base::StringToUint(args[0], &langid)) {
    return SBOX_TEST_INVALID_PARAMETER;
  }
  // This will cause an exception if not warmed up suitably.
  return langid == ::GetUserDefaultLangID() ? SBOX_TEST_SUCCEEDED
                                            : SBOX_TEST_FAILED;
}

TEST(LpcPolicyTest, GetUserDefaultLangID) {
  Lpc_GetUserDefaultLangIDTestRunner runner;
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(::GetUserDefaultLangID()));
}

SBOX_TEST_COMMAND(Lpc_GetUserDefaultLCID) {
  if (args.size() != 1) {
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;
  }

  unsigned int lcid;
  if (!base::StringToUint(args[0], &lcid)) {
    return SBOX_TEST_INVALID_PARAMETER;
  }
  // This will cause an exception if not warmed up suitably.
  return lcid == ::GetUserDefaultLCID() ? SBOX_TEST_SUCCEEDED
                                        : SBOX_TEST_FAILED;
}

TEST(LpcPolicyTest, GetUserDefaultLCID) {
  Lpc_GetUserDefaultLCIDTestRunner runner;
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(::GetUserDefaultLCID()));
}

std::wstring GetLocaleName() {
  wchar_t locale_name[LOCALE_NAME_MAX_LENGTH] = {};
  if (!::GetUserDefaultLocaleName(locale_name, LOCALE_NAME_MAX_LENGTH)) {
    return {};
  }
  return locale_name;
}

SBOX_TEST_COMMAND(Lpc_GetUserDefaultLocaleName) {
  if (args.size() != 1) {
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;
  }

  std::wstring locale_name = GetLocaleName();
  if (locale_name.empty()) {
    return SBOX_TEST_FIRST_ERROR;
  }

  return args[0] == locale_name ? SBOX_TEST_SUCCEEDED : SBOX_TEST_FAILED;
}

TEST(LpcPolicyTest, GetUserDefaultLocaleName) {
  Lpc_GetUserDefaultLocaleNameTestRunner runner;
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(GetLocaleName()));
}

// Closing ALPC port can invalidate its heap.
// Test that all heaps are valid.
SBOX_TEST_COMMAND(Lpc_TestValidProcessHeaps) {
  // Retrieves the number of heaps in the current process.
  DWORD number_of_heaps = ::GetProcessHeaps(0, nullptr);
  // Try to retrieve a handle to all the heaps owned by this process. Returns
  // false if the number of heaps has changed.
  //
  // This is inherently racy as is, but it's not something that we observe a lot
  // in Chrome, the heaps tend to be created at startup only.
  auto all_heaps = base::HeapArray<HANDLE>::Uninit(number_of_heaps);
  if (::GetProcessHeaps(number_of_heaps, all_heaps.data()) != number_of_heaps) {
    return SBOX_TEST_FIRST_ERROR;
  }

  for (size_t i = 0; i < number_of_heaps; ++i) {
    HANDLE handle = all_heaps[i];
    ULONG HeapInformation;
    bool result = HeapQueryInformation(handle, HeapCompatibilityInformation,
                                       &HeapInformation,
                                       sizeof(HeapInformation), nullptr);
    if (!result)
      return SBOX_TEST_SECOND_ERROR;
  }
  return SBOX_TEST_SUCCEEDED;
}

TEST(LpcPolicyTest, TestValidProcessHeaps) {
  Lpc_TestValidProcessHeapsTestRunner runner;
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest());
}

// All processes should have a shared heap with csrss.exe. This test ensures
// that this heap can be found.
TEST(LpcPolicyTest, TestCanFindCsrPortHeap) {
  if (!CsrssDisconnectSupported()) {
    return;
  }
  HANDLE csr_port_handle = sandbox::FindCsrPortHeap();
  EXPECT_NE(nullptr, csr_port_handle);
}

// Fails on Windows ARM64: https://crbug.com/905328
#if defined(ARCH_CPU_ARM64)
#define MAYBE_TestHeapFlags DISABLED_TestHeapFlags
#else
#define MAYBE_TestHeapFlags TestHeapFlags
#endif

TEST(LpcPolicyTest, MAYBE_TestHeapFlags) {
  if (!CsrssDisconnectSupported()) {
    return;
  }
  // Windows does not support callers supplying arbitrary flag values. So we
  // write some non-trivial value to reduce the chance we match this in random
  // data.
  DWORD flags = 0x41007;
  HANDLE heap = HeapCreate(flags, 0, 0);
  EXPECT_NE(nullptr, heap);
  DWORD actual_flags = 0;
  EXPECT_TRUE(sandbox::HeapFlags(heap, &actual_flags));
  EXPECT_EQ(flags, actual_flags);
  EXPECT_TRUE(HeapDestroy(heap));
}

}  // namespace sandbox
