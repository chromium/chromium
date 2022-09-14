// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#import "ios/chrome/test/block_cleanup_test.h"

#import "base/check.h"
#import "base/mac/foundation_util.h"

void BlockCleanupTest::SetUp() {
  block_cleanup_pool_ = [[NSAutoreleasePool alloc] init];
}

void BlockCleanupTest::TearDown() {
  // Block-copied items are released asynchronously; spin the loop to give them
  // a chance to be cleaned up.
  const NSTimeInterval kCleanupTime = 0.1;
  SpinRunLoop(kCleanupTime);

  // Drain the autorelease pool to finish cleaning up after blocks.
  // TODO(rohitrao): Can this be an EXPECT, so as to not crash the whole suite?
  DCHECK(block_cleanup_pool_);
  [base::mac::ObjCCastStrict<NSAutoreleasePool>(block_cleanup_pool_) release];
  block_cleanup_pool_ = nil;

  PlatformTest::TearDown();
}

void BlockCleanupTest::SpinRunLoop(NSTimeInterval cleanup_time) {
  NSDate* cleanup_start = [NSDate date];
  while (fabs([cleanup_start timeIntervalSinceNow]) < cleanup_time) {
    NSDate* beforeDate =
        [[NSDate alloc] initWithTimeIntervalSinceNow:cleanup_time];
    [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode
                             beforeDate:beforeDate];
  }
}
