// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/public/web_content_area/app_launcher_overlay.h"

#include "base/bind.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/overlays/public/web_content_area/alert_overlay.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using alert_overlays::AlertRequest;
using alert_overlays::AlertResponse;
using alert_overlays::ButtonConfig;

namespace app_launcher_overlays {

namespace {
// The index of the alert for the OK button.
const size_t kButtonIndexOk = 0;

// Creates an OverlayRequest with AllowAppLaunchResponse from one created
// with an AlertResponse.
std::unique_ptr<OverlayResponse> CreateAllowAppLaunchResponse(
    std::unique_ptr<OverlayResponse> alert_response) {
  AlertResponse* alert_info = alert_response->GetInfo<AlertResponse>();
  if (!alert_info || alert_info->tapped_button_index() != kButtonIndexOk)
    return nullptr;

  return OverlayResponse::CreateWithInfo<AllowAppLaunchResponse>();
}
}  // namespace

#pragma mark - AppLaunchConfirmationRequest

OVERLAY_USER_DATA_SETUP_IMPL(AppLaunchConfirmationRequest);

AppLaunchConfirmationRequest::AppLaunchConfirmationRequest(
    bool is_repeated_request)
    : is_repeated_request_(is_repeated_request) {}

AppLaunchConfirmationRequest::~AppLaunchConfirmationRequest() = default;

void AppLaunchConfirmationRequest::CreateAuxiliaryData(
    base::SupportsUserData* user_data) {
  NSString* alert_message =
      is_repeated_request()
          ? l10n_util::GetNSString(IDS_IOS_OPEN_REPEATEDLY_ANOTHER_APP)
          : l10n_util::GetNSString(IDS_IOS_OPEN_IN_ANOTHER_APP);
  NSString* reject_button_title = l10n_util::GetNSString(IDS_CANCEL);
  NSString* allow_button_title =
      is_repeated_request()
          ? l10n_util::GetNSString(IDS_IOS_OPEN_REPEATEDLY_ANOTHER_APP_ALLOW)
          : l10n_util::GetNSString(IDS_IOS_APP_LAUNCHER_OPEN_APP_BUTTON_LABEL);
  const std::vector<ButtonConfig> alert_button_configs{
      ButtonConfig(allow_button_title),
      ButtonConfig(reject_button_title, UIAlertActionStyleCancel)};
  AlertRequest::CreateForUserData(
      user_data, /*title=*/nil, alert_message, /*accessibility_identifier=*/nil,
      /*text_field_configs=*/nil, alert_button_configs,
      base::BindRepeating(&CreateAllowAppLaunchResponse));
}

#pragma mark - AllowAppLaunchResponse

OVERLAY_USER_DATA_SETUP_IMPL(AllowAppLaunchResponse);

}  // app_launcher_overlays
