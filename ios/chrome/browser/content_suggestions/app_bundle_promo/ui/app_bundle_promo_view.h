// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_APP_BUNDLE_PROMO_UI_APP_BUNDLE_PROMO_VIEW_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_APP_BUNDLE_PROMO_UI_APP_BUNDLE_PROMO_VIEW_H_

#import <UIKit/UIKit.h>

@protocol AppBundlePromoAudience;
@class AppBundlePromoConfig;

// A view displaying the App Bundle promo module in the Magic Stack.
@interface AppBundlePromoView : UIView

// Default initializer with `config`.
- (instancetype)initWithConfig:(AppBundlePromoConfig*)config
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;

// The object that should handle user events.
@property(nonatomic, weak) id<AppBundlePromoAudience> audience;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_APP_BUNDLE_PROMO_UI_APP_BUNDLE_PROMO_VIEW_H_
