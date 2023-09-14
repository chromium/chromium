// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#import <memory>

#import "base/apple/scoped_nsautorelease_pool.h"
#import "base/check.h"
#import "ios/chrome/test/block_cleanup_test.h"

BlockCleanupTest::BlockCleanupTest() = default;
BlockCleanupTest::~BlockCleanupTest() = default;

void BlockCleanupTest::SetUp() {
  pool_.emplace();
}

void BlockCleanupTest::TearDown() {
  // Block-copied items are released asynchronously; spin the loop to give
  // them a chance to be cleaned up.
  const NSTimeInterval kCleanupTime = 0.1;
  SpinRunLoop(kCleanupTime);

  // Drain the autorelease pool to finish cleaning up after blocks.
  DCHECK(pool_);
  pool_.reset();

  PlatformTest::TearDown();
}

void BlockCleanupTest::SpinRunLoop(NSTimeInterval cleanup_time) {
  NSDate* cleanup_start = NSDate.date;
  while (fabs(cleanup_start.timeIntervalSinceNow) < cleanup_time) {
    NSDate* beforeDate =
        [[NSDate alloc] initWithTimeIntervalSinceNow:cleanup_time];
    [NSRunLoop.currentRunLoop runMode:NSDefaultRunLoopMode
                           beforeDate:beforeDate];
  }
}
