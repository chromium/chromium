// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/model/confirm_download_closing_overlay.h"

#import "base/functional/bind.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/overlays/model/public/common/confirmation/confirmation_overlay_response_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

const char kDownloadCloseActionName[] = "IOSDownloadConfirmClose";
const char kDownloadDoNotCloseActionName[] = "IOSDownloadDoNotClose";

OVERLAY_USER_DATA_SETUP_IMPL(ConfirmDownloadClosingRequest);

void ConfirmDownloadClosingRequest::CreateAuxiliaryData(
    base::SupportsUserData* user_data) {
  const std::vector<std::vector<alert_overlays::ButtonConfig>> buttons{
      {alert_overlays::ButtonConfig(
          l10n_util::GetNSString(IDS_IOS_DOWNLOAD_MANAGER_STOP),
          kDownloadCloseActionName)},
      {alert_overlays::ButtonConfig(
          l10n_util::GetNSString(IDS_IOS_DOWNLOAD_MANAGER_CONTINUE),
          kDownloadDoNotCloseActionName, UIAlertActionStyleCancel)}};
  alert_overlays::AlertRequest::CreateForUserData(
      user_data,
      l10n_util::GetNSString(IDS_IOS_DOWNLOAD_MANAGER_CANCEL_CONFIRMATION),
      /*message=*/nil,
      /*accessibility_identifier=*/nil,
      /*text_fields=*/nil, buttons,
      GetConfirmationResponseConverter(/*confirm_button_row_index=*/0));
}
