// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_WEB_CONTENT_AREA_APP_LAUNCHER_OVERLAY_H_
#define IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_WEB_CONTENT_AREA_APP_LAUNCHER_OVERLAY_H_

#include "ios/chrome/browser/overlays/model/public/overlay_request_config.h"
#include "ios/chrome/browser/overlays/model/public/overlay_response_info.h"

namespace app_launcher_overlays {

// The reason why the confirmation request was created.
enum class AppLaunchConfirmationRequestCause {
  kOther,
  kRepeatedRequest,
  kOpenFromIncognito,
  kNoUserInteraction,
  kAppLaunchFailed,
};

// Configuration object for OverlayRequests for alerts notifying the user that
// a navigation will open another app.
class AppLaunchConfirmationRequest
    : public OverlayRequestConfig<AppLaunchConfirmationRequest> {
 public:
  ~AppLaunchConfirmationRequest() override;

  // The reason why a confirmation dialog was displayed.
  AppLaunchConfirmationRequestCause cause() const { return cause_; }

 private:
  OVERLAY_USER_DATA_SETUP(AppLaunchConfirmationRequest);
  AppLaunchConfirmationRequest(AppLaunchConfirmationRequestCause cause);

  // OverlayUserData:
  void CreateAuxiliaryData(base::SupportsUserData* user_data) override;

  const AppLaunchConfirmationRequestCause cause_;
};

// Completion response used when the user allows the app launcher navigation.
DEFINE_STATELESS_OVERLAY_RESPONSE_INFO(AllowAppLaunchResponse);

}  // namespace app_launcher_overlays

#endif  // IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_WEB_CONTENT_AREA_APP_LAUNCHER_OVERLAY_H_
