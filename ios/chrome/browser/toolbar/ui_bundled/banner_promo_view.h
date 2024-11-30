// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_BANNER_PROMO_VIEW_H_
#define IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_BANNER_PROMO_VIEW_H_

#import <UIKit/UIKit.h>

// View to display a banner promo in the toolbar.
@interface BannerPromoView : UIView

- (instancetype)init NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_BANNER_PROMO_VIEW_H_
