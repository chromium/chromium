// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/model/public/web_content_area/app_launcher_overlay.h"

#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request.h"
#import "ios/chrome/browser/overlays/model/public/overlay_response.h"
#import "ios/chrome/browser/overlays/model/public/web_content_area/alert_overlay.h"
#import "ios/chrome/browser/overlays/model/public/web_content_area/app_launcher_overlay.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"

using alert_overlays::AlertRequest;
using alert_overlays::AlertResponse;
using alert_overlays::ButtonConfig;
using app_launcher_overlays::AppLaunchConfirmationRequest;
using app_launcher_overlays::AllowAppLaunchResponse;

// Test fixture for app launcher overlays.
using AppLauncherOverlayTest = PlatformTest;

// Tests that the alert overlay request is set correctly for the first app
// launch request.
TEST_F(AppLauncherOverlayTest, FirstRequestAlertSetup) {
  std::unique_ptr<OverlayRequest> request =
      OverlayRequest::CreateWithConfig<AppLaunchConfirmationRequest>(
          app_launcher_overlays::AppLaunchConfirmationRequestCause::kOther);
  AlertRequest* config = request->GetConfig<AlertRequest>();
  ASSERT_TRUE(config);

  // The app launch alert has no title, and uses IDS_IOS_OPEN_IN_ANOTHER_APP as
  // its message.
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_OPEN_IN_ANOTHER_APP),
              config->message());

  // There is an OK button and a Cancel button in app launch alerts.
  ASSERT_EQ(2U, config->button_configs().size());
  const ButtonConfig& ok_button_config = config->button_configs()[0][0];
  const ButtonConfig& cancel_button_config = config->button_configs()[1][0];

  EXPECT_EQ(UIAlertActionStyleDefault, ok_button_config.style);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_OPEN_ANOTHER_APP_ALLOW),
              ok_button_config.title);

  EXPECT_EQ(UIAlertActionStyleCancel, cancel_button_config.style);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_OPEN_ANOTHER_APP_BLOCK),
              cancel_button_config.title);
}

// Tests that the alert overlay request is set correctly for a repeated app
// launch request.
TEST_F(AppLauncherOverlayTest, RepeatedRequestAlertSetup) {
  std::unique_ptr<OverlayRequest> request =
      OverlayRequest::CreateWithConfig<AppLaunchConfirmationRequest>(
          app_launcher_overlays::AppLaunchConfirmationRequestCause::
              kRepeatedRequest);
  AlertRequest* config = request->GetConfig<AlertRequest>();
  ASSERT_TRUE(config);

  // The app launch alert has no title, and uses
  // IDS_IOS_OPEN_REPEATEDLY_ANOTHER_APP as its message.
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_OPEN_REPEATEDLY_ANOTHER_APP),
              config->message());

  // There is an OK button and a Cancel button in app launch alerts.
  ASSERT_EQ(2U, config->button_configs().size());
  const ButtonConfig& ok_button_config = config->button_configs()[0][0];
  const ButtonConfig& cancel_button_config = config->button_configs()[1][0];

  EXPECT_EQ(UIAlertActionStyleDefault, ok_button_config.style);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_OPEN_ANOTHER_APP_ALLOW),
              ok_button_config.title);

  EXPECT_EQ(UIAlertActionStyleCancel, cancel_button_config.style);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_OPEN_ANOTHER_APP_BLOCK),
              cancel_button_config.title);
}

