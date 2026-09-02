// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/model/public/web_content_area/permissions_dialog_overlay.h"

#import "base/notreached.h"
#import "base/strings/utf_string_conversions.h"
#import "ios/chrome/browser/overlays/model/public/web_content_area/alert_constants.h"
#import "ios/chrome/browser/overlays/model/public/web_content_area/alert_overlay.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/permissions/permissions.h"
#import "ui/base/l10n/l10n_util.h"

using alert_overlays::AlertRequest;
using alert_overlays::AlertResponse;
using alert_overlays::ButtonConfig;

namespace {

// The row indices of the buttons that the user clicks in persistent 3-button
// permissions dialogs.
constexpr size_t kAlwaysAllowButtonRowIndex = 0;
constexpr size_t kAllowThisTimeButtonRowIndex = 1;
constexpr size_t kDontAllowButtonRowIndex = 2;

// The column index of the button that the user clicks to grant permissions
// in 2-button dialogs.
constexpr size_t kPermissionsGrantedButtonIndex = 1;

// Creates a permissions dialog response for a dialog, containing a
// `PermissionsDialogResponse` constructed from an `AlertResponse`.
std::unique_ptr<OverlayResponse> CreatePermissionsDialogResponse(
    std::unique_ptr<OverlayResponse> response) {
  AlertResponse* alert_response =
      response ? response->GetInfo<AlertResponse>() : nullptr;
  if (!alert_response) {
    return nullptr;
  }
  if (IsDomainLevelSitePermissionsEnabled()) {
    size_t row_index = alert_response->tapped_button_row_index();
    PermissionDialogDecision decision;
    switch (row_index) {
      case kAlwaysAllowButtonRowIndex:
        decision = PermissionDialogDecision::kAlwaysAllow;
        break;
      case kAllowThisTimeButtonRowIndex:
        decision = PermissionDialogDecision::kAllowThisTime;
        break;
      case kDontAllowButtonRowIndex:
        decision = PermissionDialogDecision::kDontAllow;
        break;
      default:
        NOTREACHED();
    }
    return OverlayResponse::CreateWithInfo<PermissionsDialogResponse>(decision);
  }

  size_t button_index = alert_response->tapped_button_column_index();
  return OverlayResponse::CreateWithInfo<PermissionsDialogResponse>(
      /*capture_allow=*/button_index == kPermissionsGrantedButtonIndex);
}

}  // namespace

#pragma mark - PermissionsDialogRequest

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
    NOTREACHED();
  }
  message_ = l10n_util::GetNSStringF(
      IDS_IOS_PERMISSIONS_ALERT_DIALOG_MESSAGE,
      base::UTF8ToUTF16(url.GetHost()),
      l10n_util::GetStringUTF16(string_id_for_permission));
}

PermissionsDialogRequest::~PermissionsDialogRequest() = default;

void PermissionsDialogRequest::CreateAuxiliaryData(
    base::SupportsUserData* user_data) {
  // Configure buttons.
  std::vector<std::vector<ButtonConfig>> button_configs;
  if (IsDomainLevelSitePermissionsEnabled()) {
    button_configs = {
        {ButtonConfig(l10n_util::GetNSString(
            IDS_IOS_PERMISSIONS_ALERT_DIALOG_BUTTON_TEXT_ALWAYS_ALLOW))},
        {ButtonConfig(l10n_util::GetNSString(
            IDS_IOS_PERMISSIONS_ALERT_DIALOG_BUTTON_TEXT_ALLOW_THIS_TIME))},
        {ButtonConfig(
            l10n_util::GetNSString(
                IDS_IOS_PERMISSIONS_ALERT_DIALOG_BUTTON_TEXT_NEVER_ALLOW),
            UIAlertActionStyleCancel)},
    };
  } else {
    button_configs = {
        {ButtonConfig(l10n_util::GetNSString(
                          IDS_IOS_PERMISSIONS_ALERT_DIALOG_BUTTON_TEXT_DENY),
                      UIAlertActionStyleCancel),
         ButtonConfig(l10n_util::GetNSString(
             IDS_IOS_PERMISSIONS_ALERT_DIALOG_BUTTON_TEXT_GRANT))}};
  }
  // Create the alert config with the buttons and other information.
  AlertRequest::CreateForUserData(
      user_data, message(), nil, kPermissionsDialogAccessibilityIdentifier, nil,
      button_configs, base::BindRepeating(&CreatePermissionsDialogResponse));
}

#pragma mark - PermissionsDialogResponse

PermissionsDialogResponse::PermissionsDialogResponse(bool capture_allow)
    : decision_(capture_allow ? PermissionDialogDecision::kAllowThisTime
                              : PermissionDialogDecision::kDontAllow) {}

PermissionsDialogResponse::PermissionsDialogResponse(
    PermissionDialogDecision decision)
    : decision_(decision) {}

PermissionsDialogResponse::~PermissionsDialogResponse() = default;
