// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/model/public/web_content_area/java_script_prompt_dialog_overlay.h"

#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/dialogs/ui_bundled/java_script_dialog_blocking_state.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request.h"
#import "ios/chrome/browser/overlays/model/public/overlay_response.h"
#import "ios/chrome/browser/overlays/model/public/web_content_area/alert_constants.h"
#import "ios/chrome/browser/overlays/model/public/web_content_area/alert_overlay.h"
#import "ios/chrome/browser/shared/ui/elements/text_field_configuration.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

using alert_overlays::AlertRequest;
using alert_overlays::AlertResponse;
using alert_overlays::ButtonConfig;

// Test fixture for JavaScript prompt dialog overlays.
class JavaScriptPromptDialogOverlayTest : public PlatformTest {
 protected:
  JavaScriptPromptDialogOverlayTest()
      : url_("http://www.chromium.test"),
        message_(@"message"),
        default_text_field_value_(@"default_text_field_value") {}

  std::unique_ptr<OverlayRequest> CreateRequest(bool is_main_frame = true) {
    return OverlayRequest::CreateWithConfig<JavaScriptPromptDialogRequest>(
        &web_state_, url_, is_main_frame, message_, default_text_field_value_);
  }

  web::FakeWebState web_state_;
  GURL url_;
  NSString* message_ = nil;
  NSString* default_text_field_value_ = nil;
};

// Tests that the alert config's values are set correctly for dialogs from the
// main frame.
TEST_F(JavaScriptPromptDialogOverlayTest, MainFrameDialogTitleAndMessage) {
  std::unique_ptr<OverlayRequest> request = CreateRequest();
  AlertRequest* config = request->GetConfig<AlertRequest>();
  ASSERT_TRUE(config);

  // Check the title and message strings.
  EXPECT_NSEQ(message_, config->title());
  EXPECT_FALSE(config->message());
}

// Tests that the alert config's values are set correctly for dialogs from an
// iframe.
TEST_F(JavaScriptPromptDialogOverlayTest, IFrameDialogTitleAndMessage) {
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

// Tests that the prompt dialog has a single text field.
TEST_F(JavaScriptPromptDialogOverlayTest, TextFieldConfigSetup) {
  std::unique_ptr<OverlayRequest> prompt_request = CreateRequest();
  AlertRequest* prompt_config = prompt_request->GetConfig<AlertRequest>();
  ASSERT_TRUE(prompt_config);
  ASSERT_EQ(1ul, [prompt_config->text_field_configs() count]);
  TextFieldConfiguration* text_field_config =
      [prompt_config->text_field_configs() firstObject];
  EXPECT_TRUE(text_field_config);
  EXPECT_FALSE(text_field_config.placeholder);
  EXPECT_NSEQ(default_text_field_value_, text_field_config.text);
  EXPECT_NSEQ(kJavaScriptDialogTextFieldAccessibilityIdentifier,
              text_field_config.accessibilityIdentifier);
}

// Tests that the prompt dialog buttons are set up correctly.
TEST_F(JavaScriptPromptDialogOverlayTest, ButtonConfigSetup) {
  std::unique_ptr<OverlayRequest> prompt_request = CreateRequest();
  AlertRequest* prompt_config = prompt_request->GetConfig<AlertRequest>();
  ASSERT_TRUE(prompt_config);
  const std::vector<std::vector<ButtonConfig>>& prompt_button_configs =
      prompt_config->button_configs();
  ASSERT_EQ(2U, prompt_button_configs.size());
  ButtonConfig ok_button = prompt_button_configs[0][0];
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_OK), ok_button.title);
  EXPECT_EQ(UIAlertActionStyleDefault, ok_button.style);
  ButtonConfig cancel_button = prompt_button_configs[1][0];
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_CANCEL), cancel_button.title);
  EXPECT_EQ(UIAlertActionStyleCancel, cancel_button.style);
}

