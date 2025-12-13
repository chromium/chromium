// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/location_bar/badge/coordinator/location_bar_badge_coordinator.h"

#import "base/test/task_environment.h"
#import "ios/chrome/browser/contextual_panel/entrypoint/coordinator/contextual_panel_entrypoint_mediator_delegate.h"
#import "ios/chrome/browser/location_bar/badge/coordinator/location_bar_badge_coordinator_delegate.h"
#import "ios/chrome/browser/location_bar/badge/coordinator/location_bar_badge_mediator_delegate.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

// Test fixture for LocationBarBadgeCoordinator.
class LocationBarBadgeCoordinatorTest : public PlatformTest {
 protected:
  LocationBarBadgeCoordinatorTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    coordinator_ = [[LocationBarBadgeCoordinator alloc]
        initWithBaseViewController:nil
                           browser:browser_.get()];
    mock_coordinator_delegate_ =
        OCMProtocolMock(@protocol(LocationBarBadgeCoordinatorDelegate));
    coordinator_.delegate = mock_coordinator_delegate_;
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  LocationBarBadgeCoordinator* coordinator_;
  id mock_coordinator_delegate_;
};

// Tests that the delegate method `setLocationBarLabelCenteredBetweenContent:`
// is called.
TEST_F(LocationBarBadgeCoordinatorTest,
       SetLocationBarLabelCenteredBetweenContentDelegateCall) {
  OCMExpect([mock_coordinator_delegate_
      setLocationBarLabelCenteredBetweenContent:coordinator_
                                       centered:YES]);
  id<LocationBarBadgeMediatorDelegate> mediator_delegate =
      static_cast<id>(coordinator_);
  [mediator_delegate setLocationBarLabelCenteredBetweenContent:nil
                                                      centered:YES];
  EXPECT_OCMOCK_VERIFY(mock_coordinator_delegate_);
}

// Tests that the delegate method `canShowLargeContextualPanelEntrypoint:` is
// called.
TEST_F(LocationBarBadgeCoordinatorTest,
       CanShowLargeContextualPanelEntrypointDelegateCall) {
  OCMExpect([mock_coordinator_delegate_
      canShowLargeContextualPanelEntrypoint:coordinator_]);
  id<ContextualPanelEntrypointMediatorDelegate> mediator_delegate =
      static_cast<id>(coordinator_);
  [mediator_delegate canShowLargeContextualPanelEntrypoint:nil];
  EXPECT_OCMOCK_VERIFY(mock_coordinator_delegate_);
}
