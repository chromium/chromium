// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/popup_menu/overflow_menu/public/overflow_menu_constants.h"
#import "ios/chrome/browser/popup_menu/overflow_menu/ui/ui_swift.h"
#import "ios/chrome/browser/popup_menu/public/popup_menu_metrics_handler.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/device_form_factor.h"

namespace {
const CGFloat kTestWindowWidth = 375.0;
const CGFloat kTestWindowHeight = 812.0;
const CGFloat kCombinedHeightThreshold = 250.0;
}  // namespace

@interface DummyPopupMenuMetricsHandler : NSObject <PopupMenuMetricsHandler>
@end

@implementation DummyPopupMenuMetricsHandler
- (void)popupMenuScrolledVertically {
}
- (void)popupMenuScrolledHorizontally {
}
- (void)popupMenuTriggerElement {
}
- (void)popupMenuDidTriggerAction:(NSInteger)actionType {
}
- (void)popupMenuUserSelectedAction {
}
- (void)popupMenuUserSelectedDestination {
}
- (void)popupMenuUserScrolledToEndOfActions {
}
@end

class OverflowMenuHostingControllerTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    model_ = [[OverflowMenuModel alloc] initWithDestinations:@[]
                                                actionGroups:@[]];

    OverflowMenuUIConfiguration* uiConfiguration =
        [[OverflowMenuUIConfiguration alloc]
            initWithPresentingViewControllerHorizontalSizeClass:
                UIUserInterfaceSizeClassCompact
                      presentingViewControllerVerticalSizeClass:
                          UIUserInterfaceSizeClassRegular
                                           highlightDestination:-1];

    metrics_handler_ = [[DummyPopupMenuMetricsHandler alloc] init];

    // Create the hosting controller using the provider.
    hosting_controller_ =
        [OverflowMenuViewProvider makeViewControllerWithModel:model_
                                              uiConfiguration:uiConfiguration
                                               metricsHandler:metrics_handler_
                                    customizationEventHandler:nil];

    hosting_controller_.view.frame =
        CGRectMake(0, 0, kTestWindowWidth, kTestWindowHeight);

    window_ = [[UIWindow alloc]
        initWithFrame:CGRectMake(0, 0, kTestWindowWidth, kTestWindowHeight)];
    window_.rootViewController = hosting_controller_;
    [window_ makeKeyAndVisible];
  }

  void TearDown() override {
    window_.hidden = YES;
    window_ = nil;
    hosting_controller_ = nil;
    metrics_handler_ = nil;
    model_ = nil;
    PlatformTest::TearDown();
  }

  OverflowMenuModel* model_;
  id<PopupMenuMetricsHandler> metrics_handler_;
  UIViewController* hosting_controller_;
  UIWindow* window_;
};

// Tests that the hosting controller's preferredContentSize is updated when the
// model changes.
TEST_F(OverflowMenuHostingControllerTest, PreferredContentSizeUpdates) {
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    // Dynamic height sizing is only enabled on iPhone; iPad uses the default
    // popover size.
    return;
  }

  // Wait until the hosting controller has found the scroll view and initialized
  // preferredContentSize.
  bool initial_layout_success = base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        [window_ setNeedsLayout];
        [window_ layoutIfNeeded];
        return hosting_controller_.preferredContentSize.height > 10;
      });
  ASSERT_TRUE(initial_layout_success);

  // The preferredContentSize should be small initially for an empty list
  // (representing default list top inset + empty section height).
  CGFloat initialHeight = hosting_controller_.preferredContentSize.height;
  EXPECT_GT(initialHeight, 0);
  EXPECT_LT(initialHeight, 150);

  // Add some actions to the model.
  NSMutableArray<OverflowMenuAction*>* actions = [[NSMutableArray alloc] init];
  for (int i = 0; i < 5; i++) {
    OverflowMenuAction* action = [[OverflowMenuAction alloc]
                   initWithName:@"Action"
                     symbolName:@"activity"
                   systemSymbol:YES
               monochromeSymbol:NO
        accessibilityIdentifier:[NSString stringWithFormat:@"Action %d", i]
             enterpriseDisabled:NO
            displayNewLabelIcon:NO
                        handler:^{
                        }];
    [actions addObject:action];
  }
  OverflowMenuActionGroup* group =
      [[OverflowMenuActionGroup alloc] initWithGroupName:@"Group"
                                                 actions:actions
                                                  footer:nil];
  [model_ setActionGroups:@[ group ]];

  // Force layout and wait until the height increases.
  bool actions_layout_success = base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        [window_ setNeedsLayout];
        [window_ layoutIfNeeded];
        return hosting_controller_.preferredContentSize.height > initialHeight;
      });
  EXPECT_TRUE(actions_layout_success);

  // The preferredContentSize height should have increased.
  CGFloat heightWithActions = hosting_controller_.preferredContentSize.height;
  EXPECT_GT(heightWithActions, initialHeight);
}

// Tests that preferredContentSize correctly includes the destinations row when
// both destinations and actions are present.
TEST_F(OverflowMenuHostingControllerTest,
       PreferredContentSizeWithDestinationsAndActions) {
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    // Dynamic height sizing is only enabled on iPhone; iPad uses the default
    // popover size.
    return;
  }

  // First wait for the initial empty list layout to complete.
  bool initial_layout_success = base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        [window_ setNeedsLayout];
        [window_ layoutIfNeeded];
        return hosting_controller_.preferredContentSize.height > 10;
      });
  ASSERT_TRUE(initial_layout_success);

  CGFloat initialHeight = hosting_controller_.preferredContentSize.height;

  NSMutableArray<OverflowMenuDestination*>* destinations =
      [[NSMutableArray alloc] init];
  for (int i = 0; i < 8; i++) {
    OverflowMenuDestination* destination = [[OverflowMenuDestination alloc]
                   initWithName:@"Destination"
                     symbolName:@"activity"
                   systemSymbol:YES
               monochromeSymbol:NO
        accessibilityIdentifier:[NSString stringWithFormat:@"Destination %d", i]
             enterpriseDisabled:NO
            displayNewLabelIcon:NO
                        handler:^{
                        }];
    [destinations addObject:destination];
  }
  [model_ setDestinationsWithAnimation:destinations];

  NSMutableArray<OverflowMenuAction*>* actions = [[NSMutableArray alloc] init];
  for (int i = 0; i < 5; i++) {
    OverflowMenuAction* action = [[OverflowMenuAction alloc]
                   initWithName:@"Action"
                     symbolName:@"activity"
                   systemSymbol:YES
               monochromeSymbol:NO
        accessibilityIdentifier:[NSString stringWithFormat:@"Action %d", i]
             enterpriseDisabled:NO
            displayNewLabelIcon:NO
                        handler:^{
                        }];
    [actions addObject:action];
  }
  OverflowMenuActionGroup* group =
      [[OverflowMenuActionGroup alloc] initWithGroupName:@"Group"
                                                 actions:actions
                                                  footer:nil];
  [model_ setActionGroups:@[ group ]];

  // Force layout and wait until the height increases beyond the initial empty
  // height.
  bool final_layout_success = base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        [window_ setNeedsLayout];
        [window_ layoutIfNeeded];
        return hosting_controller_.preferredContentSize.height > initialHeight;
      });
  EXPECT_TRUE(final_layout_success);

  CGFloat totalHeight = hosting_controller_.preferredContentSize.height;
  // The height should be large enough to include both the destinations (~137pt)
  // and actions (~320pt). We expect it to be at least 350pt.
  EXPECT_GT(totalHeight, kCombinedHeightThreshold);
}
