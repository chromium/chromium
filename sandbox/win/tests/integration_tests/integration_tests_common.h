// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_TESTS_INTEGRATION_TESTS_INTEGRATION_TESTS_COMMON_H_
#define SANDBOX_WIN_TESTS_INTEGRATION_TESTS_INTEGRATION_TESTS_COMMON_H_

#include <windows.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {

//------------------------------------------------------------------------------
// Common - for sharing between source files.
//------------------------------------------------------------------------------

enum TestPolicy {
  TESTPOLICY_DEP = 1,
  TESTPOLICY_ASLR,
  TESTPOLICY_STRICTHANDLE,
  TESTPOLICY_WIN32K,
  TESTPOLICY_WIN32K_NOFAKEGDI,
  TESTPOLICY_EXTENSIONPOINT,
  TESTPOLICY_DYNAMICCODE,
  TESTPOLICY_NONSYSFONT,
  TESTPOLICY_MSSIGNED,
  TESTPOLICY_LOADNOREMOTE,
  TESTPOLICY_LOADNOLOW,
  TESTPOLICY_DYNAMICCODEOPTOUT,
  TESTPOLICY_LOADPREFERSYS32,
  TESTPOLICY_RESTRICTINDIRECTBRANCHPREDICTION,
  TESTPOLICY_CETDISABLED,
  TESTPOLICY_CETDYNAMICAPIS,
  TESTPOLICY_CETSTRICT,
  TESTPOLICY_KTMCOMPONENTFILTER,
  TESTPOLICY_PREANDPOSTSTARTUP,
  TESTPOLICY_FSCTLDISABLED,
  TESTPOLICY_RESTRICTCORESHARING,
};

// Timeout for ::WaitForSingleObject synchronization.
DWORD SboxTestEventTimeout();

// Ensures that a given set of tests specified by |name| never run at the same
// time, as they deal with machine-global data.
class ScopedTestMutex {
public:
  explicit ScopedTestMutex(const wchar_t* name)
    : mutex_(::CreateMutexW(nullptr, false, name)) {
    EXPECT_TRUE(mutex_);
    EXPECT_EQ(DWORD{WAIT_OBJECT_0},
              ::WaitForSingleObject(mutex_, SboxTestEventTimeout()));
  }

  ScopedTestMutex(const ScopedTestMutex&) = delete;
  ScopedTestMutex& operator=(const ScopedTestMutex&) = delete;

  ~ScopedTestMutex() {
    EXPECT_TRUE(::ReleaseMutex(mutex_));
    EXPECT_TRUE(::CloseHandle(mutex_));
  }

private:
 HANDLE mutex_;
};

}  // namespace sandbox

#endif  // SANDBOX_WIN_TESTS_INTEGRATION_TESTS_INTEGRATION_TESTS_COMMON_H_
