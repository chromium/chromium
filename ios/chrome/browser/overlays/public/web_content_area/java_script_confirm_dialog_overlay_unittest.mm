// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/public/web_content_area/java_script_confirm_dialog_overlay.h"

#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/overlays/public/overlay_request.h"
#import "ios/chrome/browser/overlays/public/overlay_response.h"
#import "ios/chrome/browser/overlays/public/web_content_area/alert_overlay.h"
#import "ios/chrome/browser/ui/dialogs/java_script_dialog_blocking_state.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using alert_overlays::AlertRequest;
using alert_overlays::AlertResponse;
using alert_overlays::ButtonConfig;

// Test fixture for JavaScript confirmation dialog overlays.
class JavaScriptConfirmDialogOverlayTest : public PlatformTest {
 protected:
  JavaScriptConfirmDialogOverlayTest()
      : url_("http://www.chromium.test"), message_(@"message") {}

  std::unique_ptr<OverlayRequest> CreateRequest(bool is_main_frame = true) {
    return OverlayRequest::CreateWithConfig<JavaScriptConfirmDialogRequest>(
        &web_state_, url_, is_main_frame, message_);
  }

  web::FakeWebState web_state_;
  GURL url_;
  NSString* message_ = nil;
};

// Tests that the alert config's values are set correctly for dialogs from the
// main frame.
TEST_F(JavaScriptConfirmDialogOverlayTest, MainFrameDialogTitleAndMessage) {
  std::unique_ptr<OverlayRequest> request = CreateRequest();
  AlertRequest* config = request->GetConfig<AlertRequest>();
  ASSERT_TRUE(config);

  // Check the title and message strings.
  EXPECT_NSEQ(message_, config->title());
  EXPECT_FALSE(config->message());
}

// Tests that the alert config's values are set correctly for dialogs from an
// iframe.
TEST_F(JavaScriptConfirmDialogOverlayTest, IFrameDialogTitleAndMessage) {
  std::unique_ptr<OverlayRequest> request =
      CreateRequest(/*is_main_frame=*/false);
  AlertRequest* config = request->GetConfig<AlertRequest>();
  ASSERT_TRUE(config);

  // Check the title and message strings.
  NSString* iframe_title = l10n_util::GetNSString(
      IDS_JAVASCRIPT_MESSAGEBOX_TITLE_NONSTANDARD_URL_IFRAME);
  EXPECT_NSEQ(iframe_title, config->title());
  EXPECT_NSEQ(message_, config->message());
}

// Tests that the confirm dialog has no text field.
TEST_F(JavaScriptConfirmDialogOverlayTest, TextFieldConfigSetup) {
  std::unique_ptr<OverlayRequest> confirm_request = CreateRequest();
  AlertRequest* confirm_config = confirm_request->GetConfig<AlertRequest>();
  ASSERT_TRUE(confirm_config);
  EXPECT_FALSE([confirm_config->text_field_configs() firstObject]);
}

// Tests that the confirmation dialog buttons are set up correctly.
TEST_F(JavaScriptConfirmDialogOverlayTest, ButtonConfigSetup) {
  std::unique_ptr<OverlayRequest> confirm_request = CreateRequest();
  AlertRequest* confirm_config = confirm_request->GetConfig<AlertRequest>();
  ASSERT_TRUE(confirm_config);
  const std::vector<ButtonConfig>& confirm_button_configs =
      confirm_config->button_configs();
  ASSERT_EQ(2U, confirm_button_configs.size());
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_OK), confirm_button_configs[0].title);
  EXPECT_EQ(UIAlertActionStyleDefault, confirm_button_configs[0].style);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_CANCEL),
              confirm_button_configs[1].title);
  EXPECT_EQ(UIAlertActionStyleCancel, confirm_button_configs[1].style);
}

