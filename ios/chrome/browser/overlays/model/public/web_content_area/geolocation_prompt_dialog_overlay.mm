// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/model/public/web_content_area/geolocation_prompt_dialog_overlay.h"

#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/overlays/model/public/web_content_area/alert_constants.h"
#import "ios/chrome/browser/overlays/model/public/web_content_area/alert_overlay.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

using alert_overlays::AlertRequest;
using alert_overlays::AlertResponse;
using alert_overlays::ButtonConfig;

namespace {

// Creates the overlay response to a geolocation prompt dialog where
// `open_settings_index` is the index of the open settings button in the alert.
std::unique_ptr<OverlayResponse> CreateGeolocationPromptDialogResponse(
    size_t open_settings_index,
    std::unique_ptr<OverlayResponse> response) {
  AlertResponse* alert_response = response->GetInfo<AlertResponse>();
  if (!alert_response) {
    return nullptr;
  }
  size_t button_index = alert_response->tapped_button_column_index();
  GeolocationPromptDialogDecision decision =
      button_index == open_settings_index
          ? GeolocationPromptDialogDecision::kOpenSettings
          : GeolocationPromptDialogDecision::kCancel;
  return OverlayResponse::CreateWithInfo<GeolocationPromptDialogResponse>(
      decision);
}

}  // namespace

#pragma mark - GeolocationPromptDialogRequest

GeolocationPromptDialogRequest::GeolocationPromptDialogRequest() {
  title_ = l10n_util::GetNSString(IDS_IOS_LOCATION_SETTINGS_HELP_TITLE);
  message_ = l10n_util::GetNSString(IDS_IOS_LOCATION_SETTINGS_HELP_DETAIL);
}

GeolocationPromptDialogRequest::~GeolocationPromptDialogRequest() = default;

void GeolocationPromptDialogRequest::CreateAuxiliaryData(
    base::SupportsUserData* user_data) {
  std::vector<std::vector<ButtonConfig>> button_configs{
      {ButtonConfig(l10n_util::GetNSString(IDS_CANCEL),
                    UIAlertActionStyleCancel),
       ButtonConfig(l10n_util::GetNSString(
           IDS_IOS_LOCATION_SETTINGS_HELP_BUTTON_SETTINGS))}};

  AlertRequest::CreateForUserData(
      user_data, title(), message(), kPermissionsDialogAccessibilityIdentifier,
      nil, button_configs,
      base::BindRepeating(&CreateGeolocationPromptDialogResponse,
                          /*open_settings_index=*/1));
}

#pragma mark - GeolocationPromptDialogResponse

GeolocationPromptDialogResponse::GeolocationPromptDialogResponse(
    GeolocationPromptDialogDecision decision)
    : decision_(decision) {}

GeolocationPromptDialogResponse::~GeolocationPromptDialogResponse() = default;
