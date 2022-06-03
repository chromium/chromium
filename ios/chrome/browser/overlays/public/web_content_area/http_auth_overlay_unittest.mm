// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/public/web_content_area/http_auth_overlay.h"

#include "base/strings/sys_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/overlays/public/overlay_request.h"
#import "ios/chrome/browser/overlays/public/overlay_response.h"
#import "ios/chrome/browser/overlays/public/web_content_area/alert_overlay.h"
#import "ios/chrome/browser/overlays/public/web_content_area/http_auth_overlay.h"
#import "ios/chrome/browser/ui/elements/text_field_configuration.h"
#include "ios/chrome/grit/ios_strings.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using alert_overlays::AlertRequest;
using alert_overlays::AlertResponse;
using alert_overlays::ButtonConfig;

// Test fixture for HTTP auth overlays.
class HttpAuthOverlayTest : public PlatformTest {
 public:
  HttpAuthOverlayTest()
      : url_("http://www.chromium.test"),
        message_("Message"),
        default_user_text_("Default Text"),
        request_(OverlayRequest::CreateWithConfig<HTTPAuthOverlayRequestConfig>(
            url_,
            message_,
            default_user_text_)) {}

  AlertRequest* alert_config() const {
    return request_->GetConfig<AlertRequest>();
  }

 protected:
  const GURL url_;
  const std::string message_;
  const std::string default_user_text_;
  std::unique_ptr<OverlayRequest> request_;
};

// Tests that the consumer values are set correctly for HTTP auth dialogs.
TEST_F(HttpAuthOverlayTest, AlertSetup) {
  AlertRequest* config = alert_config();

  // There is no title for the HTTP auth dialog, only a message.
  EXPECT_NSEQ(base::SysUTF8ToNSString(message_), config->message());

  // There should be a username text field and a password text field.
  EXPECT_EQ(2U, config->text_field_configs().count);
  TextFieldConfiguration* username_config = config->text_field_configs()[0];
  TextFieldConfiguration* password_config = config->text_field_configs()[1];

  EXPECT_NSEQ(base::SysUTF8ToNSString(default_user_text_),
              username_config.text);
  NSString* user_placeholer =
      l10n_util::GetNSString(IDS_IOS_HTTP_LOGIN_DIALOG_USERNAME_PLACEHOLDER);
  EXPECT_NSEQ(user_placeholer, username_config.placeholder);

  EXPECT_FALSE(password_config.text);
  NSString* password_placeholder =
      l10n_util::GetNSString(IDS_IOS_HTTP_LOGIN_DIALOG_PASSWORD_PLACEHOLDER);
  EXPECT_NSEQ(password_placeholder, password_config.placeholder);

  // There should be an OK button and a Cancel button.
  ASSERT_EQ(2U, config->button_configs().size());
  const ButtonConfig& ok_button_config = config->button_configs()[0];
  const ButtonConfig& cancel_button_config = config->button_configs()[1];

  EXPECT_EQ(UIAlertActionStyleDefault, ok_button_config.style);
  NSString* sign_in_label =
      l10n_util::GetNSStringWithFixup(IDS_LOGIN_DIALOG_OK_BUTTON_LABEL);
  EXPECT_NSEQ(sign_in_label, ok_button_config.title);
  EXPECT_EQ(kHttpAuthSignInTappedActionName, ok_button_config.user_action_name);

  EXPECT_EQ(UIAlertActionStyleCancel, cancel_button_config.style);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_CANCEL), cancel_button_config.title);
  EXPECT_EQ(kHttpAuthCancelTappedActionName,
            cancel_button_config.user_action_name);
}

// Tests that an alert response after tapping the OK button successfully creates
// the correct response.
TEST_F(HttpAuthOverlayTest, ResponseConversionOk) {
  // Simulate a response where the OK button is tapped.
  std::unique_ptr<OverlayResponse> alert_response =
      OverlayResponse::CreateWithInfo<AlertResponse>(
          /*tapped_button_index=*/0, @[ @"username", @"password" ]);

  // Convert the response to the HTTP auth response.
  std::unique_ptr<OverlayResponse> response =
      alert_config()->response_converter().Run(std::move(alert_response));
  ASSERT_TRUE(response.get());
  HTTPAuthOverlayResponseInfo* info =
      response->GetInfo<HTTPAuthOverlayResponseInfo>();
  ASSERT_TRUE(info);
  EXPECT_EQ(std::string("username"), info->username());
  EXPECT_EQ(std::string("password"), info->password());
}

// Tests that an alert response after tapping the Cancel button is converted to
// a null HTTP auth response.
TEST_F(HttpAuthOverlayTest, ResponseConversionCancel) {
  // Simulate a response where the OK button is tapped.
  std::unique_ptr<OverlayResponse> alert_response =
      OverlayResponse::CreateWithInfo<AlertResponse>(
          /*tapped_button_index=*/1, @[ @"username", @"password" ]);

  // Since the cancel button is tapped, a null HTTP auth response should be
  // used.
  std::unique_ptr<OverlayResponse> response =
      alert_config()->response_converter().Run(std::move(alert_response));
  EXPECT_FALSE(response.get());
}
