// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/public/infobar_banner/add_to_reading_list_infobar_banner_overlay_request_config.h"

#import "base/strings/sys_string_conversions.h"
#import "components/infobars/core/infobar.h"
#import "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/infobars/overlays/infobar_overlay_type.h"
#import "ios/chrome/browser/overlays/public/common/infobars/infobar_overlay_request_config.h"
#import "ios/chrome/browser/ui/reading_list/ios_add_to_reading_list_infobar_delegate.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

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
  int time_to_read = delegate->estimated_read_time();

  NSString* time_to_read_string =
      [NSString stringWithFormat:@"%i", time_to_read];
  message_text_ =
      l10n_util::GetNSStringF(IDS_IOS_READING_LIST_MESSAGES_BANNER_SUBTITLE,
                              base::SysNSStringToUTF16(time_to_read_string));
  title_text_ =
      l10n_util::GetNSString(IDS_IOS_READING_LIST_MESSAGES_BANNER_TITLE);
  button_text_ =
      l10n_util::GetNSString(IDS_IOS_READING_LIST_MESSAGES_MAIN_ACTION);
}

ReadingListBannerRequestConfig::~ReadingListBannerRequestConfig() = default;

void ReadingListBannerRequestConfig::CreateAuxiliaryData(
    base::SupportsUserData* user_data) {
  InfobarOverlayRequestConfig::CreateForUserData(
      user_data, static_cast<InfoBarIOS*>(infobar_),
      InfobarOverlayType::kBanner, false);
}

}  // namespace reading_list_infobar_overlay
