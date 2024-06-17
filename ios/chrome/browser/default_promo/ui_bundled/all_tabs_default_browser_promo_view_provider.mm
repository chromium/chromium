// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/default_promo/ui_bundled/all_tabs_default_browser_promo_view_provider.h"

#import "components/feature_engagement/public/feature_constants.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

using l10n_util::GetNSString;

@implementation AllTabsDefaultBrowserPromoViewProvider

- (UIImage*)promoImage {
  return [UIImage imageNamed:@"all_your_tabs"];
}

- (NSString*)promoTitle {
  return GetNSString(IDS_IOS_DEFAULT_BROWSER_TAILORED_ALL_TABS_TITLE);
}

- (NSString*)promoSubtitle {
  return GetNSString(IDS_IOS_DEFAULT_BROWSER_TAILORED_ALL_TABS_DESCRIPTION);
}

- (promos_manager::Promo)promoIdentifier {
  return promos_manager::Promo::AllTabsDefaultBrowser;
}

- (const base::Feature*)featureEngagmentIdentifier {
  return &feature_engagement::kIPHiOSPromoAllTabsFeature;
}

- (DefaultPromoType)defaultBrowserPromoType {
  return DefaultPromoTypeAllTabs;
}
@end
