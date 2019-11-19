// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/testing/scoped_method_swizzler.h"

#import <Foundation/Foundation.h>

#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Class containing methods that will be swizzled by the unittests.
@interface ScopedMethodSwizzlerTestClass : NSObject

- (NSString*)instanceMethodToSwizzle;
- (NSString*)swizzledInstanceMethod;

@end

namespace {

NSString* const kOriginalInstanceValue = @"Bizz";
NSString* const kSwizzledInstanceValue = @"Buzz";

using ScopedMethodSwizzlerTest = PlatformTest;

// Tests that swizzling an instance method works properly.
TEST_F(ScopedMethodSwizzlerTest, SwizzlingInstanceMethod) {
  ScopedMethodSwizzlerTestClass* target =
      [[ScopedMethodSwizzlerTestClass alloc] init];

  EXPECT_NSEQ(kOriginalInstanceValue, [target instanceMethodToSwizzle]);
  EXPECT_NSEQ(kSwizzledInstanceValue, [target swizzledInstanceMethod]);

  {
    ScopedMethodSwizzler swizzler([ScopedMethodSwizzlerTestClass class],
                                  @selector(instanceMethodToSwizzle),
                                  @selector(swizzledInstanceMethod));
    EXPECT_NSEQ(kSwizzledInstanceValue, [target instanceMethodToSwizzle]);
    EXPECT_NSEQ(kOriginalInstanceValue, [target swizzledInstanceMethod]);
  }

  EXPECT_NSEQ(kOriginalInstanceValue, [target instanceMethodToSwizzle]);
  EXPECT_NSEQ(kSwizzledInstanceValue, [target swizzledInstanceMethod]);
}

}  // namespace

#pragma mark - ScopedMethodSwizzlerTestClass

@implementation ScopedMethodSwizzlerTestClass

- (NSString*)instanceMethodToSwizzle {
  return kOriginalInstanceValue;
}

- (NSString*)swizzledInstanceMethod {
  return kSwizzledInstanceValue;
}

@end
