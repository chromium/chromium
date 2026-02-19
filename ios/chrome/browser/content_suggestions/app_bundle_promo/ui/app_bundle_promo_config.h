// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_APP_BUNDLE_PROMO_UI_APP_BUNDLE_PROMO_CONFIG_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_APP_BUNDLE_PROMO_UI_APP_BUNDLE_PROMO_CONFIG_H_

#import "ios/chrome/browser/content_suggestions/ui/cells/icon_detail_view.h"
#import "ios/chrome/browser/content_suggestions/ui/cells/icon_detail_view_config.h"

@protocol AppBundlePromoAudience;

// Config object for the App Bundle promo module.
@interface AppBundlePromoConfig
    : IconDetailViewConfig <IconDetailViewTapDelegate>

// The name of the image resource being used for the promo card's icon.
@property(nonatomic, copy) NSString* imageName;

// The object that should handle user events.
@property(nonatomic, weak) id<AppBundlePromoAudience> audience;

// Initializes config with an image file named `imageName`.
- (instancetype)initWithImageNamed:(NSString*)imageName
    NS_DESIGNATED_INITIALIZER;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_APP_BUNDLE_PROMO_UI_APP_BUNDLE_PROMO_CONFIG_H_
