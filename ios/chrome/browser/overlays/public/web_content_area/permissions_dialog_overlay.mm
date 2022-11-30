// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/public/web_content_area/permissions_dialog_overlay.h"

#import "ios/chrome/browser/overlays/public/web_content_area/alert_constants.h"
#import "ios/chrome/browser/overlays/public/web_content_area/alert_overlay.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using alert_overlays::AlertRequest;
using alert_overlays::AlertResponse;
using alert_overlays::ButtonConfig;

namespace {
// The index of the "Don't Allow" button in the alert button array.
const size_t kButtonIndexDontAllow = 0;

// Creates an permissions dialog response for a dialog, containing a boolean
// `capture_allow()` indicating ther user's answer on the media capture request;
// created with an AlertResponse.
std::unique_ptr<OverlayResponse> CreatePermissionsDialogResponse(
    std::unique_ptr<OverlayResponse> response) API_AVAILABLE(ios(15.0)) {
  AlertResponse* alert_response = response->GetInfo<AlertResponse>();
  size_t button_index = alert_response->tapped_button_index();
  return OverlayResponse::CreateWithInfo<PermissionsDialogResponse>(
      button_index != kButtonIndexDontAllow);
}
}  // namespace

#pragma mark - PermissionsDialogRequest

OVERLAY_USER_DATA_SETUP_IMPL(PermissionsDialogRequest);

PermissionsDialogRequest::PermissionsDialogRequest(
    NSString* website,
    NSArray<NSNumber*>* requested_permissions) {
  // Computes the dialog message based on the website and permissions requested.
  // TODO(crbug.com/1356768): Add strings to ios_strings.grd and retrieve from
  // there.
  NSString* typeString;
  BOOL cameraCapturing =
      [requested_permissions containsObject:@(web::PermissionCamera)];
  BOOL micCapturing =
      [requested_permissions containsObject:@(web::PermissionMicrophone)];
  if (cameraCapturing) {
    typeString = micCapturing ? @"Microphone and Camera" : @"Camera";
  } else {
    typeString = @"Microphone";
    message_ = [NSString stringWithFormat:@"\"%@\" Would Like To Access %@",
                                          website, typeString];
  }
}

PermissionsDialogRequest::~PermissionsDialogRequest() = default;

void PermissionsDialogRequest::CreateAuxiliaryData(
    base::SupportsUserData* user_data) {
  // Conrigure buttons.
  NSString* dont_allow_label = @"Don't Allow";
  NSString* allow_label = @"Allow";
  const std::vector<ButtonConfig> button_configs{
      ButtonConfig(dont_allow_label, UIAlertActionStyleCancel),
      ButtonConfig(allow_label, UIAlertActionStyleDefault)};
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
