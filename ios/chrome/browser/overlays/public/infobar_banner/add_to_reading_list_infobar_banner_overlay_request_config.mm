// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/public/infobar_banner/add_to_reading_list_infobar_banner_overlay_request_config.h"

#include "components/infobars/core/infobar.h"
#include "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/infobars/overlays/infobar_overlay_type.h"
#import "ios/chrome/browser/overlays/public/common/infobars/infobar_overlay_request_config.h"
#import "ios/chrome/browser/ui/reading_list/ios_add_to_reading_list_infobar_delegate.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using infobars::InfoBar;

namespace reading_list_infobar_overlay {

OVERLAY_USER_DATA_SETUP_IMPL(ReadingListBannerRequestConfig);

ReadingListBannerRequestConfig::ReadingListBannerRequestConfig(InfoBar* infobar)
    : infobar_(infobar) {
  DCHECK(infobar_);
  IOSAddToReadingListInfobarDelegate* delegate =
      static_cast<IOSAddToReadingListInfobarDelegate*>(infobar_->delegate());
  int time_to_read = delegate->time_to_read();
  // TODO(crbug.com/1195978): Use localized strings.
  message_text_ = [NSString stringWithFormat:@"%i minute read", time_to_read];
  title_text_ = [NSString stringWithFormat:@"Add to Reading List for Later?"];
  button_text_ = @"Add";
}

ReadingListBannerRequestConfig::~ReadingListBannerRequestConfig() = default;

void ReadingListBannerRequestConfig::CreateAuxiliaryData(
    base::SupportsUserData* user_data) {
  InfobarOverlayRequestConfig::CreateForUserData(
      user_data, static_cast<InfoBarIOS*>(infobar_),
      InfobarOverlayType::kBanner, false);
}

}  // namespace reading_list_infobar_overlay
