// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_BLOCK_CLEANUP_TEST_H_
#define IOS_CHROME_TEST_BLOCK_CLEANUP_TEST_H_

#import <Foundation/Foundation.h>

#include "testing/platform_test.h"

@class NSAutoreleasePool;

// Extends PlatformTest to provide a TearDown() method that spins the runloop
// for a short time.  This allows any blocks created during tests to clean up
// after themselves.
class BlockCleanupTest : public PlatformTest {
 protected:
  void SetUp() override;
  void TearDown() override;

  void SpinRunLoop(NSTimeInterval cleanup_time);

 private:
  id block_cleanup_pool_;
};

#endif  // IOS_CHROME_TEST_BLOCK_CLEANUP_TEST_H_
