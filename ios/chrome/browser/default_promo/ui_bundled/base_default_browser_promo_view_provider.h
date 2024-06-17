// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DEFAULT_PROMO_UI_BUNDLED_BASE_DEFAULT_BROWSER_PROMO_VIEW_PROVIDER_H_
#define IOS_CHROME_BROWSER_DEFAULT_PROMO_UI_BUNDLED_BASE_DEFAULT_BROWSER_PROMO_VIEW_PROVIDER_H_

#import "base/feature_list.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/promos_manager/model/constants.h"
#import "ios/chrome/browser/ui/promos_manager/standard_promo_view_provider.h"

// Base provider for displaying the Default Browser Promo. Should not be
// instantiated.
@interface BaseDefaultBrowserPromoViewProvider
    : NSObject <StandardPromoViewProvider>

// Should be implemented in subclassses.
- (UIImage*)promoImage;

// Should be implemented in subclassses.
- (NSString*)promoTitle;

// Should be implemented in subclassses.
- (NSString*)promoSubtitle;

// Should be implemented in subclassses.
- (promos_manager::Promo)promoIdentifier;

// Should be implemented in subclassses.
- (const base::Feature*)featureEngagmentIdentifier;

// Should be implemented in subclasses.
- (DefaultPromoType)defaultBrowserPromoType;

// Delegate callback to tell the provider that the promo was displayed.
- (void)promoWasDisplayed;

@end

#endif  // IOS_CHROME_BROWSER_DEFAULT_PROMO_UI_BUNDLED_BASE_DEFAULT_BROWSER_PROMO_VIEW_PROVIDER_H_
