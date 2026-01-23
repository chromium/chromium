// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/bottom_sheet/ui/passkey_creation_bottom_sheet_view_controller.h"

#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/password_manager/core/browser/features/password_features.h"
#import "ios/chrome/browser/favicon/model/test_favicon_loader.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/ui/bottom_sheet/table_view_bottom_sheet_view_controller.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_view_controller.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

// Tests for PasskeyCreationBottomSheetViewController.
class PasskeyCreationBottomSheetViewControllerTest : public PlatformTest {
 protected:
  PasskeyCreationBottomSheetViewControllerTest() {
    handler_ = OCMProtocolMock(@protocol(BrowserCoordinatorCommands));
    view_controller_ = [[PasskeyCreationBottomSheetViewController alloc]
        initWithHandler:handler_
          faviconLoader:&favicon_loader_];
  }

  id handler_;
  PasskeyCreationBottomSheetViewController* view_controller_;
  TestFaviconLoader favicon_loader_;
};

// Tests that the view controller shows the correct information.
TEST_F(PasskeyCreationBottomSheetViewControllerTest, BasicInformation) {
  NSString* username = @"user";
  NSString* email = @"email@example.com";
  GURL url("https://example.com");

  [view_controller_ setUsername:username email:email url:url];
  [view_controller_ loadView];
  [view_controller_ viewDidLoad];

  EXPECT_TRUE(view_controller_.view);
  EXPECT_TRUE(view_controller_.aboveTitleView);
  EXPECT_TRUE(view_controller_.underTitleView);

  // Verifies the title string.
  EXPECT_NSEQ(
      l10n_util::GetNSString(IDS_IOS_PASSKEY_CREATION_BOTTOM_SHEET_TITLE),
      view_controller_.titleString);

  // Verifies username and domain.
  UIStackView* underTitleView =
      base::apple::ObjCCast<UIStackView>(view_controller_.underTitleView);
  UIStackView* accountInfoStackView =
      base::apple::ObjCCast<UIStackView>(underTitleView.arrangedSubviews[0]);
  UIStackView* labelsStackView = base::apple::ObjCCast<UIStackView>(
      accountInfoStackView.arrangedSubviews[1]);

  UILabel* usernameLabel =
      base::apple::ObjCCast<UILabel>(labelsStackView.arrangedSubviews[0]);
  UILabel* domainLabel =
      base::apple::ObjCCast<UILabel>(labelsStackView.arrangedSubviews[1]);

  EXPECT_NSEQ(username, usernameLabel.text);
  EXPECT_NSEQ(base::SysUTF8ToNSString(url.host()), domainLabel.text);

  // Verifies the button stack configuration.
  EXPECT_NSEQ(
      l10n_util::GetNSString(IDS_IOS_PASSKEY_CREATION_BOTTOM_SHEET_CREATE),
      view_controller_.configuration.primaryActionString);
  EXPECT_NSEQ(l10n_util::GetNSString(
                  IDS_IOS_PASSKEY_CREATION_BOTTOM_SHEET_SAVE_ANOTHER_WAY),
              view_controller_.configuration.secondaryActionString);
}