// Tests that the blocking option is successfully added.
TEST_F(JavaScriptPromptDialogOverlayTest, BlockingOptionSetup) {
  JavaScriptDialogBlockingState::CreateForWebState(&web_state_);
  JavaScriptDialogBlockingState::FromWebState(&web_state_)
      ->JavaScriptDialogWasShown();
  NSString* blocking_option_title =
      l10n_util::GetNSString(IDS_IOS_JAVA_SCRIPT_DIALOG_BLOCKING_BUTTON_TEXT);

  std::unique_ptr<OverlayRequest> prompt_request = CreateRequest();
  AlertRequest* prompt_config = prompt_request->GetConfig<AlertRequest>();
  ASSERT_TRUE(prompt_config);
  const std::vector<std::vector<ButtonConfig>>& prompt_button_configs =
      prompt_config->button_configs();
  ASSERT_FALSE(prompt_button_configs.empty());
  ButtonConfig button = prompt_button_configs.back()[0];
  EXPECT_NSEQ(blocking_option_title, button.title);
  EXPECT_EQ(UIAlertActionStyleDestructive, button.style);
}

// Tests that a prompt alert is correctly converted to a
// JavaScriptPromptDialogResponse after tapping the OK button.
TEST_F(JavaScriptPromptDialogOverlayTest, PromptResponseConversionOk) {
  std::unique_ptr<OverlayRequest> request = CreateRequest();
  AlertRequest* config = request->GetConfig<AlertRequest>();
  ASSERT_TRUE(config);

  // Simulate a response where the OK button is tapped.
  NSString* user_input = @"user_input";
  std::unique_ptr<OverlayResponse> alert_response =
      OverlayResponse::CreateWithInfo<AlertResponse>(
          /*tapped_button_row_index=*/0,
          /*tapped_button_column_index=*/0, @[ user_input ]);

  // Since the OK button is tapped, the kConfirm action should be used and the
  // text field input should be supplied to the JavaScriptAlertDialogResponse.
  std::unique_ptr<OverlayResponse> response =
      config->response_converter().Run(std::move(alert_response));
  ASSERT_TRUE(response.get());

  JavaScriptPromptDialogResponse* dialog_response =
      response->GetInfo<JavaScriptPromptDialogResponse>();
  ASSERT_TRUE(dialog_response);

  EXPECT_EQ(JavaScriptPromptDialogResponse::Action::kConfirm,
            dialog_response->action());
  EXPECT_NSEQ(user_input, dialog_response->user_input());
}

// Tests that a prompt response is correctly converted to a
// JavaScriptPromptDialogResponse after tapping the Cancel button.
TEST_F(JavaScriptPromptDialogOverlayTest, ResponseConversionCancel) {
  std::unique_ptr<OverlayRequest> request = CreateRequest();
  AlertRequest* config = request->GetConfig<AlertRequest>();
  ASSERT_TRUE(config);

  // Simulate a response where the Cancel button is tapped.
  NSString* user_input = @"user_input";
  std::unique_ptr<OverlayResponse> alert_response =
      OverlayResponse::CreateWithInfo<AlertResponse>(
          /*tapped_button_row_index=*/1,
          /*tapped_button_column_index=*/0, @[ user_input ]);

  std::unique_ptr<OverlayResponse> response =
      config->response_converter().Run(std::move(alert_response));
  ASSERT_TRUE(response.get());

  JavaScriptPromptDialogResponse* dialog_response =
      response->GetInfo<JavaScriptPromptDialogResponse>();
  ASSERT_TRUE(dialog_response);

  EXPECT_EQ(JavaScriptPromptDialogResponse::Action::kCancel,
            dialog_response->action());
  // Since the Cancel button is tapped the text field input should be nil.
  EXPECT_FALSE(dialog_response->user_input());
}

// Tests that an alert response after tapping the blocking option is correctly
// converted to a JavaScriptDialogResponse.
TEST_F(JavaScriptPromptDialogOverlayTest, ResponseConversionBlockDialogs) {
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
          /*tapped_button_row_index=*/blocking_option_button_index,
          /*tapped_button_column_index=*/0, @[ @"user_input" ]);

  std::unique_ptr<OverlayResponse> response =
      config->response_converter().Run(std::move(alert_response));
  ASSERT_TRUE(response.get());

  JavaScriptPromptDialogResponse* dialog_response =
      response->GetInfo<JavaScriptPromptDialogResponse>();
  ASSERT_TRUE(dialog_response);

  // Since the Block button is tapped, the kBlockDialogs action should be used
  // and the text field input should be nil.
  EXPECT_EQ(JavaScriptPromptDialogResponse::Action::kBlockDialogs,
            dialog_response->action());
  EXPECT_FALSE(dialog_response->user_input());
}
