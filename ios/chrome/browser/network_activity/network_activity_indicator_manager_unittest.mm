// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/network_activity/network_activity_indicator_manager.h"

#import <UIKit/UIKit.h>

#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

NSString* const kNetworkActivityKeyOne =
    @"NetworkActivityIndicatorManagerTest.NetworkActivityIndicatorKeyOne";
NSString* const kNetworkActivityKeyTwo =
    @"NetworkActivityIndicatorManagerTest.NetworkActivityIndicatorKeyTwo";

class NetworkActivityIndicatorManagerTest : public PlatformTest {
 public:
  NetworkActivityIndicatorManagerTest()
      : manager_([[NetworkActivityIndicatorManager alloc] init]) {}

 protected:
  void ExpectNetworkActivity(NSUInteger groupOneCount,
                             NSUInteger groupTwoCount) {
    EXPECT_EQ([manager_ numNetworkTasksForGroup:kNetworkActivityKeyOne],
              groupOneCount);
    EXPECT_EQ([manager_ numNetworkTasksForGroup:kNetworkActivityKeyTwo],
              groupTwoCount);
    EXPECT_EQ([manager_ numTotalNetworkTasks], groupOneCount + groupTwoCount);
    if (groupOneCount + groupTwoCount > 0U) {
      EXPECT_TRUE([[UIApplication sharedApplication]
          isNetworkActivityIndicatorVisible]);
    } else {
      EXPECT_FALSE([[UIApplication sharedApplication]
          isNetworkActivityIndicatorVisible]);
    }
  }
  NetworkActivityIndicatorManager* manager_;
};

TEST_F(NetworkActivityIndicatorManagerTest, TestNumNetworkTasksForGroup) {
  EXPECT_EQ([manager_ numNetworkTasksForGroup:kNetworkActivityKeyOne], 0U);
  EXPECT_EQ([manager_ numNetworkTasksForGroup:kNetworkActivityKeyTwo], 0U);
  [manager_ startNetworkTasks:2U forGroup:kNetworkActivityKeyOne];
  EXPECT_EQ([manager_ numNetworkTasksForGroup:kNetworkActivityKeyOne], 2U);
  EXPECT_EQ([manager_ numNetworkTasksForGroup:kNetworkActivityKeyTwo], 0U);
  [manager_ startNetworkTasks:3U forGroup:kNetworkActivityKeyTwo];
  EXPECT_EQ([manager_ numNetworkTasksForGroup:kNetworkActivityKeyOne], 2U);
  EXPECT_EQ([manager_ numNetworkTasksForGroup:kNetworkActivityKeyTwo], 3U);
  [manager_ stopNetworkTasks:2U forGroup:kNetworkActivityKeyOne];
  EXPECT_EQ([manager_ numNetworkTasksForGroup:kNetworkActivityKeyOne], 0U);
  EXPECT_EQ([manager_ numNetworkTasksForGroup:kNetworkActivityKeyTwo], 3U);
  [manager_ stopNetworkTasks:3U forGroup:kNetworkActivityKeyTwo];
  EXPECT_EQ([manager_ numNetworkTasksForGroup:kNetworkActivityKeyOne], 0U);
  EXPECT_EQ([manager_ numNetworkTasksForGroup:kNetworkActivityKeyTwo], 0U);
}

TEST_F(NetworkActivityIndicatorManagerTest, TestNumTotalNetworkTasks) {
  EXPECT_EQ([manager_ numTotalNetworkTasks], 0U);
  [manager_ startNetworkTasks:2U forGroup:kNetworkActivityKeyOne];
  EXPECT_EQ([manager_ numTotalNetworkTasks], 2U);
  [manager_ startNetworkTasks:3U forGroup:kNetworkActivityKeyTwo];
  EXPECT_EQ([manager_ numTotalNetworkTasks], 5U);
  [manager_ stopNetworkTasks:2U forGroup:kNetworkActivityKeyOne];
  EXPECT_EQ([manager_ numTotalNetworkTasks], 3U);
  [manager_ stopNetworkTasks:3U forGroup:kNetworkActivityKeyTwo];
  EXPECT_EQ([manager_ numTotalNetworkTasks], 0U);
}

