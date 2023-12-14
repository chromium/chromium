// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/model/public/web_content_area/insecure_form_overlay.h"

#import <vector>

#import "base/functional/bind.h"
#import "base/strings/sys_string_conversions.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/overlays/model/public/web_content_area/alert_constants.h"
#import "ios/chrome/browser/overlays/model/public/web_content_area/alert_overlay.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

using alert_overlays::AlertRequest;
using alert_overlays::AlertResponse;
using alert_overlays::ButtonConfig;

namespace {

// Name of UMA User Action logged when user taps Cancel button.
const char kInsecureFormCancelActionName[] = "IOS.InsecureFormCancel";

// Name of UMA User Action logged when user taps Send Anyway button.
const char kInsecureFormSendAnywayActionName[] = "IOS.InsecureFormSendAnyway";

// The dialog shows Cancel and Allow buttons. This index is zero based.
const size_t kAllowSendButtonIndex = 1;

std::unique_ptr<OverlayResponse> CreateInsecureFormDialogResponse(
    std::unique_ptr<OverlayResponse> response) {
  AlertResponse* alert_response = response->GetInfo<AlertResponse>();
  if (!alert_response) {
    return nullptr;
  }
  size_t button_index = alert_response->tapped_button_column_index();
  return OverlayResponse::CreateWithInfo<InsecureFormDialogResponse>(
      /*allow_send=*/button_index == kAllowSendButtonIndex);
}

}  // namespace

#pragma mark - InsecureFormOverlayRequestConfig

OVERLAY_USER_DATA_SETUP_IMPL(InsecureFormOverlayRequestConfig);

InsecureFormOverlayRequestConfig::InsecureFormOverlayRequestConfig() = default;

InsecureFormOverlayRequestConfig::~InsecureFormOverlayRequestConfig() = default;

void InsecureFormOverlayRequestConfig::CreateAuxiliaryData(
    base::SupportsUserData* user_data) {
  NSString* alert_title =
      l10n_util::GetNSStringWithFixup(IDS_INSECURE_FORM_HEADING);
  NSString* alert_message =
      l10n_util::GetNSStringWithFixup(IDS_INSECURE_FORM_PRIMARY_PARAGRAPH);

  // Configure buttons.
  std::vector<std::vector<ButtonConfig>> button_configs{
      {ButtonConfig(l10n_util::GetNSString(IDS_CANCEL),
                    kInsecureFormCancelActionName, UIAlertActionStyleCancel),
       ButtonConfig(l10n_util::GetNSString(IDS_INSECURE_FORM_SUBMIT_BUTTON),
                    kInsecureFormSendAnywayActionName)}};
  // Create the alert config with the buttons and other information.
  AlertRequest::CreateForUserData(
      user_data, alert_title, alert_message,
      kInsecureFormWarningAccessibilityIdentifier,
      /*text_fields=*/nil, button_configs,
      base::BindRepeating(&CreateInsecureFormDialogResponse));
}

#pragma mark - InsecureFormDialogResponse

OVERLAY_USER_DATA_SETUP_IMPL(InsecureFormDialogResponse);

InsecureFormDialogResponse::InsecureFormDialogResponse(bool allow_send)
    : allow_send_(allow_send) {}

InsecureFormDialogResponse::~InsecureFormDialogResponse() = default;
