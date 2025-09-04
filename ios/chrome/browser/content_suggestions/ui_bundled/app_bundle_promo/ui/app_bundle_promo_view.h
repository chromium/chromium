// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_APP_BUNDLE_PROMO_UI_APP_BUNDLE_PROMO_VIEW_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_APP_BUNDLE_PROMO_UI_APP_BUNDLE_PROMO_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/content_suggestions/ui_bundled/app_bundle_promo/ui/app_bundle_promo_audience.h"

@class AppBundlePromoConfig;

@interface AppBundlePromoView : UIView

// Default initializer.
- (instancetype)initWithConfig:(AppBundlePromoConfig*)config;

// The object that should handle user events.
@property(nonatomic, weak) id<AppBundlePromoAudience> audience;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_APP_BUNDLE_PROMO_UI_APP_BUNDLE_PROMO_VIEW_H_
