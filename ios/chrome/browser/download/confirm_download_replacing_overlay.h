// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_CONFIRM_DOWNLOAD_REPLACING_OVERLAY_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_CONFIRM_DOWNLOAD_REPLACING_OVERLAY_H_

#include "ios/chrome/browser/overlays/public/overlay_request_config.h"
#include "ios/chrome/browser/overlays/public/overlay_response_info.h"

// Name of UMA User Action recorded when the user confirms replacing.
extern const char kDownloadReplaceActionName[];
// Name of UMA User Action recorded when the user reject replacing.
extern const char kDownloadDoNotReplaceActionName[];

// Confirmation dialog for replacing the download.
// Uses ConfirmationOverlayResponse.
class ConfirmDownloadReplacingRequest
    : public OverlayRequestConfig<ConfirmDownloadReplacingRequest> {
 private:
  void CreateAuxiliaryData(base::SupportsUserData* user_data) override;
  OVERLAY_USER_DATA_SETUP(ConfirmDownloadReplacingRequest);
};

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_CONFIRM_DOWNLOAD_REPLACING_OVERLAY_H_
