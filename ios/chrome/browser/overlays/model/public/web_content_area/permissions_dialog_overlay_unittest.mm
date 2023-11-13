// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/model/public/web_content_area/permissions_dialog_overlay.h"

#import "ios/chrome/browser/overlays/model/public/overlay_request.h"
#import "ios/chrome/browser/overlays/model/public/overlay_response.h"
#import "ios/chrome/browser/overlays/model/public/web_content_area/alert_overlay.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/permissions/permissions.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

using alert_overlays::AlertRequest;
using alert_overlays::AlertResponse;
using alert_overlays::ButtonConfig;

// Test fixture for permissions dialog overlays.
class PermissionsDialogOverlayTest : public PlatformTest {
 protected:
  std::unique_ptr<OverlayRequest> CreateRequest(
      NSArray<NSNumber*>* permissions) {
    return OverlayRequest::CreateWithConfig<PermissionsDialogRequest>(
        GURL("http://www.chromium.test"), permissions);
  }
};

// Tests that the alert config is set correctly for dialogs requesting only
// camera permission.
TEST_F(PermissionsDialogOverlayTest, DialogTitleCameraOnly) {
  std::unique_ptr<OverlayRequest> request =
      CreateRequest(@[ @(web::PermissionCamera) ]);
  AlertRequest* config = request->GetConfig<AlertRequest>();
  ASSERT_TRUE(config);
  NSString* expected_string = l10n_util::GetNSStringF(
      IDS_IOS_PERMISSIONS_ALERT_DIALOG_MESSAGE, u"www.chromium.test",
      l10n_util::GetStringUTF16(
          IDS_IOS_PERMISSIONS_ALERT_DIALOG_PERMISSION_CAMERA));
  // Check strings.
  EXPECT_NSEQ(expected_string, config->title());
  EXPECT_EQ(nil, config->message());
  // Check buttons.
  ASSERT_EQ(1U, config->button_configs().size());
  const std::vector<ButtonConfig>& button_configs = config->button_configs()[0];
  ASSERT_EQ(2U, button_configs.size());
  ButtonConfig deny_button = button_configs[0];
  EXPECT_NSEQ(
      l10n_util::GetNSString(IDS_IOS_PERMISSIONS_ALERT_DIALOG_BUTTON_TEXT_DENY),
      deny_button.title);
  EXPECT_EQ(UIAlertActionStyleCancel, deny_button.style);
  ButtonConfig allow_button = button_configs[1];
  EXPECT_NSEQ(l10n_util::GetNSString(
                  IDS_IOS_PERMISSIONS_ALERT_DIALOG_BUTTON_TEXT_GRANT),
              allow_button.title);
  EXPECT_EQ(UIAlertActionStyleDefault, allow_button.style);
}

// Tests that the alert config is set correctly for dialogs requesting only
// microphone permission.
TEST_F(PermissionsDialogOverlayTest, DialogMicrophoneOnly) {
  std::unique_ptr<OverlayRequest> request =
      CreateRequest(@[ @(web::PermissionMicrophone) ]);
  AlertRequest* config = request->GetConfig<AlertRequest>();
  ASSERT_TRUE(config);
  NSString* expected_string = l10n_util::GetNSStringF(
      IDS_IOS_PERMISSIONS_ALERT_DIALOG_MESSAGE, u"www.chromium.test",
      l10n_util::GetStringUTF16(
          IDS_IOS_PERMISSIONS_ALERT_DIALOG_PERMISSION_MICROPHONE));
  // Check strings.
  EXPECT_NSEQ(expected_string, config->title());
  EXPECT_EQ(nil, config->message());
  // Check buttons.
  ASSERT_EQ(1U, config->button_configs().size());
  const std::vector<ButtonConfig>& button_configs = config->button_configs()[0];
  ASSERT_EQ(2U, button_configs.size());
  ButtonConfig deny_button = button_configs[0];
  EXPECT_NSEQ(
      l10n_util::GetNSString(IDS_IOS_PERMISSIONS_ALERT_DIALOG_BUTTON_TEXT_DENY),
      deny_button.title);
  EXPECT_EQ(UIAlertActionStyleCancel, deny_button.style);
  ButtonConfig allow_button = button_configs[1];
  EXPECT_NSEQ(l10n_util::GetNSString(
                  IDS_IOS_PERMISSIONS_ALERT_DIALOG_BUTTON_TEXT_GRANT),
              allow_button.title);
  EXPECT_EQ(UIAlertActionStyleDefault, allow_button.style);
}

