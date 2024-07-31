// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/model/infobar_badge_tab_helper_observer_bridge.h"

#import "ios/chrome/browser/infobars/model/infobar_badge_tab_helper_observer.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

@interface TestInfobarBadgeTabHelperObserving
    : NSObject <InfobarBadgeTabHelperObserving>

@property(nonatomic, assign) BOOL infobarBadgesUpdatedCalled;

@end

@implementation TestInfobarBadgeTabHelperObserving

- (void)infobarBadgesUpdated:(InfobarBadgeTabHelper*)tabHelper {
  self.infobarBadgesUpdatedCalled = YES;
}

@end

class InfobarBadgeTabHelperObserverBridgeTest : public PlatformTest {
 public:
  InfobarBadgeTabHelperObserverBridgeTest() {
    observing_ = [[TestInfobarBadgeTabHelperObserving alloc] init];
    bridge_ = std::make_unique<InfobarBadgeTabHelperObserverBridge>(observing_);
  }

 protected:
  TestInfobarBadgeTabHelperObserving* observing_;
  std::unique_ptr<InfobarBadgeTabHelperObserverBridge> bridge_;
};

TEST_F(InfobarBadgeTabHelperObserverBridgeTest, BridgeForwardsMethodCalls) {
  bridge_->InfobarBadgesUpdated(nil);
  EXPECT_TRUE(observing_.infobarBadgesUpdatedCalled);
}
