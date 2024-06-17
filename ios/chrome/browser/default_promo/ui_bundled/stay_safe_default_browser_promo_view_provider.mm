// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/default_promo/ui_bundled/stay_safe_default_browser_promo_view_provider.h"

#import "components/feature_engagement/public/feature_constants.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/branded_images/branded_images_api.h"
#import "ui/base/l10n/l10n_util_mac.h"

using l10n_util::GetNSString;

@implementation StaySafeDefaultBrowserPromoViewProvider

- (UIImage*)promoImage {
  return ios::provider::GetBrandedImage(
      ios::provider::BrandedImage::kStaySafePromo);
}

- (NSString*)promoTitle {
  return GetNSString(IDS_IOS_DEFAULT_BROWSER_TAILORED_STAY_SAFE_TITLE);
}

- (NSString*)promoSubtitle {
  return GetNSString(IDS_IOS_DEFAULT_BROWSER_TAILORED_STAY_SAFE_DESCRIPTION);
}

- (promos_manager::Promo)promoIdentifier {
  return promos_manager::Promo::StaySafeDefaultBrowser;
}

- (const base::Feature*)featureEngagmentIdentifier {
  return &feature_engagement::kIPHiOSPromoStaySafeFeature;
}

- (DefaultPromoType)defaultBrowserPromoType {
  return DefaultPromoTypeStaySafe;
}
@end
