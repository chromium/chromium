// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/upgrade/upgrade_center.h"

#import "ios/chrome/browser/upgrade/upgrade_recommended_details.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

class UpgradeCenterTest : public PlatformTest {
 public:
  unsigned int count_;

 protected:
  void SetUp() override {
    [[UpgradeCenter sharedInstance] resetForTests];
    count_ = 0;
  }

  void TearDown() override { [[UpgradeCenter sharedInstance] resetForTests]; }
};

}  // namespace

@interface FakeUpgradeCenterClient : NSObject<UpgradeCenterClient>
- (instancetype)initWithTest:(UpgradeCenterTest*)test;
@end

@implementation FakeUpgradeCenterClient {
  UpgradeCenterTest* _test;
}

- (instancetype)initWithTest:(UpgradeCenterTest*)test {
  self = [super init];
  if (self) {
    _test = test;
  }
  return self;
}

- (void)showUpgrade:(UpgradeCenter*)center {
  _test->count_ += 1;
}

@end

namespace {

TEST_F(UpgradeCenterTest, NoUpgrade) {
  EXPECT_EQ(count_, 0u);
  FakeUpgradeCenterClient* fake =
      [[FakeUpgradeCenterClient alloc] initWithTest:this];
  [[UpgradeCenter sharedInstance] registerClient:fake withHandler:nil];
  EXPECT_EQ(count_, 0u);
  [[UpgradeCenter sharedInstance] unregisterClient:fake];
}

TEST_F(UpgradeCenterTest, GoodUpgradeAfterRegistration) {
  EXPECT_EQ(count_, 0u);
  FakeUpgradeCenterClient* fake =
      [[FakeUpgradeCenterClient alloc] initWithTest:this];
  [[UpgradeCenter sharedInstance] registerClient:fake withHandler:nil];
  EXPECT_EQ(count_, 0u);

  UpgradeRecommendedDetails details;
  details.next_version = "9999.9999.9999.9999";
  details.upgrade_url = GURL("http://foobar.org");
  [[UpgradeCenter sharedInstance] upgradeNotificationDidOccur:details];
  EXPECT_EQ(count_, 1u);
  [[UpgradeCenter sharedInstance] unregisterClient:fake];
}

TEST_F(UpgradeCenterTest, GoodUpgradeBeforeRegistration) {
  UpgradeRecommendedDetails details;
  details.next_version = "9999.9999.9999.9999";
  details.upgrade_url = GURL("http://foobar.org");
  [[UpgradeCenter sharedInstance] upgradeNotificationDidOccur:details];
  EXPECT_EQ(count_, 0u);
  FakeUpgradeCenterClient* fake =
      [[FakeUpgradeCenterClient alloc] initWithTest:this];
  [[UpgradeCenter sharedInstance] registerClient:fake withHandler:nil];
  EXPECT_EQ(count_, 1u);
  [[UpgradeCenter sharedInstance] unregisterClient:fake];
}

TEST_F(UpgradeCenterTest, NoRepeatedDisplay) {
  FakeUpgradeCenterClient* fake =
      [[FakeUpgradeCenterClient alloc] initWithTest:this];
  [[UpgradeCenter sharedInstance] registerClient:fake withHandler:nil];
  EXPECT_EQ(count_, 0u);

  // First notification should display
  UpgradeRecommendedDetails details;
  details.next_version = "9999.9999.9999.9999";
  details.upgrade_url = GURL("http://foobar.org");
  [[UpgradeCenter sharedInstance] upgradeNotificationDidOccur:details];
  EXPECT_EQ(count_, 1u);

  // Second shouldn't, since it was just displayed.
  [[UpgradeCenter sharedInstance] upgradeNotificationDidOccur:details];
  EXPECT_EQ(count_, 1u);

  // After enough time has elapsed, it should again.
  [[UpgradeCenter sharedInstance] setLastDisplayToPast];
  [[UpgradeCenter sharedInstance] upgradeNotificationDidOccur:details];
  EXPECT_EQ(count_, 2u);

  [[UpgradeCenter sharedInstance] unregisterClient:fake];
}

TEST_F(UpgradeCenterTest, NewVersionResetsInterval) {
  FakeUpgradeCenterClient* fake =
      [[FakeUpgradeCenterClient alloc] initWithTest:this];
  [[UpgradeCenter sharedInstance] registerClient:fake withHandler:nil];
  EXPECT_EQ(count_, 0u);

  // First notification should display
  UpgradeRecommendedDetails details;
  details.next_version = "9999.9999.9999.9998";
  details.upgrade_url = GURL("http://foobar.org");
  [[UpgradeCenter sharedInstance] upgradeNotificationDidOccur:details];
  EXPECT_EQ(count_, 1u);

  // Second shouldn't, since it was just displayed.
  [[UpgradeCenter sharedInstance] upgradeNotificationDidOccur:details];
  EXPECT_EQ(count_, 1u);

  // A new version should show right away though.
  details.next_version = "9999.9999.9999.9999";
  [[UpgradeCenter sharedInstance] upgradeNotificationDidOccur:details];
  EXPECT_EQ(count_, 2u);

  [[UpgradeCenter sharedInstance] unregisterClient:fake];
}

}  // namespace
