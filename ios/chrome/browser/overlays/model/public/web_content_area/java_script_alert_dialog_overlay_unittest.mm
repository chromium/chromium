// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/model/public/web_content_area/java_script_alert_dialog_overlay.h"

#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/dialogs/ui_bundled/java_script_dialog_blocking_state.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request.h"
#import "ios/chrome/browser/overlays/model/public/overlay_response.h"
#import "ios/chrome/browser/overlays/model/public/web_content_area/alert_overlay.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

using alert_overlays::AlertRequest;
using alert_overlays::AlertResponse;
using alert_overlays::ButtonConfig;

namespace {
// Message string for dialog overaly request.
static NSString* kDialogMessage = @"message";
}  // namespace

// Test fixture for JavaScript alert dialog overlays.
class JavaScriptAlertDialogOverlayTest : public PlatformTest {
 protected:
  JavaScriptAlertDialogOverlayTest() {}

  std::unique_ptr<OverlayRequest> CreateRequest(bool is_main_frame = true) {
    return OverlayRequest::CreateWithConfig<JavaScriptAlertDialogRequest>(
        &web_state_, GURL("http://www.chromium.test"), is_main_frame,
        kDialogMessage);
  }

  web::FakeWebState web_state_;
};

// Tests that the alert config's values are set correctly for dialogs from the
// main frame.
TEST_F(JavaScriptAlertDialogOverlayTest, MainFrameDialogTitleAndMessage) {
  std::unique_ptr<OverlayRequest> request = CreateRequest();
  AlertRequest* config = request->GetConfig<AlertRequest>();
  ASSERT_TRUE(config);

  // Check the title and message strings.
  EXPECT_NSEQ(kDialogMessage, config->title());
  EXPECT_FALSE(config->message());
}

// Tests that the alert config's values are set correctly for dialogs from an
// iframe.
TEST_F(JavaScriptAlertDialogOverlayTest, IFrameDialogTitleAndMessage) {
  std::unique_ptr<OverlayRequest> request =
      CreateRequest(/*is_main_frame=*/false);
  AlertRequest* config = request->GetConfig<AlertRequest>();
  ASSERT_TRUE(config);

  // Check the title and message strings.
  NSString* iframe_title = l10n_util::GetNSString(
      IDS_JAVASCRIPT_MESSAGEBOX_TITLE_NONSTANDARD_URL_IFRAME);
  EXPECT_NSEQ(iframe_title, config->title());
  EXPECT_NSEQ(kDialogMessage, config->message());
}

// Tests that the alert dialog has no text field.
TEST_F(JavaScriptAlertDialogOverlayTest, TextFieldConfigSetup) {
  std::unique_ptr<OverlayRequest> alert_request = CreateRequest();
  AlertRequest* alert_config = alert_request->GetConfig<AlertRequest>();
  ASSERT_TRUE(alert_config);
  EXPECT_FALSE([alert_config->text_field_configs() firstObject]);
}

// Tests that the alert dialog buttons are set up correctly.
TEST_F(JavaScriptAlertDialogOverlayTest, ButtonConfigSetup) {
  std::unique_ptr<OverlayRequest> alert_request = CreateRequest();
  AlertRequest* alert_config = alert_request->GetConfig<AlertRequest>();
  ASSERT_TRUE(alert_config);
  const std::vector<std::vector<ButtonConfig>>& alert_button_configs =
      alert_config->button_configs();
  ASSERT_EQ(1U, alert_button_configs.size());
  ASSERT_EQ(1U, alert_button_configs[0].size());
  ButtonConfig button_config = alert_button_configs[0][0];
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_OK), button_config.title);
  EXPECT_EQ(UIAlertActionStyleDefault, button_config.style);
}

// Tests that the blocking option is successfully added.
TEST_F(JavaScriptAlertDialogOverlayTest, BlockingOptionSetup) {
  JavaScriptDialogBlockingState::CreateForWebState(&web_state_);
  JavaScriptDialogBlockingState::FromWebState(&web_state_)
      ->JavaScriptDialogWasShown();
  NSString* blocking_option_title =
      l10n_util::GetNSString(IDS_IOS_JAVA_SCRIPT_DIALOG_BLOCKING_BUTTON_TEXT);

  std::unique_ptr<OverlayRequest> alert_request = CreateRequest();
  AlertRequest* alert_config = alert_request->GetConfig<AlertRequest>();
  ASSERT_TRUE(alert_config);
  const std::vector<std::vector<ButtonConfig>>& alert_button_configs =
      alert_config->button_configs();
  ASSERT_FALSE(alert_button_configs.empty());
  ButtonConfig button_config = alert_button_configs.back()[0];
  EXPECT_NSEQ(blocking_option_title, button_config.title);
  EXPECT_EQ(UIAlertActionStyleDestructive, button_config.style);
}

// Tests that an alert is correctly converted to a JavaScriptAlertDialogResponse
// after tapping the OK button.
TEST_F(JavaScriptAlertDialogOverlayTest, ResponseConversionOk) {
  std::unique_ptr<OverlayRequest> request = CreateRequest();
  AlertRequest* config = request->GetConfig<AlertRequest>();
  ASSERT_TRUE(config);

  // Simulate a response where the OK button is tapped.
  std::unique_ptr<OverlayResponse> alert_response =
      OverlayResponse::CreateWithInfo<AlertResponse>(
          /*tapped_button_row_index=*/0, /*tapped_button_column_index=*/0,
          @[ @"" ]);

  std::unique_ptr<OverlayResponse> response =
      config->response_converter().Run(std::move(alert_response));
  ASSERT_TRUE(response.get());

  JavaScriptAlertDialogResponse* dialog_response =
      response->GetInfo<JavaScriptAlertDialogResponse>();
  ASSERT_TRUE(dialog_response);

  EXPECT_EQ(JavaScriptAlertDialogResponse::Action::kConfirm,
            dialog_response->action());
}

// Tests that an alert response after tapping the blocking option is correctly
// converted to a JavaScriptDialogResponse.
TEST_F(JavaScriptAlertDialogOverlayTest, ResponseConversionBlockDialogs) {
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
          blocking_option_button_index, 0, @[ @"" ]);

  std::unique_ptr<OverlayResponse> response =
      config->response_converter().Run(std::move(alert_response));
  ASSERT_TRUE(response.get());

  JavaScriptAlertDialogResponse* dialog_response =
      response->GetInfo<JavaScriptAlertDialogResponse>();
  ASSERT_TRUE(dialog_response);

  EXPECT_EQ(JavaScriptAlertDialogResponse::Action::kBlockDialogs,
            dialog_response->action());
}
