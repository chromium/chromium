// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/model/public/default/default_infobar_overlay_request_config.h"

#import "ios/chrome/browser/overlays/model/public/common/infobars/infobar_overlay_request_config.h"

OVERLAY_USER_DATA_SETUP_IMPL(DefaultInfobarOverlayRequestConfig);

DefaultInfobarOverlayRequestConfig::DefaultInfobarOverlayRequestConfig(
    InfoBarIOS* infobar,
    InfobarOverlayType overlay_type)
    : weak_infobar_(infobar->GetWeakPtr()), overlay_type_(overlay_type) {
  DCHECK(weak_infobar_.get());
}

infobars::InfoBarDelegate* DefaultInfobarOverlayRequestConfig::delegate()
    const {
  return weak_infobar_ ? weak_infobar_->delegate() : nullptr;
}

DefaultInfobarOverlayRequestConfig::~DefaultInfobarOverlayRequestConfig() =
    default;

void DefaultInfobarOverlayRequestConfig::CreateAuxiliaryData(
    base::SupportsUserData* user_data) {
  InfobarOverlayRequestConfig::CreateForUserData(user_data, weak_infobar_.get(),
                                                 overlay_type_, false);
}
