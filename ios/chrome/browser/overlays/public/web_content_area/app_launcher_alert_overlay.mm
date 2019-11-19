// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/public/web_content_area/app_launcher_alert_overlay.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

OVERLAY_USER_DATA_SETUP_IMPL(AppLauncherAlertOverlayRequestConfig);

AppLauncherAlertOverlayRequestConfig::AppLauncherAlertOverlayRequestConfig(
    bool is_repeated_request)
    : is_repeated_request_(is_repeated_request) {}

AppLauncherAlertOverlayRequestConfig::~AppLauncherAlertOverlayRequestConfig() =
    default;

OVERLAY_USER_DATA_SETUP_IMPL(AppLauncherAlertOverlayResponseInfo);

AppLauncherAlertOverlayResponseInfo::AppLauncherAlertOverlayResponseInfo(
    bool allow_navigation)
    : allow_navigation_(allow_navigation) {}

AppLauncherAlertOverlayResponseInfo::~AppLauncherAlertOverlayResponseInfo() =
    default;
