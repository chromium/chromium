// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/public/web_content_area/app_launcher_overlay.h"

#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/overlays/public/overlay_request.h"
#import "ios/chrome/browser/overlays/public/overlay_response.h"
#import "ios/chrome/browser/overlays/public/web_content_area/alert_overlay.h"
#import "ios/chrome/browser/overlays/public/web_content_area/app_launcher_overlay.h"
#include "ios/chrome/grit/ios_strings.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
          /*is_repeated_request=*/false);
  AlertRequest* config = request->GetConfig<AlertRequest>();
  ASSERT_TRUE(config);

  // The app launch alert has no title, and uses IDS_IOS_OPEN_IN_ANOTHER_APP as
  // its message.
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_OPEN_IN_ANOTHER_APP),
              config->message());

  // There is an OK button and a Cancel button in app launch alerts.
  ASSERT_EQ(2U, config->button_configs().size());
  const ButtonConfig& ok_button_config = config->button_configs()[0];
  const ButtonConfig& cancel_button_config = config->button_configs()[1];

  EXPECT_EQ(UIAlertActionStyleDefault, ok_button_config.style);
  EXPECT_NSEQ(
      l10n_util::GetNSString(IDS_IOS_APP_LAUNCHER_OPEN_APP_BUTTON_LABEL),
      ok_button_config.title);

  EXPECT_EQ(UIAlertActionStyleCancel, cancel_button_config.style);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_CANCEL), cancel_button_config.title);
}

// Tests that the alert overlay request is set correctly for a repeated app
// launch request.
TEST_F(AppLauncherOverlayTest, RepeatedRequestAlertSetup) {
  std::unique_ptr<OverlayRequest> request =
      OverlayRequest::CreateWithConfig<AppLaunchConfirmationRequest>(
          /*is_repeated_request=*/true);
  AlertRequest* config = request->GetConfig<AlertRequest>();
  ASSERT_TRUE(config);

  // The app launch alert has no title, and uses
  // IDS_IOS_OPEN_REPEATEDLY_ANOTHER_APP as its message.
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_OPEN_REPEATEDLY_ANOTHER_APP),
              config->message());

  // There is an OK button and a Cancel button in app launch alerts.
  ASSERT_EQ(2U, config->button_configs().size());
  const ButtonConfig& ok_button_config = config->button_configs()[0];
  const ButtonConfig& cancel_button_config = config->button_configs()[1];

  EXPECT_EQ(UIAlertActionStyleDefault, ok_button_config.style);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_OPEN_REPEATEDLY_ANOTHER_APP_ALLOW),
              ok_button_config.title);

  EXPECT_EQ(UIAlertActionStyleCancel, cancel_button_config.style);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_CANCEL), cancel_button_config.title);
}

// Tests that an alert response after tapping the OK button successfully creates
// an AllowAppLaunchResponse.
TEST_F(AppLauncherOverlayTest, ResponseConversionOk) {
  // Simulate a response where the OK button is tapped.
  std::unique_ptr<OverlayRequest> request =
      OverlayRequest::CreateWithConfig<AppLaunchConfirmationRequest>(
          /*is_repeated_request=*/false);
  AlertRequest* config = request->GetConfig<AlertRequest>();
  std::unique_ptr<OverlayResponse> alert_response =
      OverlayResponse::CreateWithInfo<AlertResponse>(
          /*tapped_button_index=*/0, /*text_field_values=*/nil);

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
          /*is_repeated_request=*/false);
  AlertRequest* config = request->GetConfig<AlertRequest>();
  std::unique_ptr<OverlayResponse> alert_response =
      OverlayResponse::CreateWithInfo<AlertResponse>(
          /*tapped_button_index=*/1, /*text_field_values=*/nil);

  // Convert the response and verify that no AllowAppLaunchResponse was created.
  std::unique_ptr<OverlayResponse> response =
      config->response_converter().Run(std::move(alert_response));
  EXPECT_FALSE(response);
}
