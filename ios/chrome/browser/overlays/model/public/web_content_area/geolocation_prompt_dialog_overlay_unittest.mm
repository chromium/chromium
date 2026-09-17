// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/model/public/web_content_area/geolocation_prompt_dialog_overlay.h"

#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request.h"
#import "ios/chrome/browser/overlays/model/public/overlay_response.h"
#import "ios/chrome/browser/overlays/model/public/web_content_area/alert_overlay.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"

using alert_overlays::AlertRequest;
using alert_overlays::AlertResponse;
using alert_overlays::ButtonConfig;

// Test fixture for Geolocation prompt dialog overlays.
class GeolocationPromptDialogOverlayTest : public PlatformTest {
 protected:
  std::unique_ptr<OverlayRequest> CreateRequest() {
    return OverlayRequest::CreateWithConfig<GeolocationPromptDialogRequest>();
  }
};

// Tests that the alert config is set correctly with title, detail message, and
// buttons.
TEST_F(GeolocationPromptDialogOverlayTest, DialogStringsAndButtons) {
  std::unique_ptr<OverlayRequest> request = CreateRequest();
  AlertRequest* config = request->GetConfig<AlertRequest>();
  ASSERT_TRUE(config);

  NSString* expected_title =
      l10n_util::GetNSString(IDS_IOS_LOCATION_SETTINGS_HELP_TITLE);
  NSString* expected_message =
      l10n_util::GetNSString(IDS_IOS_LOCATION_SETTINGS_HELP_DETAIL);

  // Check strings.
  EXPECT_NSEQ(expected_title, config->title());
  EXPECT_NSEQ(expected_message, config->message());

  // Check buttons.
  ASSERT_EQ(1U, config->button_configs().size());
  const std::vector<ButtonConfig>& button_configs = config->button_configs()[0];
  ASSERT_EQ(2U, button_configs.size());

  ButtonConfig cancel_button = button_configs[0];
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_CANCEL), cancel_button.title);
  EXPECT_EQ(UIAlertActionStyleCancel, cancel_button.style);

  ButtonConfig settings_button = button_configs[1];
  EXPECT_NSEQ(
      l10n_util::GetNSString(IDS_IOS_LOCATION_SETTINGS_HELP_BUTTON_SETTINGS),
      settings_button.title);
  EXPECT_EQ(UIAlertActionStyleDefault, settings_button.style);
}

// Tests that an alert is correctly converted to a
// GeolocationPromptDialogResponse after tapping "Cancel".
TEST_F(GeolocationPromptDialogOverlayTest, DialogResponseCancel) {
  std::unique_ptr<OverlayRequest> request = CreateRequest();
  AlertRequest* config = request->GetConfig<AlertRequest>();
  ASSERT_TRUE(config);

  std::unique_ptr<OverlayResponse> alert_response =
      OverlayResponse::CreateWithInfo<AlertResponse>(
          /*tapped_button_row_index=*/0,
          /*tapped_button_column_index=*/0,
          /*text_field_values=*/nil);

  std::unique_ptr<OverlayResponse> response =
      config->response_converter().Run(std::move(alert_response));
  ASSERT_TRUE(response.get());

  GeolocationPromptDialogResponse* geo_response =
      response->GetInfo<GeolocationPromptDialogResponse>();
  ASSERT_TRUE(geo_response);
  EXPECT_FALSE(geo_response->tapped_settings());
  EXPECT_EQ(GeolocationPromptDialogDecision::kCancel, geo_response->decision());
}

// Tests that an alert is correctly converted to a
// GeolocationPromptDialogResponse after tapping "Settings".
TEST_F(GeolocationPromptDialogOverlayTest, DialogResponseSettings) {
  std::unique_ptr<OverlayRequest> request = CreateRequest();
  AlertRequest* config = request->GetConfig<AlertRequest>();
  ASSERT_TRUE(config);

  std::unique_ptr<OverlayResponse> alert_response =
      OverlayResponse::CreateWithInfo<AlertResponse>(
          /*tapped_button_row_index=*/0,
          /*tapped_button_column_index=*/1,
          /*text_field_values=*/nil);

  std::unique_ptr<OverlayResponse> response =
      config->response_converter().Run(std::move(alert_response));
  ASSERT_TRUE(response.get());

  GeolocationPromptDialogResponse* geo_response =
      response->GetInfo<GeolocationPromptDialogResponse>();
  ASSERT_TRUE(geo_response);
  EXPECT_TRUE(geo_response->tapped_settings());
  EXPECT_EQ(GeolocationPromptDialogDecision::kOpenSettings,
            geo_response->decision());
}
