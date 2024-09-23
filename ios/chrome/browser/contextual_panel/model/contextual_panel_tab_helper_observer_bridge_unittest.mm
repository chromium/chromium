// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/contextual_panel/model/contextual_panel_tab_helper_observer_bridge.h"

#import "ios/chrome/browser/contextual_panel/model/contextual_panel_tab_helper_observer.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

@interface TestContextualTabHelperObserving
    : NSObject <ContextualPanelTabHelperObserving>

@property(nonatomic, assign) BOOL contextualPanelHasNewDataCalled;

@property(nonatomic, assign) BOOL contextualPanelTabHelperDestroyedCalled;

@end

@implementation TestContextualTabHelperObserving

- (void)contextualPanel:(ContextualPanelTabHelper*)tabHelper
             hasNewData:
                 (std::vector<base::WeakPtr<ContextualPanelItemConfiguration>>)
                     item_configurations {
  self.contextualPanelHasNewDataCalled = YES;
}

- (void)contextualPanelTabHelperDestroyed:(ContextualPanelTabHelper*)tabHelper {
  self.contextualPanelTabHelperDestroyedCalled = YES;
}

@end

class ContextualPanelTabHelperObserverBridgeTest : public PlatformTest {
 public:
  ContextualPanelTabHelperObserverBridgeTest() {
    tab_helper_observing_ = [[TestContextualTabHelperObserving alloc] init];
    bridge_ = std::make_unique<ContextualPanelTabHelperObserverBridge>(
        tab_helper_observing_);
  }

 protected:
  TestContextualTabHelperObserving* tab_helper_observing_;
  std::unique_ptr<ContextualPanelTabHelperObserverBridge> bridge_;
};

TEST_F(ContextualPanelTabHelperObserverBridgeTest, ForwardsMethods) {
  bridge_->ContextualPanelHasNewData(nil, {});
  EXPECT_TRUE(tab_helper_observing_.contextualPanelHasNewDataCalled);

  bridge_->ContextualPanelTabHelperDestroyed(nil);
  EXPECT_TRUE(tab_helper_observing_.contextualPanelTabHelperDestroyedCalled);
}
