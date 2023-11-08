// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_MODEL_CONFIRM_DOWNLOAD_CLOSING_OVERLAY_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_MODEL_CONFIRM_DOWNLOAD_CLOSING_OVERLAY_H_

#include "ios/chrome/browser/overlays/model/public/overlay_request_config.h"
#include "ios/chrome/browser/overlays/model/public/overlay_response_info.h"

// Name of UMA User Action recorded when the user confirms closing.
extern const char kDownloadCloseActionName[];
// Name of UMA User Action recorded when the user reject closing.
extern const char kDownloadDoNotCloseActionName[];

// Confirmation dialog for closing the download.
// Uses ConfirmationOverlayResponse.
class ConfirmDownloadClosingRequest
    : public OverlayRequestConfig<ConfirmDownloadClosingRequest> {
 private:
  void CreateAuxiliaryData(base::SupportsUserData* user_data) override;
  OVERLAY_USER_DATA_SETUP(ConfirmDownloadClosingRequest);
};

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_MODEL_CONFIRM_DOWNLOAD_CLOSING_OVERLAY_H_
