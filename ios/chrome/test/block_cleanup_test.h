// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_BLOCK_CLEANUP_TEST_H_
#define IOS_CHROME_TEST_BLOCK_CLEANUP_TEST_H_

#import <Foundation/Foundation.h>

#import <memory>
#import <optional>

#import "base/apple/scoped_nsautorelease_pool.h"
#import "base/memory/stack_allocated.h"
#import "testing/platform_test.h"

// Extends PlatformTest to provide a TearDown() method that spins the runloop
// for a short time.  This allows any blocks created during tests to clean up
// after themselves.
class BlockCleanupTest : public PlatformTest {
 public:
  BlockCleanupTest();
  ~BlockCleanupTest() override;

 protected:
  void SetUp() override;
  void TearDown() override;

  void SpinRunLoop(NSTimeInterval cleanup_time);

 private:
  STACK_ALLOCATED_IGNORE("https://crbug.com/1424190")
  std::optional<base::apple::ScopedNSAutoreleasePool> pool_;
};

#endif  // IOS_CHROME_TEST_BLOCK_CLEANUP_TEST_H_