// Tests that the alert overlay request is set correctly for a launch request in
// incognito.
TEST_F(AppLauncherOverlayTest, IncognitoRequestAlertSetup) {
  std::unique_ptr<OverlayRequest> request =
      OverlayRequest::CreateWithConfig<AppLaunchConfirmationRequest>(
          app_launcher_overlays::AppLaunchConfirmationRequestCause::
              kOpenFromIncognito);
  AlertRequest* config = request->GetConfig<AlertRequest>();
  ASSERT_TRUE(config);

  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_OPEN_ANOTHER_APP_FROM_INCOGNITO),
              config->message());

  // There is an OK button and a Cancel button in app launch alerts.
  ASSERT_EQ(2U, config->button_configs().size());
  const ButtonConfig& ok_button_config = config->button_configs()[0][0];
  const ButtonConfig& cancel_button_config = config->button_configs()[1][0];

  EXPECT_EQ(UIAlertActionStyleDefault, ok_button_config.style);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_OPEN_ANOTHER_APP_ALLOW),
              ok_button_config.title);

  EXPECT_EQ(UIAlertActionStyleCancel, cancel_button_config.style);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_OPEN_ANOTHER_APP_BLOCK),
              cancel_button_config.title);
}

// Tests that the alert overlay request is set correctly for a launch request
// not user initiated.
TEST_F(AppLauncherOverlayTest, NotUserInitiatedRequestAlertSetup) {
  std::unique_ptr<OverlayRequest> request =
      OverlayRequest::CreateWithConfig<AppLaunchConfirmationRequest>(
          app_launcher_overlays::AppLaunchConfirmationRequestCause::
              kNoUserInteraction);
  AlertRequest* config = request->GetConfig<AlertRequest>();
  ASSERT_TRUE(config);

  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_OPEN_IN_ANOTHER_APP),
              config->message());

  // There is an OK button and a Cancel button in app launch alerts.
  ASSERT_EQ(2U, config->button_configs().size());
  const ButtonConfig& ok_button_config = config->button_configs()[0][0];
  const ButtonConfig& cancel_button_config = config->button_configs()[1][0];

  EXPECT_EQ(UIAlertActionStyleDefault, ok_button_config.style);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_OPEN_ANOTHER_APP_ALLOW),
              ok_button_config.title);

  EXPECT_EQ(UIAlertActionStyleCancel, cancel_button_config.style);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_OPEN_ANOTHER_APP_BLOCK),
              cancel_button_config.title);
}

// Tests that an alert response after tapping the OK button successfully creates
// an AllowAppLaunchResponse.
TEST_F(AppLauncherOverlayTest, ResponseConversionOk) {
  // Simulate a response where the OK button is tapped.
  std::unique_ptr<OverlayRequest> request =
      OverlayRequest::CreateWithConfig<AppLaunchConfirmationRequest>(
          app_launcher_overlays::AppLaunchConfirmationRequestCause::kOther);
  AlertRequest* config = request->GetConfig<AlertRequest>();
  std::unique_ptr<OverlayResponse> alert_response =
      OverlayResponse::CreateWithInfo<AlertResponse>(
          /*tapped_button_row_index=*/0, /*tapped_button_column_index=*/0,
          /*text_field_values=*/nil);

  // Convert the response to the AllowAppLaunchResponse.
  std::unique_ptr<OverlayResponse> response =
      config->response_converter().Run(std::move(alert_response));
  ASSERT_TRUE(response);
  EXPECT_TRUE(response->GetInfo<AllowAppLaunchResponse>());
}

// Tests that an alert response after tapping the Cancel button is converted to
// a null response.
TEST_F(AppLauncherOverlayTest, ResponseConversionCancel) {
  // Simulate a response where the Cancel button is tapped.
  std::unique_ptr<OverlayRequest> request =
      OverlayRequest::CreateWithConfig<AppLaunchConfirmationRequest>(
          app_launcher_overlays::AppLaunchConfirmationRequestCause::kOther);
  AlertRequest* config = request->GetConfig<AlertRequest>();
  std::unique_ptr<OverlayResponse> alert_response =
      OverlayResponse::CreateWithInfo<AlertResponse>(
          /*tapped_button_row_index=*/1, /*tapped_button_column_index=*/0,
          /*text_field_values=*/nil);

  // Convert the response and verify that no AllowAppLaunchResponse was created.
  std::unique_ptr<OverlayResponse> response =
      config->response_converter().Run(std::move(alert_response));
  EXPECT_FALSE(response);
}
