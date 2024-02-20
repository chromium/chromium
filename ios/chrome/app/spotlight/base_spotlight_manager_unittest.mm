// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/spotlight/base_spotlight_manager.h"

#import "ios/chrome/app/spotlight/fake_searchable_item_factory.h"
#import "ios/chrome/app/spotlight/fake_spotlight_interface.h"
#import "ios/chrome/app/spotlight/spotlight_manager.h"
#import "ios/chrome/app/spotlight/spotlight_util.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

class BaseSpotlightManagerTest : public PlatformTest {
 public:
  BaseSpotlightManagerTest() {
    spotlight_interface_ = [[FakeSpotlightInterface alloc] init];
    searchable_item_factory_ = [[FakeSearchableItemFactory alloc]
        initWithDomain:spotlight::DOMAIN_READING_LIST];
  }

 protected:
  FakeSpotlightInterface* spotlight_interface_;
  FakeSearchableItemFactory* searchable_item_factory_;
};

TEST_F(BaseSpotlightManagerTest, testInitAndShutdown) {
  BaseSpotlightManager* manager = [[BaseSpotlightManager alloc]
      initWithSpotlightInterface:spotlight_interface_
           searchableItemFactory:searchable_item_factory_];
  EXPECT_EQ(manager.spotlightInterface, spotlight_interface_);
  EXPECT_EQ(manager.searchableItemFactory, searchable_item_factory_);
  EXPECT_EQ(manager.isShuttingDown, NO);

  EXPECT_EQ(searchable_item_factory_.cancelItemsGenerationCallCount, 0);
  [manager shutdown];
  EXPECT_EQ(searchable_item_factory_.cancelItemsGenerationCallCount, 1);
  EXPECT_EQ(manager.isShuttingDown, YES);
}

TEST_F(BaseSpotlightManagerTest, testBackground) {
  BaseSpotlightManager* manager = [[BaseSpotlightManager alloc]
      initWithSpotlightInterface:spotlight_interface_
           searchableItemFactory:searchable_item_factory_];

  // Check the notification.
  EXPECT_EQ(manager.isAppInBackground, NO);
  [[NSNotificationCenter defaultCenter]
      postNotificationName:UIApplicationDidEnterBackgroundNotification
                    object:nil
                  userInfo:nil];
  EXPECT_EQ(manager.isAppInBackground, YES);
  [[NSNotificationCenter defaultCenter]
      postNotificationName:UIApplicationWillEnterForegroundNotification
                    object:nil
                  userInfo:nil];
  EXPECT_EQ(manager.isAppInBackground, NO);

  // Check the subclassing points.
  EXPECT_EQ(manager.isAppInBackground, NO);
  [manager appDidEnterBackground];
  EXPECT_EQ(manager.isAppInBackground, YES);
  [manager appWillEnterForeground];
  EXPECT_EQ(manager.isAppInBackground, NO);
}
