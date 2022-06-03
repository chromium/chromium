// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_WEB_CONTENT_AREA_APP_LAUNCHER_OVERLAY_H_
#define IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_WEB_CONTENT_AREA_APP_LAUNCHER_OVERLAY_H_

#include "ios/chrome/browser/overlays/public/overlay_request_config.h"
#include "ios/chrome/browser/overlays/public/overlay_response_info.h"

namespace app_launcher_overlays {

// Configuration object for OverlayRequests for alerts notifying the user that
// a navigation will open another app.
class AppLaunchConfirmationRequest
    : public OverlayRequestConfig<AppLaunchConfirmationRequest> {
 public:
  ~AppLaunchConfirmationRequest() override;

  // Whether the current page has previously attempted to open another app.
  bool is_repeated_request() const { return is_repeated_request_; }

 private:
  OVERLAY_USER_DATA_SETUP(AppLaunchConfirmationRequest);
  AppLaunchConfirmationRequest(bool is_repeated_request);

  // OverlayUserData:
  void CreateAuxiliaryData(base::SupportsUserData* user_data) override;

  const bool is_repeated_request_;
};

// Completion response used when the user allows the app launcher navigation.
DEFINE_STATELESS_OVERLAY_RESPONSE_INFO(AllowAppLaunchResponse);

}  // namespace app_launcher_overlays

#endif  // IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_WEB_CONTENT_AREA_APP_LAUNCHER_OVERLAY_H_
