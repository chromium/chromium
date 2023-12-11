// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_WEB_CONTENT_AREA_INSECURE_FORM_OVERLAY_H_
#define IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_WEB_CONTENT_AREA_INSECURE_FORM_OVERLAY_H_

#include "ios/chrome/browser/overlays/model/public/overlay_request_config.h"
#include "ios/chrome/browser/overlays/model/public/overlay_response_info.h"

// Configuration object for OverlayRequests for Insecure Form warnings (i.e.
// forms posted from HTTPS to HTTP URLs).
class InsecureFormOverlayRequestConfig
    : public OverlayRequestConfig<InsecureFormOverlayRequestConfig> {
 public:
  ~InsecureFormOverlayRequestConfig() override;

 private:
  OVERLAY_USER_DATA_SETUP(InsecureFormOverlayRequestConfig);
  InsecureFormOverlayRequestConfig();

  // OverlayUserData:
  void CreateAuxiliaryData(base::SupportsUserData* user_data) override;
};

// Response type used for Insecure Form dialogs.
class InsecureFormDialogResponse
    : public OverlayResponseInfo<InsecureFormDialogResponse> {
 public:
  ~InsecureFormDialogResponse() override;
  // Whether the user has allowed the form to be submitted.
  bool allow_send() const { return allow_send_; }

 private:
  OVERLAY_USER_DATA_SETUP(InsecureFormDialogResponse);
  InsecureFormDialogResponse(bool allow_send);
  const bool allow_send_;
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_WEB_CONTENT_AREA_INSECURE_FORM_OVERLAY_H_
