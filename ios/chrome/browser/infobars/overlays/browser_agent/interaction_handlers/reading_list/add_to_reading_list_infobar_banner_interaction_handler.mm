// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/reading_list/add_to_reading_list_infobar_banner_interaction_handler.h"

#include "base/check.h"
#include "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/overlays/public/infobar_banner/add_to_reading_list_infobar_banner_overlay_request_config.h"
#import "ios/chrome/browser/overlays/public/infobar_banner/save_card_infobar_banner_overlay_request_config.h"
#import "ios/chrome/browser/ui/reading_list/ios_add_to_reading_list_infobar_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using reading_list_infobar_overlay::ReadingListBannerRequestConfig;

#pragma mark - InfobarBannerInteractionHandler

AddToReadingListInfobarBannerInteractionHandler::
    AddToReadingListInfobarBannerInteractionHandler(Browser* browser)
    : InfobarBannerInteractionHandler(
          ReadingListBannerRequestConfig::RequestSupport()) {}

AddToReadingListInfobarBannerInteractionHandler::
    ~AddToReadingListInfobarBannerInteractionHandler() = default;

void AddToReadingListInfobarBannerInteractionHandler::MainButtonTapped(
    InfoBarIOS* infobar) {
  GetInfobarDelegate(infobar)->Accept();
}

#pragma mark - Private

IOSAddToReadingListInfobarDelegate*
AddToReadingListInfobarBannerInteractionHandler::GetInfobarDelegate(
    InfoBarIOS* infobar) {
  IOSAddToReadingListInfobarDelegate* delegate =
      IOSAddToReadingListInfobarDelegate::FromInfobarDelegate(
          infobar->delegate());
  DCHECK(delegate);
  return delegate;
}
