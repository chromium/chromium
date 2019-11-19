// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/public/web_content_area/http_auth_overlay.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

OVERLAY_USER_DATA_SETUP_IMPL(HTTPAuthOverlayRequestConfig);

HTTPAuthOverlayRequestConfig::HTTPAuthOverlayRequestConfig(
    const std::string& message,
    const std::string& default_username)
    : message_(message), default_username_(default_username) {}

HTTPAuthOverlayRequestConfig::~HTTPAuthOverlayRequestConfig() = default;

OVERLAY_USER_DATA_SETUP_IMPL(HTTPAuthOverlayResponseInfo);

HTTPAuthOverlayResponseInfo::HTTPAuthOverlayResponseInfo(
    const std::string& username,
    const std::string& password)
    : username_(username), password_(password) {}

HTTPAuthOverlayResponseInfo::~HTTPAuthOverlayResponseInfo() = default;
