// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/model/public/web_content_area/permissions_dialog_overlay.h"

#import "base/strings/utf_string_conversions.h"
#import "ios/chrome/browser/overlays/model/public/web_content_area/alert_constants.h"
#import "ios/chrome/browser/overlays/model/public/web_content_area/alert_overlay.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/permissions/permissions.h"
#import "ui/base/l10n/l10n_util.h"

using alert_overlays::AlertRequest;
using alert_overlays::AlertResponse;
using alert_overlays::ButtonConfig;

namespace {

// The column index of the button that the user clicks to grant permissions.
const size_t kPermissionsGrantedButtonIndex = 1;

// Creates an permissions dialog response for a dialog, containing a boolean
// `capture_allow()` indicating ther user's answer on the media capture request;
// created with an AlertResponse.
std::unique_ptr<OverlayResponse> CreatePermissionsDialogResponse(
    std::unique_ptr<OverlayResponse> response) {
  AlertResponse* alert_response = response->GetInfo<AlertResponse>();
  if (!alert_response) {
    return nullptr;
  }
  size_t button_index = alert_response->tapped_button_column_index();
  return OverlayResponse::CreateWithInfo<PermissionsDialogResponse>(
      /*capture_allow=*/button_index == kPermissionsGrantedButtonIndex);
}

}  // namespace

#pragma mark - PermissionsDialogRequest

OVERLAY_USER_DATA_SETUP_IMPL(PermissionsDialogRequest);

PermissionsDialogRequest::PermissionsDialogRequest(
    const GURL& url,
    NSArray<NSNumber*>* requested_permissions) {
  // Computes the dialog message based on the website and permissions requested.
  int string_id_for_permission = 0;
  BOOL camera_permission_requested =
      [requested_permissions containsObject:@(web::PermissionCamera)];
  BOOL mic_permission_requested =
      [requested_permissions containsObject:@(web::PermissionMicrophone)];
  if (camera_permission_requested && mic_permission_requested) {
    string_id_for_permission =
        IDS_IOS_PERMISSIONS_ALERT_DIALOG_PERMISSION_CAMERA_AND_MICROPHONE;
  } else if (camera_permission_requested) {
    string_id_for_permission =
        IDS_IOS_PERMISSIONS_ALERT_DIALOG_PERMISSION_CAMERA;
  } else if (mic_permission_requested) {
    string_id_for_permission =
        IDS_IOS_PERMISSIONS_ALERT_DIALOG_PERMISSION_MICROPHONE;
  } else {
    NOTREACHED_IN_MIGRATION();
  }
  message_ = l10n_util::GetNSStringF(
      IDS_IOS_PERMISSIONS_ALERT_DIALOG_MESSAGE, base::UTF8ToUTF16(url.host()),
      l10n_util::GetStringUTF16(string_id_for_permission));
}

PermissionsDialogRequest::~PermissionsDialogRequest() = default;

void PermissionsDialogRequest::CreateAuxiliaryData(
    base::SupportsUserData* user_data) {
  // Conrigure buttons.
  std::vector<std::vector<ButtonConfig>> button_configs{
      {ButtonConfig(l10n_util::GetNSString(
                        IDS_IOS_PERMISSIONS_ALERT_DIALOG_BUTTON_TEXT_DENY),
                    UIAlertActionStyleCancel),
       ButtonConfig(l10n_util::GetNSString(
           IDS_IOS_PERMISSIONS_ALERT_DIALOG_BUTTON_TEXT_GRANT))}};
  // Create the alert config with the buttons and other information.
  AlertRequest::CreateForUserData(
      user_data, message(), nil, kPermissionsDialogAccessibilityIdentifier, nil,
      button_configs, base::BindRepeating(&CreatePermissionsDialogResponse));
}

#pragma mark - PermissionsDialogResponse

OVERLAY_USER_DATA_SETUP_IMPL(PermissionsDialogResponse);

PermissionsDialogResponse::PermissionsDialogResponse(bool capture_allow)
    : capture_allow_(capture_allow) {}

PermissionsDialogResponse::~PermissionsDialogResponse() = default;
