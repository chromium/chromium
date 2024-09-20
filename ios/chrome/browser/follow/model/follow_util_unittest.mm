// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/follow/model/follow_util.h"

#import <UIKit/UIKit.h>

#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

class FollowUtilTest : public PlatformTest {
 protected:
  void SetUp() override { ClearUserDefault(); }

  void TearDown() override { ClearUserDefault(); }

  // Clean up the NSUserDefault.
  void ClearUserDefault() {
    NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
    [defaults removeObjectForKey:kFollowIPHPreviousDisplayEvents];
  }

  // Set the follow IPH show time array to the NSUserDefault.
  void SetFollowIPHShowTimeArray() {
    NSMutableArray<NSDictionary*>* previousEvents =
        [[NSMutableArray<NSDictionary*> alloc] init];
    NSDate* date1 = [NSDate dateWithTimeIntervalSinceNow:-3600 * 28];
    NSDate* date2 = [NSDate dateWithTimeIntervalSinceNow:-3600 * 25];
    NSDate* date3 = [NSDate dateWithTimeIntervalSinceNow:-3600 * 20];

    [previousEvents
        addObject:@{kFollowIPHHost : @"abc.com", kFollowIPHDate : date1}];
    [previousEvents
        addObject:@{kFollowIPHHost : @"def.com", kFollowIPHDate : date2}];
    [previousEvents
        addObject:@{kFollowIPHHost : @"ghi.com", kFollowIPHDate : date3}];

    NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
    [defaults setObject:previousEvents forKey:kFollowIPHPreviousDisplayEvents];
  }
};

// Tests follow IPH interval eligiblility.
TEST_F(FollowUtilTest, TestIPHInterval) {
  // Test Follow IPH can be shown when there's no Follow IPH have been
  // displayed.
  EXPECT_TRUE(IsFollowIPHShownFrequencyEligible(@"now.com"));

  StoreFollowIPHDisplayEvent(@"now.com");
  // Test Follow IPH can not be shown within 15 minutes from a previous Follow
  // IPH.
  EXPECT_FALSE(IsFollowIPHShownFrequencyEligible(@"now.com"));

  SetFollowIPHShowTimeArray();
  // Test Follow IPH can be shown for specific host if no Follow IPH has been
  // shown ever for it.
  EXPECT_TRUE(IsFollowIPHShownFrequencyEligible(@"now.com"));
  // Test Follow IPH can be shown for specific host if a Follow IPH has been
  // shown for it a day ago.
  EXPECT_TRUE(IsFollowIPHShownFrequencyEligible(@"abc.com"));
  // Test Follow IPH can not be shown for specific host if a Follow IPH has been
  // shown for it within a day.
  EXPECT_FALSE(IsFollowIPHShownFrequencyEligible(@"ghi.com"));
}

// Tests storing follow IPH display event.
TEST_F(FollowUtilTest, TestStoreFollowIPHDisplayEvent) {
  // When storing the url and time for the first time.
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  StoreFollowIPHDisplayEvent(@"now.com");
  ASSERT_EQ(
      1, (int)[[defaults objectForKey:kFollowIPHPreviousDisplayEvents] count]);

  // When storing the url and time to an existing user default.
  SetFollowIPHShowTimeArray();
  StoreFollowIPHDisplayEvent(@"now.com");
  NSArray<NSDictionary*>* updatedArray =
      [defaults objectForKey:kFollowIPHPreviousDisplayEvents];
  EXPECT_EQ(2, (int)updatedArray.count);
  EXPECT_NSEQ([updatedArray[0] objectForKey:kFollowIPHHost], @"ghi.com");
  EXPECT_NSEQ([updatedArray[1] objectForKey:kFollowIPHHost], @"now.com");
}

// Tests removing the last follow IPH display event.
TEST_F(FollowUtilTest, TestRemoveLastFollowIPHDisplayEvent) {
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  SetFollowIPHShowTimeArray();
  ASSERT_EQ(
      3, (int)[[defaults objectForKey:kFollowIPHPreviousDisplayEvents] count]);
  RemoveLastFollowIPHDisplayEvent();
  NSArray<NSDictionary*>* updatedArray =
      [defaults objectForKey:kFollowIPHPreviousDisplayEvents];
  EXPECT_EQ(2, (int)updatedArray.count);
  EXPECT_NSEQ([updatedArray[0] objectForKey:kFollowIPHHost], @"abc.com");
  EXPECT_NSEQ([updatedArray[1] objectForKey:kFollowIPHHost], @"def.com");
}
