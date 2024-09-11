// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/testing/scoped_block_swizzler.h"
#import "base/apple/foundation_util.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

// Class containing two methods that will be swizzled by the unittests.
@interface ScopedBlockSwizzlerTestClass : NSObject

// An NSString property that will be accessed by one of the swizzled methods.
@property(nonatomic, copy) NSString* value;

+ (NSString*)classMethodToSwizzle;
- (NSString*)instanceMethodToSwizzle;
@end

namespace {

NSString* const kOriginalClassValue = @"Bar";
NSString* const kSwizzledClassValue = @"Foo";
NSString* const kOriginalInstanceValue = @"Bizz";
NSString* const kSwizzledInstanceValue = @"Buzz";

using ScopedBlockSwizzlerTest = PlatformTest;

// Tests that swizzling a class method works properly.
TEST_F(ScopedBlockSwizzlerTest, SwizzlingClassMethods) {
  EXPECT_NSEQ(kOriginalClassValue,
              [ScopedBlockSwizzlerTestClass classMethodToSwizzle]);

  {
    id block = ^NSString*(id self) { return kSwizzledClassValue; };
    ScopedBlockSwizzler swizzler([ScopedBlockSwizzlerTestClass class],
                                 @selector(classMethodToSwizzle), block);
    EXPECT_NSEQ(kSwizzledClassValue,
                [ScopedBlockSwizzlerTestClass classMethodToSwizzle]);
  }

  EXPECT_NSEQ(kOriginalClassValue,
              [ScopedBlockSwizzlerTestClass classMethodToSwizzle]);
}

// Tests that swizzling an instance method works properly.
TEST_F(ScopedBlockSwizzlerTest, SwizzlingInstanceMethod) {
  ScopedBlockSwizzlerTestClass* target =
      [[ScopedBlockSwizzlerTestClass alloc] init];
  target.value = kSwizzledInstanceValue;

  EXPECT_NSEQ(kOriginalInstanceValue, [target instanceMethodToSwizzle]);
  EXPECT_NSNE([target instanceMethodToSwizzle], kSwizzledInstanceValue);

  {
    id block = ^NSString*(id self) {
      return base::apple::ObjCCastStrict<ScopedBlockSwizzlerTestClass>(self)
          .value;
    };
    ScopedBlockSwizzler swizzler([ScopedBlockSwizzlerTestClass class],
                                 @selector(instanceMethodToSwizzle), block);
    EXPECT_NSEQ(kSwizzledInstanceValue, [target instanceMethodToSwizzle]);
  }

  EXPECT_NSEQ(kOriginalInstanceValue, [target instanceMethodToSwizzle]);
}

// Tests that calling |ScopedBlockSwizzler::reset()| properly unswizzles the
// method.
TEST_F(ScopedBlockSwizzlerTest, TestReset) {
  EXPECT_NSEQ(kOriginalClassValue,
              [ScopedBlockSwizzlerTestClass classMethodToSwizzle]);

  id block = ^NSString*(id self) { return kSwizzledClassValue; };
  std::unique_ptr<ScopedBlockSwizzler> swizzler(
      new ScopedBlockSwizzler([ScopedBlockSwizzlerTestClass class],
                              @selector(classMethodToSwizzle), block));
  EXPECT_NSEQ(kSwizzledClassValue,
              [ScopedBlockSwizzlerTestClass classMethodToSwizzle]);

  swizzler.reset();
  EXPECT_NSEQ(kOriginalClassValue,
              [ScopedBlockSwizzlerTestClass classMethodToSwizzle]);
}

}  // namespace

#pragma mark - ScopedBlockSwizzlerTestClass

@implementation ScopedBlockSwizzlerTestClass

@synthesize value = _value;

+ (NSString*)classMethodToSwizzle {
  return kOriginalClassValue;
}

- (NSString*)instanceMethodToSwizzle {
  return kOriginalInstanceValue;
}

@end
