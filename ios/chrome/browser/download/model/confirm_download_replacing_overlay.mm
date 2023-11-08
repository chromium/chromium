// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/model/confirm_download_replacing_overlay.h"

#import "base/functional/bind.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/overlays/model/public/common/confirmation/confirmation_overlay_response_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

const char kDownloadReplaceActionName[] = "IOSDownloadConfirmReplace";
const char kDownloadDoNotReplaceActionName[] = "IOSDownloadDoNotReplace";

using l10n_util::GetNSString;

OVERLAY_USER_DATA_SETUP_IMPL(ConfirmDownloadReplacingRequest);

void ConfirmDownloadReplacingRequest::CreateAuxiliaryData(
    base::SupportsUserData* user_data) {
  const std::vector<std::vector<alert_overlays::ButtonConfig>> buttons{
      {alert_overlays::ButtonConfig(GetNSString(IDS_OK),
                                    kDownloadReplaceActionName)},
      {alert_overlays::ButtonConfig(GetNSString(IDS_CANCEL),
                                    kDownloadDoNotReplaceActionName,
                                    UIAlertActionStyleCancel)}};
  alert_overlays::AlertRequest::CreateForUserData(
      user_data, GetNSString(IDS_IOS_DOWNLOAD_MANAGER_REPLACE_CONFIRMATION),
      GetNSString(IDS_IOS_DOWNLOAD_MANAGER_REPLACE_CONFIRMATION_MESSAGE),
      /*accessibility_identifier=*/nil,
      /*text_fields=*/nil, buttons,
      GetConfirmationResponseConverter(/*confirm_button_row_index=*/0));
}
