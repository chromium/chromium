// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_TESTS_INTEGRATION_TESTS_COMMON_H_
#define SANDBOX_TESTS_INTEGRATION_TESTS_COMMON_H_

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
  TESTPOLICY_EXTENSIONPOINT,
  TESTPOLICY_DYNAMICCODE,
  TESTPOLICY_NONSYSFONT,
  TESTPOLICY_MSSIGNED,
  TESTPOLICY_LOADNOREMOTE,
  TESTPOLICY_LOADNOLOW,
  TESTPOLICY_DYNAMICCODEOPTOUT,
  TESTPOLICY_LOADPREFERSYS32,
  TESTPOLICY_RESTRICTINDIRECTBRANCHPREDICTION
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
    EXPECT_EQ(WAIT_OBJECT_0,
      ::WaitForSingleObject(mutex_, SboxTestEventTimeout()));
  }

  ~ScopedTestMutex() {
    EXPECT_TRUE(::ReleaseMutex(mutex_));
    EXPECT_TRUE(::CloseHandle(mutex_));
  }

private:
  HANDLE mutex_;

  DISALLOW_COPY_AND_ASSIGN(ScopedTestMutex);
};

}  // namespace sandbox

#endif  // SANDBOX_TESTS_INTEGRATION_TESTS_COMMON_H_