// Tests that the alert config is set correctly for dialogs requesting both
// camera and microphone permission.
TEST_F(PermissionsDialogOverlayTest, DialogCameraAndMicrophone) {
  std::unique_ptr<OverlayRequest> request = CreateRequest(
      @[ @(web::PermissionCamera), @(web::PermissionMicrophone) ]);
  AlertRequest* config = request->GetConfig<AlertRequest>();
  ASSERT_TRUE(config);
  NSString* expected_string = l10n_util::GetNSStringF(
      IDS_IOS_PERMISSIONS_ALERT_DIALOG_MESSAGE, u"www.chromium.test",
      l10n_util::GetStringUTF16(
          IDS_IOS_PERMISSIONS_ALERT_DIALOG_PERMISSION_CAMERA_AND_MICROPHONE));
  // Check strings.
  EXPECT_NSEQ(expected_string, config->title());
  EXPECT_EQ(nil, config->message());
  // Check buttons.
  ASSERT_EQ(1U, config->button_configs().size());
  const std::vector<ButtonConfig>& button_configs = config->button_configs()[0];
  ASSERT_EQ(2U, button_configs.size());
  ButtonConfig deny_button = button_configs[0];
  EXPECT_NSEQ(
      l10n_util::GetNSString(IDS_IOS_PERMISSIONS_ALERT_DIALOG_BUTTON_TEXT_DENY),
      deny_button.title);
  EXPECT_EQ(UIAlertActionStyleCancel, deny_button.style);
  ButtonConfig allow_button = button_configs[1];
  EXPECT_NSEQ(l10n_util::GetNSString(
                  IDS_IOS_PERMISSIONS_ALERT_DIALOG_BUTTON_TEXT_GRANT),
              allow_button.title);
  EXPECT_EQ(UIAlertActionStyleDefault, allow_button.style);
}

// Tests that an alert is correctly converted to a
// PermissionsDialogOverlayResponse after tapping "Don't Allow".
TEST_F(PermissionsDialogOverlayTest, DialogResponseDeny) {
  std::unique_ptr<OverlayRequest> request =
      CreateRequest(@[ @(web::PermissionCamera) ]);
  AlertRequest* config = request->GetConfig<AlertRequest>();
  ASSERT_TRUE(config);
  // Simulate a response where the "Don't Allow" button is tapped.
  std::unique_ptr<OverlayResponse> alert_response =
      OverlayResponse::CreateWithInfo<AlertResponse>(
          /*tapped_button_row_index=*/0,
          /*tapped_button_column_index=*/0, nil);
  // Since the OK button is tapped, the kConfirm action should be used and the
  // text field input should be supplied to the JavaScriptAlertDialogResponse.
  std::unique_ptr<OverlayResponse> response =
      config->response_converter().Run(std::move(alert_response));
  ASSERT_TRUE(response.get());
  PermissionsDialogResponse* permissions_response =
      response->GetInfo<PermissionsDialogResponse>();
  ASSERT_TRUE(permissions_response);
  ASSERT_FALSE(permissions_response->capture_allow());
}

// Tests that an alert is correctly converted to a
// PermissionsDialogOverlayResponse after tapping "Allow".
TEST_F(PermissionsDialogOverlayTest, DialogResponseAllow) {
  std::unique_ptr<OverlayRequest> request =
      CreateRequest(@[ @(web::PermissionMicrophone) ]);
  AlertRequest* config = request->GetConfig<AlertRequest>();
  ASSERT_TRUE(config);
  // Simulate a response where the "Allow" button is tapped.
  std::unique_ptr<OverlayResponse> alert_response =
      OverlayResponse::CreateWithInfo<AlertResponse>(
          /*tapped_button_row_index=*/0,
          /*tapped_button_column_index=*/1, nil);
  // Since the OK button is tapped, the kConfirm action should be used and the
  // text field input should be supplied to the JavaScriptAlertDialogResponse.
  std::unique_ptr<OverlayResponse> response =
      config->response_converter().Run(std::move(alert_response));
  ASSERT_TRUE(response.get());
  PermissionsDialogResponse* permissions_response =
      response->GetInfo<PermissionsDialogResponse>();
  ASSERT_TRUE(permissions_response && permissions_response->capture_allow());
}
