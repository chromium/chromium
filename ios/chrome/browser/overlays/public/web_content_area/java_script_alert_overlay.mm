// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/public/web_content_area/java_script_alert_overlay.h"

#include "base/logging.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

OVERLAY_USER_DATA_SETUP_IMPL(JavaScriptAlertOverlayRequestConfig);

JavaScriptAlertOverlayRequestConfig::JavaScriptAlertOverlayRequestConfig(
    const JavaScriptDialogSource& source,
    const std::string& message)
    : source_(source), message_(message) {}

JavaScriptAlertOverlayRequestConfig::~JavaScriptAlertOverlayRequestConfig() =
    default;
