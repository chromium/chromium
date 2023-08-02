// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/default_promo/tailored_promo_util.h"

#import "base/notreached.h"
#import "ios/chrome/grit/ios_google_chrome_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/branded_images/branded_images_api.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util_mac.h"

using l10n_util::GetNSString;

void SetUpTailoredConsumerWithType(id<TailoredPromoConsumer> consumer,
                                   DefaultPromoType type) {
  NSString* title;
  NSString* subtitle;
  UIImage* image;

  switch (type) {
    case DefaultPromoTypeVideo:
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
      image = ios::provider::GetBrandedImage(
          ios::provider::BrandedImage::kStaySafePromo);
      break;
    case DefaultPromoTypeMadeForIOS:
      if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
        title = GetNSString(
            IDS_IOS_DEFAULT_BROWSER_TAILORED_BUILT_FOR_IPADOS_TITLE);
        image = ios::provider::GetBrandedImage(
            ios::provider::BrandedImage::kMadeForIPadOSPromo);
      } else {
        title =
            GetNSString(IDS_IOS_DEFAULT_BROWSER_TAILORED_BUILT_FOR_IOS_TITLE);
        image = ios::provider::GetBrandedImage(
            ios::provider::BrandedImage::kMadeForIOSPromo);
      }
      subtitle = GetNSString(
          IDS_IOS_DEFAULT_BROWSER_TAILORED_BUILT_FOR_IOS_DESCRIPTION);
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
