// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_WEB_CONTENT_AREA_APP_LAUNCHER_ALERT_OVERLAY_H_
#define IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_WEB_CONTENT_AREA_APP_LAUNCHER_ALERT_OVERLAY_H_

#include "ios/chrome/browser/overlays/public/overlay_user_data.h"

// Configuration object for OverlayRequests for alerts notifying the user that
// a navigation will open another app.
class AppLauncherAlertOverlayRequestConfig
    : public OverlayUserData<AppLauncherAlertOverlayRequestConfig> {
 public:
  ~AppLauncherAlertOverlayRequestConfig() override;

  // Whether the current page has previously attempted to open another app.
  bool is_repeated_request() const { return is_repeated_request_; }

 private:
  OVERLAY_USER_DATA_SETUP(AppLauncherAlertOverlayRequestConfig);
  AppLauncherAlertOverlayRequestConfig(bool is_repeated_request);

  const bool is_repeated_request_;
};

// User interaction info for OverlayResponses for app launcher alerts.
class AppLauncherAlertOverlayResponseInfo
    : public OverlayUserData<AppLauncherAlertOverlayResponseInfo> {
 public:
  ~AppLauncherAlertOverlayResponseInfo() override;

  // Whether the user has chosen to allow navigation to another app.
  bool allow_navigation() const { return allow_navigation_; }

 private:
  OVERLAY_USER_DATA_SETUP(AppLauncherAlertOverlayResponseInfo);
  AppLauncherAlertOverlayResponseInfo(bool allow_navigation);

  const bool allow_navigation_;
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_WEB_CONTENT_AREA_APP_LAUNCHER_ALERT_OVERLAY_H_
