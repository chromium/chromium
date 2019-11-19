// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/web_content_area/http_auth_dialogs/http_auth_dialog_overlay_mediator.h"

#include "base/strings/sys_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#import "ios/chrome/browser/overlays/public/overlay_request.h"
#import "ios/chrome/browser/overlays/public/overlay_response.h"
#import "ios/chrome/browser/overlays/public/web_content_area/http_auth_overlay.h"
#import "ios/chrome/browser/ui/alert_view_controller/alert_action.h"
#import "ios/chrome/browser/ui/alert_view_controller/test/fake_alert_consumer.h"
#import "ios/chrome/browser/ui/dialogs/java_script_dialog_blocking_state.h"
#import "ios/chrome/browser/ui/elements/text_field_configuration.h"
#import "ios/chrome/browser/ui/overlays/common/alerts/test/alert_overlay_mediator_test.h"
#include "ios/chrome/grit/ios_strings.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class HTTPAuthDialogOverlayMediatorTest : public AlertOverlayMediatorTest {
 public:
  HTTPAuthDialogOverlayMediatorTest()
      : message_("Message"),
        default_user_text_("Default Text"),
        request_(OverlayRequest::CreateWithConfig<HTTPAuthOverlayRequestConfig>(
            message_,
            default_user_text_)) {
    SetMediator(
        [[HTTPAuthDialogOverlayMediator alloc] initWithRequest:request_.get()]);
  }

 protected:
  const std::string message_;
  const std::string default_user_text_;
  std::unique_ptr<OverlayRequest> request_;
};

// Tests that the consumer values are set correctly for confirmations.
TEST_F(HTTPAuthDialogOverlayMediatorTest, AlertSetup) {
  // Verify the consumer values.
  EXPECT_NSEQ(base::SysUTF8ToNSString(message_), consumer().message);
  EXPECT_EQ(2U, consumer().textFieldConfigurations.count);
  EXPECT_NSEQ(base::SysUTF8ToNSString(default_user_text_),
              consumer().textFieldConfigurations[0].text);
  NSString* user_placeholer =
      l10n_util::GetNSString(IDS_IOS_HTTP_LOGIN_DIALOG_USERNAME_PLACEHOLDER);
  EXPECT_NSEQ(user_placeholer,
              consumer().textFieldConfigurations[0].placeholder);
  EXPECT_FALSE(!!consumer().textFieldConfigurations[1].text);
  NSString* password_placeholder =
      l10n_util::GetNSString(IDS_IOS_HTTP_LOGIN_DIALOG_PASSWORD_PLACEHOLDER);
  EXPECT_NSEQ(password_placeholder,
              consumer().textFieldConfigurations[1].placeholder);
  ASSERT_EQ(2U, consumer().actions.count);
  EXPECT_EQ(UIAlertActionStyleDefault, consumer().actions[0].style);
  NSString* sign_in_label =
      l10n_util::GetNSStringWithFixup(IDS_LOGIN_DIALOG_OK_BUTTON_LABEL);
  EXPECT_NSEQ(sign_in_label, consumer().actions[0].title);
  EXPECT_EQ(UIAlertActionStyleCancel, consumer().actions[1].style);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_CANCEL), consumer().actions[1].title);
}