// Tests that the blocking option is successfully added.
TEST_F(JavaScriptConfirmDialogOverlayTest, BlockingOptionSetup) {
  JavaScriptDialogBlockingState::CreateForWebState(&web_state_);
  JavaScriptDialogBlockingState::FromWebState(&web_state_)
      ->JavaScriptDialogWasShown();
  NSString* blocking_option_title =
      l10n_util::GetNSString(IDS_IOS_JAVA_SCRIPT_DIALOG_BLOCKING_BUTTON_TEXT);

  std::unique_ptr<OverlayRequest> confirm_request = CreateRequest();
  AlertRequest* confirm_config = confirm_request->GetConfig<AlertRequest>();
  ASSERT_TRUE(confirm_config);
  const std::vector<ButtonConfig>& confirm_button_configs =
      confirm_config->button_configs();
  ASSERT_FALSE(confirm_button_configs.empty());
  EXPECT_NSEQ(blocking_option_title, confirm_button_configs.back().title);
  EXPECT_EQ(UIAlertActionStyleDestructive, confirm_button_configs.back().style);
}

// Tests that a confirmation alert is correctly converted to a
// JavaScriptConfirmDialogResponse after tapping the OK button.
TEST_F(JavaScriptConfirmDialogOverlayTest, ResponseConversionOk) {
  std::unique_ptr<OverlayRequest> request = CreateRequest();
  AlertRequest* config = request->GetConfig<AlertRequest>();
  ASSERT_TRUE(config);

  // Simulate a response where the OK button is tapped.
  std::unique_ptr<OverlayResponse> alert_response =
      OverlayResponse::CreateWithInfo<AlertResponse>(
          /*tapped_button_index=*/0, @[ @"" ]);

  std::unique_ptr<OverlayResponse> response =
      config->response_converter().Run(std::move(alert_response));
  ASSERT_TRUE(response.get());

  JavaScriptConfirmDialogResponse* dialog_response =
      response->GetInfo<JavaScriptConfirmDialogResponse>();
  ASSERT_TRUE(dialog_response);

  EXPECT_EQ(JavaScriptConfirmDialogResponse::Action::kConfirm,
            dialog_response->action());
}

// Tests that a confirmation alert response is correctly converted to a
// JavaScriptConfirmDialogResponse after tapping the Cancel button.
TEST_F(JavaScriptConfirmDialogOverlayTest, ResponseConversionCancel) {
  std::unique_ptr<OverlayRequest> request = CreateRequest();
  AlertRequest* config = request->GetConfig<AlertRequest>();
  ASSERT_TRUE(config);

  // Simulate a response where the Cancel button is tapped.
  std::unique_ptr<OverlayResponse> alert_response =
      OverlayResponse::CreateWithInfo<AlertResponse>(
          /*tapped_button_index=*/1, @[ @"" ]);

  std::unique_ptr<OverlayResponse> response =
      config->response_converter().Run(std::move(alert_response));
  ASSERT_TRUE(response.get());

  JavaScriptConfirmDialogResponse* dialog_response =
      response->GetInfo<JavaScriptConfirmDialogResponse>();
  ASSERT_TRUE(dialog_response);

  EXPECT_EQ(JavaScriptConfirmDialogResponse::Action::kCancel,
            dialog_response->action());
}

// Tests that an alert response after tapping the blocking option is correctly
// converted to a JavaScriptDialogResponse.
TEST_F(JavaScriptConfirmDialogOverlayTest,
       ConfirmationResponseConversionBlockDialogs) {
  JavaScriptDialogBlockingState::CreateForWebState(&web_state_);
  JavaScriptDialogBlockingState::FromWebState(&web_state_)
      ->JavaScriptDialogWasShown();

  std::unique_ptr<OverlayRequest> request = CreateRequest();
  AlertRequest* config = request->GetConfig<AlertRequest>();
  ASSERT_TRUE(config);

  // Simulate a response where the blocking option is tapped.
  size_t blocking_option_button_index = config->button_configs().size() - 1;
  std::unique_ptr<OverlayResponse> alert_response =
      OverlayResponse::CreateWithInfo<AlertResponse>(
          blocking_option_button_index, @[ @"" ]);

  std::unique_ptr<OverlayResponse> response =
      config->response_converter().Run(std::move(alert_response));
  ASSERT_TRUE(response.get());

  JavaScriptConfirmDialogResponse* dialog_response =
      response->GetInfo<JavaScriptConfirmDialogResponse>();
  ASSERT_TRUE(dialog_response);

  EXPECT_EQ(JavaScriptConfirmDialogResponse::Action::kBlockDialogs,
            dialog_response->action());
}
