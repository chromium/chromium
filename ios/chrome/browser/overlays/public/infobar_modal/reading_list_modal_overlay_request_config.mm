// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/public/infobar_modal/reading_list_modal_overlay_request_config.h"

#import "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/overlays/public/common/infobars/infobar_overlay_request_config.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

OVERLAY_USER_DATA_SETUP_IMPL(ReadingListInfobarModalOverlayRequestConfig);

ReadingListInfobarModalOverlayRequestConfig::
    ReadingListInfobarModalOverlayRequestConfig(InfoBarIOS* infobar)
    : infobar_(infobar) {
  DCHECK(infobar_);
  current_page_added_ = infobar->accepted();
}

ReadingListInfobarModalOverlayRequestConfig::
    ~ReadingListInfobarModalOverlayRequestConfig() = default;

void ReadingListInfobarModalOverlayRequestConfig::CreateAuxiliaryData(
    base::SupportsUserData* user_data) {
  InfobarOverlayRequestConfig::CreateForUserData(
      user_data, infobar_, InfobarOverlayType::kModal, false);
}
