// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/public/infobar_modal/permissions/permissions_modal_overlay_request_config.h"

#include "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/overlays/public/common/infobars/infobar_overlay_request_config.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

OVERLAY_USER_DATA_SETUP_IMPL(PermissionsInfobarModalOverlayRequestConfig);

PermissionsInfobarModalOverlayRequestConfig::
    PermissionsInfobarModalOverlayRequestConfig(InfoBarIOS* infobar)
    : infobar_(infobar) {
  DCHECK(infobar_);
  // TODO(crbug.com/1289645): Retrieve the current webstate from the permissions
  // delegate once implemented.
}

PermissionsInfobarModalOverlayRequestConfig::
    ~PermissionsInfobarModalOverlayRequestConfig() = default;

web::WebState* PermissionsInfobarModalOverlayRequestConfig::GetWebState()
    const {
  return web_state_;
}

void PermissionsInfobarModalOverlayRequestConfig::CreateAuxiliaryData(
    base::SupportsUserData* user_data) {
  InfobarOverlayRequestConfig::CreateForUserData(
      user_data, infobar_, InfobarOverlayType::kModal, false);
}
