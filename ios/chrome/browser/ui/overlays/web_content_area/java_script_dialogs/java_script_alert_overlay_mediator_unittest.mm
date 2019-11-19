// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/web_content_area/java_script_dialogs/java_script_alert_overlay_mediator.h"

#include "base/strings/sys_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#import "ios/chrome/browser/overlays/public/overlay_request.h"
#import "ios/chrome/browser/overlays/public/web_content_area/java_script_alert_overlay.h"
#import "ios/chrome/browser/ui/alert_view_controller/alert_action.h"
#import "ios/chrome/browser/ui/alert_view_controller/test/fake_alert_consumer.h"
#import "ios/chrome/browser/ui/dialogs/java_script_dialog_blocking_state.h"
#import "ios/chrome/browser/ui/overlays/common/alerts/test/alert_overlay_mediator_test.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "testing/gtest_mac.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using JavaScriptAlertOverlayMediatorTest = AlertOverlayMediatorTest;

// Tests that the consumer values are set correctly for alerts from the main
// frame.
TEST_F(JavaScriptAlertOverlayMediatorTest, AlertSetupMainFrame) {
  web::TestWebState web_state;
  const GURL kUrl("https://chromium.test");
  const std::string kMessage("Message");
  std::unique_ptr<OverlayRequest> request =
      OverlayRequest::CreateWithConfig<JavaScriptAlertOverlayRequestConfig>(
          JavaScriptDialogSource(&web_state, kUrl, /*is_main_frame=*/true),
          kMessage);
  SetMediator(
      [[JavaScriptAlertOverlayMediator alloc] initWithRequest:request.get()]);

  // Verify the consumer values.
  EXPECT_NSEQ(base::SysUTF8ToNSString(kMessage), consumer().title);
  EXPECT_EQ(0U, consumer().textFieldConfigurations.count);
  ASSERT_EQ(1U, consumer().actions.count);
  EXPECT_EQ(UIAlertActionStyleDefault, consumer().actions[0].style);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_OK), consumer().actions[0].title);
}

// Tests that the consumer values are set correctly for alerts from an iframe.
TEST_F(JavaScriptAlertOverlayMediatorTest, AlertSetupIFrame) {
  web::TestWebState web_state;
  const GURL kUrl("https://chromium.test");
  const std::string kMessage("Message");
  std::unique_ptr<OverlayRequest> request =
      OverlayRequest::CreateWithConfig<JavaScriptAlertOverlayRequestConfig>(
          JavaScriptDialogSource(&web_state, kUrl, /*is_main_frame=*/false),
          kMessage);
  SetMediator(
      [[JavaScriptAlertOverlayMediator alloc] initWithRequest:request.get()]);

  // Verify the consumer values.
  EXPECT_NSEQ(base::SysUTF8ToNSString(kMessage), consumer().message);
  EXPECT_EQ(0U, consumer().textFieldConfigurations.count);
  ASSERT_EQ(1U, consumer().actions.count);
  EXPECT_EQ(UIAlertActionStyleDefault, consumer().actions[0].style);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_OK), consumer().actions[0].title);
}

// Tests that the consumer values are set correctly for alerts when the blocking
// option is shown.
TEST_F(JavaScriptAlertOverlayMediatorTest, AlertSetupWithBlockingOption) {
  web::TestWebState web_state;
  JavaScriptDialogBlockingState::CreateForWebState(&web_state);
  JavaScriptDialogBlockingState::FromWebState(&web_state)
      ->JavaScriptDialogWasShown();

  const GURL kUrl("https://chromium.test");
  const std::string kMessage("Message");
  std::unique_ptr<OverlayRequest> request =
      OverlayRequest::CreateWithConfig<JavaScriptAlertOverlayRequestConfig>(
          JavaScriptDialogSource(&web_state, kUrl, /*is_main_frame=*/true),
          kMessage);
  SetMediator(
      [[JavaScriptAlertOverlayMediator alloc] initWithRequest:request.get()]);

  // Verify the consumer values.
  EXPECT_NSEQ(base::SysUTF8ToNSString(kMessage), consumer().title);
  EXPECT_EQ(0U, consumer().textFieldConfigurations.count);
  ASSERT_EQ(2U, consumer().actions.count);
  EXPECT_EQ(UIAlertActionStyleDefault, consumer().actions[0].style);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_OK), consumer().actions[0].title);
  EXPECT_EQ(UIAlertActionStyleDestructive, consumer().actions[1].style);
  NSString* action_title =
      l10n_util::GetNSString(IDS_IOS_JAVA_SCRIPT_DIALOG_BLOCKING_BUTTON_TEXT);
  EXPECT_NSEQ(action_title, consumer().actions[1].title);
}
