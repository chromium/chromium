// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/data_controls/model/rules_service_observer_bridge.h"

#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

@interface FakeRulesServiceObserver : NSObject <RulesServiceObserving>
@property(nonatomic, assign) BOOL rulesUpdatedCalled;
@end

@implementation FakeRulesServiceObserver
- (void)onRulesUpdated {
  self.rulesUpdatedCalled = YES;
}
@end

class RulesServiceObserverBridgeTest : public PlatformTest {
 protected:
  RulesServiceObserverBridgeTest()
      : observer_([[FakeRulesServiceObserver alloc] init]),
        bridge_(observer_) {}

  FakeRulesServiceObserver* observer_;
  data_controls::RulesServiceObserverBridge bridge_;
};

TEST_F(RulesServiceObserverBridgeTest, ForwardsOnRulesUpdated) {
  EXPECT_FALSE(observer_.rulesUpdatedCalled);
  bridge_.OnRulesUpdated();
  EXPECT_TRUE(observer_.rulesUpdatedCalled);
}
