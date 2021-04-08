// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/default_promo/tailored_promo_util.h"

#include "base/notreached.h"
#include "ios/chrome/grit/ios_google_chrome_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/images/branded_image_provider.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using l10n_util::GetNSString;

void SetUpTailoredConsumerWithType(id<TailoredPromoConsumer> consumer,
                                   DefaultPromoType type) {
  NSString* title;
  NSString* subtitle;
  UIImage* image;

  switch (type) {
    case DefaultPromoTypeGeneral:
      NOTREACHED();  // This type is not supported.
      break;
    case DefaultPromoTypeAllTabs:
      title = GetNSString(IDS_IOS_DEFAULT_BROWSER_TAILORED_ALL_TABS_TITLE);
      subtitle =
          GetNSString(IDS_IOS_DEFAULT_BROWSER_TAILORED_ALL_TABS_DESCRIPTION);
      image = [UIImage imageNamed:@"all_your_tabs"];
      break;
    case DefaultPromoTypeStaySafe:
      title = GetNSString(IDS_IOS_DEFAULT_BROWSER_TAILORED_STAY_SAFE_TITLE);
      subtitle =
          GetNSString(IDS_IOS_DEFAULT_BROWSER_TAILORED_STAY_SAFE_DESCRIPTION);
      image = ios::GetChromeBrowserProvider()
                  ->GetBrandedImageProvider()
                  ->GetStaySafePromoImage();
      break;
    case DefaultPromoTypeMadeForIOS:
      title = GetNSString(IDS_IOS_DEFAULT_BROWSER_TAILORED_BUILT_FOR_IOS_TITLE);
      subtitle = GetNSString(
          IDS_IOS_DEFAULT_BROWSER_TAILORED_BUILT_FOR_IOS_DESCRIPTION);
      image = ios::GetChromeBrowserProvider()
                  ->GetBrandedImageProvider()
                  ->GetMadeForIOSPromoImage();
      break;
  }

  consumer.image = image;
  consumer.titleString = title;
  consumer.subtitleString = subtitle;

  consumer.primaryActionString =
      GetNSString(IDS_IOS_DEFAULT_BROWSER_TAILORED_PRIMARY_BUTTON_TEXT);
  consumer.secondaryActionString =
      GetNSString(IDS_IOS_DEFAULT_BROWSER_SECONDARY_BUTTON_TEXT);
}
