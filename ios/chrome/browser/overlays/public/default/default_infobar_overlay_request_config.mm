// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/public/default/default_infobar_overlay_request_config.h"

#import "ios/chrome/browser/overlays/public/common/infobars/infobar_overlay_request_config.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

OVERLAY_USER_DATA_SETUP_IMPL(DefaultInfobarOverlayRequestConfig);

DefaultInfobarOverlayRequestConfig::DefaultInfobarOverlayRequestConfig(
    InfoBarIOS* infobar,
    InfobarOverlayType overlay_type)
    : infobar_(infobar), overlay_type_(overlay_type) {
  DCHECK(infobar_);
}

DefaultInfobarOverlayRequestConfig::~DefaultInfobarOverlayRequestConfig() =
    default;

void DefaultInfobarOverlayRequestConfig::CreateAuxiliaryData(
    base::SupportsUserData* user_data) {
  InfobarOverlayRequestConfig::CreateForUserData(user_data, infobar_,
                                                 overlay_type_, false);
}
