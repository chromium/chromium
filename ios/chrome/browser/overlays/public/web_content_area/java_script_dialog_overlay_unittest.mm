// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/public/web_content_area/java_script_dialog_overlay.h"

#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/overlays/public/overlay_request.h"
#import "ios/chrome/browser/overlays/public/overlay_response.h"
#import "ios/chrome/browser/overlays/public/web_content_area/alert_constants.h"
#import "ios/chrome/browser/overlays/public/web_content_area/alert_overlay.h"
#import "ios/chrome/browser/ui/dialogs/java_script_dialog_blocking_state.h"
#import "ios/chrome/browser/ui/elements/text_field_configuration.h"
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
using java_script_dialog_overlays::JavaScriptDialogRequest;
using java_script_dialog_overlays::JavaScriptDialogResponse;

// Test fixture for JavaScript dialog overlays.
class JavaScriptDialogOverlayTest
    : public testing::TestWithParam<web::JavaScriptDialogType> {
 protected:
  JavaScriptDialogOverlayTest()
      : url_("http://www.chromium.test"),
        message_(@"message"),
        default_text_field_value_(@"default_text_field_value") {}

  std::unique_ptr<OverlayRequest> CreateRequest(bool is_main_frame = true) {
    return OverlayRequest::CreateWithConfig<JavaScriptDialogRequest>(
        GetParam(), &web_state_, url_, is_main_frame, message_,
        default_text_field_value_);
  }

  // Whether dialogs for this fixture's type have a cancel button.
  bool DialogHasCancelButton() {
    return GetParam() != web::JAVASCRIPT_DIALOG_TYPE_ALERT;
  }

  // Whether dialogs for this fixture's type have a prompt.
  bool DialogHasTextField() {
    return GetParam() == web::JAVASCRIPT_DIALOG_TYPE_PROMPT;
  }

  web::FakeWebState web_state_;
  GURL url_;
  NSString* message_ = nil;
  NSString* default_text_field_value_ = nil;
};

// Tests that the alert config's values are set correctly for dialogs from the
// main frame.
TEST_P(JavaScriptDialogOverlayTest, MainFrameDialogTitleAndMessage) {
  std::unique_ptr<OverlayRequest> request = CreateRequest();
  AlertRequest* config = request->GetConfig<AlertRequest>();
  ASSERT_TRUE(config);

  // Check the title and message strings.
  EXPECT_NSEQ(message_, config->title());
  EXPECT_FALSE(config->message());
}

// Tests that the alert config's values are set correctly for dialogs from an
// iframe.
TEST_P(JavaScriptDialogOverlayTest, IFrameDialogTitleAndMessage) {
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

// Tests that the text field configs are set up correctly.
TEST_P(JavaScriptDialogOverlayTest, TextFieldConfigSetup) {
  std::unique_ptr<OverlayRequest> request = CreateRequest();
  AlertRequest* config = request->GetConfig<AlertRequest>();
  ASSERT_TRUE(config);

  // Check the text fields.
  TextFieldConfiguration* text_field_config =
      [config->text_field_configs() firstObject];
  if (DialogHasTextField()) {
    EXPECT_TRUE(text_field_config);
    EXPECT_FALSE(text_field_config.placeholder);
    EXPECT_NSEQ(default_text_field_value_, text_field_config.text);
    EXPECT_NSEQ(kJavaScriptDialogTextFieldAccessibilityIdentifier,
                text_field_config.accessibilityIdentifier);
  } else {
    EXPECT_FALSE(text_field_config);
  }
}

// Tests that the button configs are set up correctly.
TEST_P(JavaScriptDialogOverlayTest, ButtonConfigSetup) {
  std::unique_ptr<OverlayRequest> request = CreateRequest();
  AlertRequest* config = request->GetConfig<AlertRequest>();
  ASSERT_TRUE(config);

  // Check the button configs.
  bool has_cancel_button = DialogHasCancelButton();
  const std::vector<ButtonConfig>& button_configs = config->button_configs();
  ASSERT_EQ(has_cancel_button ? 2U : 1U, button_configs.size());
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_OK), button_configs[0].title);
  EXPECT_EQ(UIAlertActionStyleDefault, button_configs[0].style);
  if (has_cancel_button) {
    EXPECT_NSEQ(l10n_util::GetNSString(IDS_CANCEL), button_configs[1].title);
    EXPECT_EQ(UIAlertActionStyleCancel, button_configs[1].style);
  }
}

// Tests that the blocking option is successfully added.
TEST_P(JavaScriptDialogOverlayTest, BlockingOptionSetup) {
  JavaScriptDialogBlockingState::CreateForWebState(&web_state_);
  JavaScriptDialogBlockingState::FromWebState(&web_state_)
      ->JavaScriptDialogWasShown();

  std::unique_ptr<OverlayRequest> request = CreateRequest();
  AlertRequest* config = request->GetConfig<AlertRequest>();
  ASSERT_TRUE(config);

  // Check the dialog suppression button config.
  const std::vector<ButtonConfig>& button_configs = config->button_configs();
  ASSERT_FALSE(button_configs.empty());
  NSString* blocking_option_title =
      l10n_util::GetNSString(IDS_IOS_JAVA_SCRIPT_DIALOG_BLOCKING_BUTTON_TEXT);
  EXPECT_NSEQ(blocking_option_title, button_configs.back().title);
  EXPECT_EQ(UIAlertActionStyleDestructive, button_configs.back().style);
}

