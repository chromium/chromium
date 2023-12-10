// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/model/public/web_content_area/app_launcher_overlay.h"

#import "base/functional/bind.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/overlays/model/public/web_content_area/alert_overlay.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

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
  if (!alert_info || alert_info->tapped_button_row_index() != kButtonIndexOk) {
    return nullptr;
  }

  return OverlayResponse::CreateWithInfo<AllowAppLaunchResponse>();
}
}  // namespace

#pragma mark - AppLaunchConfirmationRequest

OVERLAY_USER_DATA_SETUP_IMPL(AppLaunchConfirmationRequest);

AppLaunchConfirmationRequest::AppLaunchConfirmationRequest(
    AppLaunchConfirmationRequestCause cause)
    : cause_(cause) {}

AppLaunchConfirmationRequest::~AppLaunchConfirmationRequest() = default;

void AppLaunchConfirmationRequest::CreateAuxiliaryData(
    base::SupportsUserData* user_data) {
  NSString* alert_message = nil;
  NSString* allow_button_title = nil;
  NSString* reject_button_title = nil;
  switch (cause_) {
    case AppLaunchConfirmationRequestCause::kOther:
      alert_message = l10n_util::GetNSString(IDS_IOS_OPEN_IN_ANOTHER_APP);
      allow_button_title =
          l10n_util::GetNSString(IDS_IOS_OPEN_ANOTHER_APP_ALLOW);
      reject_button_title =
          l10n_util::GetNSString(IDS_IOS_OPEN_ANOTHER_APP_BLOCK);
      break;
    case AppLaunchConfirmationRequestCause::kRepeatedRequest:
      alert_message =
          l10n_util::GetNSString(IDS_IOS_OPEN_REPEATEDLY_ANOTHER_APP);
      allow_button_title =
          l10n_util::GetNSString(IDS_IOS_OPEN_ANOTHER_APP_ALLOW);
      reject_button_title =
          l10n_util::GetNSString(IDS_IOS_OPEN_ANOTHER_APP_BLOCK);
      break;
    case AppLaunchConfirmationRequestCause::kOpenFromIncognito:
      alert_message =
          l10n_util::GetNSString(IDS_IOS_OPEN_ANOTHER_APP_FROM_INCOGNITO);
      allow_button_title =
          l10n_util::GetNSString(IDS_IOS_OPEN_ANOTHER_APP_ALLOW);
      reject_button_title =
          l10n_util::GetNSString(IDS_IOS_OPEN_ANOTHER_APP_BLOCK);
      break;
    case AppLaunchConfirmationRequestCause::kNoUserInteraction:
      alert_message = l10n_util::GetNSString(IDS_IOS_OPEN_IN_ANOTHER_APP);
      allow_button_title =
          l10n_util::GetNSString(IDS_IOS_OPEN_ANOTHER_APP_ALLOW);
      reject_button_title =
          l10n_util::GetNSString(IDS_IOS_OPEN_ANOTHER_APP_BLOCK);
      break;
    case AppLaunchConfirmationRequestCause::kAppLaunchFailed:
      alert_message = l10n_util::GetNSString(IDS_IOS_OPEN_ANOTHER_APP_FAILED);
      reject_button_title =
          l10n_util::GetNSString(IDS_IOS_OPEN_ANOTHER_APP_FAILED_CONFIRM);
      break;
  }

  std::vector<std::vector<ButtonConfig>> alert_button_configs;
  if (allow_button_title) {
    alert_button_configs.push_back({ButtonConfig(allow_button_title)});
  }
  if (reject_button_title) {
    alert_button_configs.push_back(
        {ButtonConfig(reject_button_title, UIAlertActionStyleCancel)});
  }
  AlertRequest::CreateForUserData(
      user_data, /*title=*/nil, alert_message, /*accessibility_identifier=*/nil,
      /*text_field_configs=*/nil, alert_button_configs,
      base::BindRepeating(&CreateAllowAppLaunchResponse));
}

#pragma mark - AllowAppLaunchResponse

OVERLAY_USER_DATA_SETUP_IMPL(AllowAppLaunchResponse);

}  // app_launcher_overlays