TEST_F(NetworkActivityIndicatorManagerTest, StartStopNetworkTaskForOneGroup) {
  ExpectNetworkActivity(0U, 0U);
  [manager_ startNetworkTaskForGroup:kNetworkActivityKeyOne];
  ExpectNetworkActivity(1U, 0U);
  [manager_ stopNetworkTaskForGroup:kNetworkActivityKeyOne];
  ExpectNetworkActivity(0U, 0U);
}

TEST_F(NetworkActivityIndicatorManagerTest, StartStopNetworkTaskForTwoGroups) {
  ExpectNetworkActivity(0U, 0U);
  [manager_ startNetworkTaskForGroup:kNetworkActivityKeyOne];
  ExpectNetworkActivity(1U, 0U);
  [manager_ startNetworkTaskForGroup:kNetworkActivityKeyTwo];
  ExpectNetworkActivity(1U, 1U);
  [manager_ stopNetworkTaskForGroup:kNetworkActivityKeyOne];
  ExpectNetworkActivity(0U, 1U);
  [manager_ stopNetworkTaskForGroup:kNetworkActivityKeyTwo];
  ExpectNetworkActivity(0U, 0U);
}

TEST_F(NetworkActivityIndicatorManagerTest, StartStopNetworkTasksForOneGroup) {
  ExpectNetworkActivity(0U, 0U);
  [manager_ startNetworkTasks:2U forGroup:kNetworkActivityKeyOne];
  ExpectNetworkActivity(2U, 0U);
  [manager_ stopNetworkTasks:2U forGroup:kNetworkActivityKeyOne];
  ExpectNetworkActivity(0U, 0U);
}

TEST_F(NetworkActivityIndicatorManagerTest, StartStopNetworkTasksForTwoGroups) {
  ExpectNetworkActivity(0U, 0U);
  [manager_ startNetworkTasks:2U forGroup:kNetworkActivityKeyOne];
  ExpectNetworkActivity(2U, 0U);
  [manager_ startNetworkTasks:3U forGroup:kNetworkActivityKeyTwo];
  ExpectNetworkActivity(2U, 3U);
  [manager_ stopNetworkTasks:2U forGroup:kNetworkActivityKeyOne];
  ExpectNetworkActivity(0U, 3U);
  [manager_ stopNetworkTasks:3U forGroup:kNetworkActivityKeyTwo];
  ExpectNetworkActivity(0U, 0U);
}

TEST_F(NetworkActivityIndicatorManagerTest, ClearNetworkTasksForGroup) {
  ExpectNetworkActivity(0U, 0U);
  [manager_ startNetworkTasks:2U forGroup:kNetworkActivityKeyOne];
  ExpectNetworkActivity(2U, 0U);
  [manager_ clearNetworkTasksForGroup:kNetworkActivityKeyOne];
  ExpectNetworkActivity(0U, 0U);
}

TEST_F(NetworkActivityIndicatorManagerTest, ClearNetworkTasksForUnusedGroup) {
  ExpectNetworkActivity(0U, 0U);
  [manager_ clearNetworkTasksForGroup:kNetworkActivityKeyOne];
  ExpectNetworkActivity(0U, 0U);
}

TEST_F(NetworkActivityIndicatorManagerTest, StartStopNetworkTasksInChunks) {
  ExpectNetworkActivity(0U, 0U);
  [manager_ startNetworkTasks:2U forGroup:kNetworkActivityKeyOne];
  ExpectNetworkActivity(2U, 0U);
  [manager_ startNetworkTasks:3U forGroup:kNetworkActivityKeyTwo];
  ExpectNetworkActivity(2U, 3U);
  [manager_ startNetworkTasks:7U forGroup:kNetworkActivityKeyOne];
  ExpectNetworkActivity(9U, 3U);
  [manager_ startNetworkTaskForGroup:kNetworkActivityKeyTwo];
  ExpectNetworkActivity(9U, 4U);
  [manager_ stopNetworkTasks:4U forGroup:kNetworkActivityKeyOne];
  ExpectNetworkActivity(5U, 4U);
  [manager_ stopNetworkTasks:2U forGroup:kNetworkActivityKeyTwo];
  ExpectNetworkActivity(5U, 2U);
  [manager_ stopNetworkTasks:5U forGroup:kNetworkActivityKeyOne];
  ExpectNetworkActivity(0U, 2U);
  [manager_ clearNetworkTasksForGroup:kNetworkActivityKeyTwo];
  ExpectNetworkActivity(0U, 0U);
}

}  // namespace