// Tests that an alert response after tapping the OK button is correctly
// converted to a JavaScriptDialogResponse.
TEST_P(JavaScriptDialogOverlayTest, ResponseConversionOk) {
  std::unique_ptr<OverlayRequest> request = CreateRequest();
  AlertRequest* config = request->GetConfig<AlertRequest>();
  ASSERT_TRUE(config);

  // Simulate a response where the OK button is tapped.
  NSString* user_input = @"user_input";
  std::unique_ptr<OverlayResponse> alert_response =
      OverlayResponse::CreateWithInfo<AlertResponse>(
          /*tapped_button_index=*/0, @[ user_input ]);

  // Since the OK button is tapped, the kConfirm action should be used and the
  // text field input should be supplied to the JavaScriptDialogResponse.
  std::unique_ptr<OverlayResponse> response =
      config->response_converter().Run(std::move(alert_response));
  ASSERT_TRUE(response.get());

  JavaScriptDialogResponse* dialog_response =
      response->GetInfo<JavaScriptDialogResponse>();
  ASSERT_TRUE(dialog_response);

  EXPECT_EQ(JavaScriptDialogResponse::Action::kConfirm,
            dialog_response->action());
  EXPECT_NSEQ(user_input, dialog_response->user_input());
}

// Tests that an alert response after tapping the Cancel button is correctly
// converted to a JavaScriptDialogResponse.
TEST_P(JavaScriptDialogOverlayTest, ResponseConversionCancel) {
  if (!DialogHasCancelButton())
    return;

  std::unique_ptr<OverlayRequest> request = CreateRequest();
  AlertRequest* config = request->GetConfig<AlertRequest>();
  ASSERT_TRUE(config);

  // Simulate a response where the Cancel button is tapped.
  NSString* user_input = @"user_input";
  std::unique_ptr<OverlayResponse> alert_response =
      OverlayResponse::CreateWithInfo<AlertResponse>(
          /*tapped_button_index=*/1, @[ user_input ]);

  // Since the Cancel button is tapped, the kCancel action should be used and
  // the text field input should be nil.
  std::unique_ptr<OverlayResponse> response =
      config->response_converter().Run(std::move(alert_response));
  ASSERT_TRUE(response.get());

  JavaScriptDialogResponse* dialog_response =
      response->GetInfo<JavaScriptDialogResponse>();
  ASSERT_TRUE(dialog_response);

  EXPECT_EQ(JavaScriptDialogResponse::Action::kCancel,
            dialog_response->action());
  EXPECT_FALSE(dialog_response->user_input());
}

// Tests that an alert response after tapping the blocking option is correctly
// converted to a JavaScriptDialogResponse.
TEST_P(JavaScriptDialogOverlayTest, ResponseConversionBlockDialogs) {
  JavaScriptDialogBlockingState::CreateForWebState(&web_state_);
  JavaScriptDialogBlockingState::FromWebState(&web_state_)
      ->JavaScriptDialogWasShown();

  std::unique_ptr<OverlayRequest> request = CreateRequest();
  AlertRequest* config = request->GetConfig<AlertRequest>();
  ASSERT_TRUE(config);

  // Simulate a response where the blocking option is tapped.
  NSString* user_input = @"user_input";
  size_t blocking_option_button_index = config->button_configs().size() - 1;
  std::unique_ptr<OverlayResponse> alert_response =
      OverlayResponse::CreateWithInfo<AlertResponse>(
          blocking_option_button_index, @[ user_input ]);

  // Since the Cancel button is tapped, the kBlockDialogs action should be used
  // and the text field input should be nil.
  std::unique_ptr<OverlayResponse> response =
      config->response_converter().Run(std::move(alert_response));
  ASSERT_TRUE(response.get());

  JavaScriptDialogResponse* dialog_response =
      response->GetInfo<JavaScriptDialogResponse>();
  ASSERT_TRUE(dialog_response);

  EXPECT_EQ(JavaScriptDialogResponse::Action::kBlockDialogs,
            dialog_response->action());
  EXPECT_FALSE(dialog_response->user_input());
}

INSTANTIATE_TEST_SUITE_P(/* No InstantiationName */,
                         JavaScriptDialogOverlayTest,
                         testing::Values(web::JAVASCRIPT_DIALOG_TYPE_ALERT,
                                         web::JAVASCRIPT_DIALOG_TYPE_CONFIRM,
                                         web::JAVASCRIPT_DIALOG_TYPE_PROMPT));
