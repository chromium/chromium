// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/location_bar/badge/ui/location_bar_badge_view_controller.h"

#import "ios/chrome/browser/contextual_panel/entrypoint/ui/contextual_panel_entrypoint_mutator.h"
#import "ios/chrome/browser/location_bar/badge/model/badge_type.h"
#import "ios/chrome/browser/location_bar/badge/model/location_bar_badge_configuration.h"
#import "ios/chrome/browser/location_bar/badge/ui/location_bar_badge_mutator.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

// Interface to expose functions for testing.
@interface LocationBarBadgeViewController (Testing)
- (void)userTappedBadge;
- (void)setLocationBarLabelCenteredBetweenContent:(BOOL)centered;
- (void)dismissIPHAnimated:(BOOL)animated;
@end

// Test fixture for LocationBarBadgeViewController.
class LocationBarBadgeViewControllerTest : public PlatformTest {
 protected:
  LocationBarBadgeViewControllerTest() {
    view_controller_ = [[LocationBarBadgeViewController alloc] init];
    mock_mutator_ = OCMProtocolMock(@protocol(LocationBarBadgeMutator));
    view_controller_.mutator = mock_mutator_;
    mock_contextual_panel_mutator_ =
        OCMProtocolMock(@protocol(ContextualPanelEntrypointMutator));
    view_controller_.contextualPanelEntryPointMutator =
        mock_contextual_panel_mutator_;
  }

  LocationBarBadgeViewController* view_controller_;
  id mock_mutator_;
  id mock_contextual_panel_mutator_;
};

// Tests that the badge shows and hides correctly.
TEST_F(LocationBarBadgeViewControllerTest, ShowAndHideBadge) {
  [view_controller_ view];
  LocationBarBadgeConfiguration* config = [[LocationBarBadgeConfiguration alloc]
       initWithBadgeType:LocationBarBadgeType::kReaderMode
      accessibilityLabel:@"Reader Mode"
              badgeImage:[[UIImage alloc] init]];
  [view_controller_ setBadgeConfig:config];
  [view_controller_ showBadge];
  EXPECT_FALSE(view_controller_.view.hidden);
  [view_controller_ hideBadge];
  EXPECT_TRUE(view_controller_.view.hidden);
}

// Tests that the mutator is called when the badge is tapped.
TEST_F(LocationBarBadgeViewControllerTest, BadgeTapped) {
  [view_controller_ view];
  LocationBarBadgeConfiguration* config = [[LocationBarBadgeConfiguration alloc]
       initWithBadgeType:LocationBarBadgeType::kOverflow
      accessibilityLabel:@"Overflow"
              badgeImage:[[UIImage alloc] init]];
  [view_controller_ setBadgeConfig:config];
  OCMExpect([mock_mutator_ badgeTapped:config]);
  [view_controller_ userTappedBadge];
  EXPECT_OCMOCK_VERIFY(mock_mutator_);
}

// Tests that the mutator is called when the label is centered.
TEST_F(LocationBarBadgeViewControllerTest,
       SetLocationBarLabelCenteredBetweenContent) {
  [view_controller_ view];
  OCMExpect([mock_mutator_ setLocationBarLabelCenteredBetweenContent:YES]);
  [view_controller_ setLocationBarLabelCenteredBetweenContent:YES];
  EXPECT_OCMOCK_VERIFY(mock_mutator_);
}

// Tests that the mutator is called when the IPH is dismissed.
TEST_F(LocationBarBadgeViewControllerTest, DismissIPHAnimated) {
  [view_controller_ view];
  OCMExpect([mock_mutator_ dismissIPHAnimated:YES]);
  [mock_mutator_ dismissIPHAnimated:YES];
  EXPECT_OCMOCK_VERIFY(mock_mutator_);
}
