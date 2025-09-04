// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_APP_BUNDLE_PROMO_UI_APP_BUNDLE_PROMO_CONFIG_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_APP_BUNDLE_PROMO_UI_APP_BUNDLE_PROMO_CONFIG_H_

#import "ios/chrome/browser/content_suggestions/ui_bundled/magic_stack/magic_stack_module.h"

@protocol AppBundlePromoAudience;

// Config object for the App Bundle promo module.
@interface AppBundlePromoConfig : MagicStackModule

// The object that should handle user events.
@property(nonatomic, weak) id<AppBundlePromoAudience> audience;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_APP_BUNDLE_PROMO_UI_APP_BUNDLE_PROMO_CONFIG_H_
