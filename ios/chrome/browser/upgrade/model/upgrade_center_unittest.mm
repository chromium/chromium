// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/upgrade/model/upgrade_center.h"

#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/upgrade/model/upgrade_recommended_details.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "testing/platform_test.h"

namespace {

class UpgradeCenterTest : public PlatformTest {
 public:
  unsigned int count_;

 protected:
  void SetUp() override {
    upgrade_center_ = [[UpgradeCenter alloc] init];
    count_ = 0;
  }

  void TearDown() override {
    @autoreleasepool {
      [upgrade_center_ resetForTests];
      upgrade_center_ = nil;
    }
  }

  IOSChromeScopedTestingLocalState scoped_local_state_;
  __strong UpgradeCenter* upgrade_center_ = nil;
};

}  // namespace

@interface FakeUpgradeCenterClient : NSObject <UpgradeCenterClient>
- (instancetype)initWithTest:(UpgradeCenterTest*)test;
@end

@implementation FakeUpgradeCenterClient {
  raw_ptr<UpgradeCenterTest> _test;
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
  [upgrade_center_ registerClient:fake withHandler:nil];
  EXPECT_EQ(count_, 0u);
  [upgrade_center_ unregisterClient:fake];
}

TEST_F(UpgradeCenterTest, GoodUpgradeAfterRegistration) {
  EXPECT_EQ(count_, 0u);
  FakeUpgradeCenterClient* fake =
      [[FakeUpgradeCenterClient alloc] initWithTest:this];
  [upgrade_center_ registerClient:fake withHandler:nil];
  EXPECT_EQ(count_, 0u);

  UpgradeRecommendedDetails details;
  details.next_version = "9999.9999.9999.9999";
  details.upgrade_url = GURL("http://foobar.org");
  [upgrade_center_ upgradeNotificationDidOccur:details];
  EXPECT_EQ(count_, 1u);
  [upgrade_center_ unregisterClient:fake];
}

TEST_F(UpgradeCenterTest, GoodUpgradeBeforeRegistration) {
  UpgradeRecommendedDetails details;
  details.next_version = "9999.9999.9999.9999";
  details.upgrade_url = GURL("http://foobar.org");
  [upgrade_center_ upgradeNotificationDidOccur:details];
  EXPECT_EQ(count_, 0u);
  FakeUpgradeCenterClient* fake =
      [[FakeUpgradeCenterClient alloc] initWithTest:this];
  [upgrade_center_ registerClient:fake withHandler:nil];
  EXPECT_EQ(count_, 1u);
  [upgrade_center_ unregisterClient:fake];
}

TEST_F(UpgradeCenterTest, NoRepeatedDisplay) {
  FakeUpgradeCenterClient* fake =
      [[FakeUpgradeCenterClient alloc] initWithTest:this];
  [upgrade_center_ registerClient:fake withHandler:nil];
  EXPECT_EQ(count_, 0u);

  // First notification should display
  UpgradeRecommendedDetails details;
  details.next_version = "9999.9999.9999.9999";
  details.upgrade_url = GURL("http://foobar.org");
  [upgrade_center_ upgradeNotificationDidOccur:details];
  EXPECT_EQ(count_, 1u);

  // Second shouldn't, since it was just displayed.
  [upgrade_center_ upgradeNotificationDidOccur:details];
  EXPECT_EQ(count_, 1u);

  // After enough time has elapsed, it should again.
  [upgrade_center_ setLastDisplayToPast];
  [upgrade_center_ upgradeNotificationDidOccur:details];
  EXPECT_EQ(count_, 2u);

  [upgrade_center_ unregisterClient:fake];
}

TEST_F(UpgradeCenterTest, NewVersionResetsInterval) {
  FakeUpgradeCenterClient* fake =
      [[FakeUpgradeCenterClient alloc] initWithTest:this];
  [upgrade_center_ registerClient:fake withHandler:nil];
  EXPECT_EQ(count_, 0u);

  // First notification should display
  UpgradeRecommendedDetails details;
  details.next_version = "9999.9999.9999.9998";
  details.upgrade_url = GURL("http://foobar.org");
  [upgrade_center_ upgradeNotificationDidOccur:details];
  EXPECT_EQ(count_, 1u);

  // Second shouldn't, since it was just displayed.
  [upgrade_center_ upgradeNotificationDidOccur:details];
  EXPECT_EQ(count_, 1u);

  // A new version should show right away though.
  details.next_version = "9999.9999.9999.9999";
  [upgrade_center_ upgradeNotificationDidOccur:details];
  EXPECT_EQ(count_, 2u);

  [upgrade_center_ unregisterClient:fake];
}

}  // namespace
