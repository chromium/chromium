// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/google_services/personalize_google_services_view_controller.h"

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/settings/google_services/personalize_google_services_command_handler.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

// Tests the PersonalizeGoogleServicesViewController's core functionality.
class PersonalizeGoogleServicesViewControllerUnittest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();

    view_controller_ = [[PersonalizeGoogleServicesViewController alloc]
        initWithStyle:ChromeTableViewStyle()];
    mock_command_handler_ =
        OCMProtocolMock(@protocol(PersonalizeGoogleServicesCommandHandler));
    view_controller_.handler = mock_command_handler_;
  }

 protected:
  PersonalizeGoogleServicesViewController* view_controller_;
  id<PersonalizeGoogleServicesCommandHandler> mock_command_handler_;
};

// Test the Web and App Activity item.
TEST_F(PersonalizeGoogleServicesViewControllerUnittest,
       TestOpenWebAndAppActivityItem) {
  OCMExpect([mock_command_handler_ openWebAppActivityDialog]);

  // Get the item's index path and select the item.
  NSIndexPath* itemIndexPath = [NSIndexPath indexPathForRow:0 inSection:0];
  [view_controller_ tableView:view_controller_.tableView
      didSelectRowAtIndexPath:itemIndexPath];

  EXPECT_OCMOCK_VERIFY(mock_command_handler_);
}

// Test the Linked Google services item.
TEST_F(PersonalizeGoogleServicesViewControllerUnittest,
       TestLinkedGoogleServicesItem) {
  OCMExpect([mock_command_handler_ openLinkedGoogleServicesDialog]);

  // Get the item's index path and select the item.
  NSIndexPath* itemIndexPath = [NSIndexPath indexPathForRow:1 inSection:0];
  [view_controller_ tableView:view_controller_.tableView
      didSelectRowAtIndexPath:itemIndexPath];

  EXPECT_OCMOCK_VERIFY(mock_command_handler_);
}
