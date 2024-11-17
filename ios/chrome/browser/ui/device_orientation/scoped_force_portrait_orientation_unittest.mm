// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/device_orientation/scoped_force_portrait_orientation.h"

#import <memory>

#import "ios/chrome/browser/ui/device_orientation/portait_orientation_manager.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

using ScopedForcePortraitOrientationTest = PlatformTest;

@interface TestPortraitOrientationManager : NSObject
@property(nonatomic, readonly) NSInteger counter;
@end

@implementation TestPortraitOrientationManager

- (void)incrementForcePortraitOrientationCounter {
  ++_counter;
}

- (void)decrementForcePortraitOrientationCounter {
  --_counter;
}

@end

// Tests that ScopedForcePortraitOrientation call the methods
// {increment,decrement}ForcePortraitOrientationCounter on the correct manager.
TEST_F(ScopedForcePortraitOrientationTest, ForceOrientation) {
  TestPortraitOrientationManager* manager1 =
      [[TestPortraitOrientationManager alloc] init];

  TestPortraitOrientationManager* manager2 =
      [[TestPortraitOrientationManager alloc] init];

  ASSERT_EQ(manager1.counter, 0);
  ASSERT_EQ(manager2.counter, 0);

  std::unique_ptr<ScopedForcePortraitOrientation>
      scopedForcePortaitOrientation1 =
          std::make_unique<ScopedForcePortraitOrientation>(manager1);
  EXPECT_EQ(manager1.counter, 1);
  EXPECT_EQ(manager2.counter, 0);

  std::unique_ptr<ScopedForcePortraitOrientation>
      scopedForcePortaitOrientation2 =
          std::make_unique<ScopedForcePortraitOrientation>(manager1);
  EXPECT_EQ(manager1.counter, 2);
  EXPECT_EQ(manager2.counter, 0);

  std::unique_ptr<ScopedForcePortraitOrientation>
      scopedForcePortaitOrientation3 =
          std::make_unique<ScopedForcePortraitOrientation>(manager2);
  EXPECT_EQ(manager1.counter, 2);
  EXPECT_EQ(manager2.counter, 1);

  scopedForcePortaitOrientation1.reset();
  EXPECT_EQ(manager1.counter, 1);
  EXPECT_EQ(manager2.counter, 1);

  scopedForcePortaitOrientation2.reset();
  EXPECT_EQ(manager1.counter, 0);
  EXPECT_EQ(manager2.counter, 1);

  scopedForcePortaitOrientation3.reset();
  EXPECT_EQ(manager1.counter, 0);
  EXPECT_EQ(manager2.counter, 0);
}
