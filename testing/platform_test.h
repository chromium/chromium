// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TESTING_PLATFORM_TEST_H_
#define TESTING_PLATFORM_TEST_H_

#include <gtest/gtest.h>

#if defined(GTEST_OS_MAC)
#include <objc/objc.h>

// The purpose of this class us to provide a hook for platform-specific
// operations across unit tests.  For example, on the Mac, it creates and
// releases an outer NSAutoreleasePool for each test case.  For now, it's only
// implemented on the Mac.  To enable this for another platform, just adjust
// the #ifdefs and add a platform_test_<platform>.cc implementation file.
class PlatformTest : public testing::Test {
 public:
  ~PlatformTest() override;

 protected:
  PlatformTest();

 private:
  // |pool_| is a NSAutoreleasePool, but since this header may be imported from
  // files built with Objective-C ARC that forbids explicit usage of
  // NSAutoreleasePools, it is declared as id here.
  id pool_;
};
#else
typedef testing::Test PlatformTest;
#endif // GTEST_OS_MAC

#endif // TESTING_PLATFORM_TEST_H_
