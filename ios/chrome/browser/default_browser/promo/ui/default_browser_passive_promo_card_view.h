// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DEFAULT_BROWSER_PROMO_UI_DEFAULT_BROWSER_PASSIVE_PROMO_CARD_VIEW_H_
#define IOS_CHROME_BROWSER_DEFAULT_BROWSER_PROMO_UI_DEFAULT_BROWSER_PASSIVE_PROMO_CARD_VIEW_H_

#import <UIKit/UIKit.h>

@protocol DefaultBrowserPassivePromoCardViewDelegate;

// DefaultBrowserPassivePromoCardView is a reusable card view containing an
// illustration, a title, a description, an action button, and a close button.
@interface DefaultBrowserPassivePromoCardView : UIView

// The delegate to handle user interactions on the view's buttons.
@property(nonatomic, weak) id<DefaultBrowserPassivePromoCardViewDelegate>
    delegate;

@end

#endif  // IOS_CHROME_BROWSER_DEFAULT_BROWSER_PROMO_UI_DEFAULT_BROWSER_PASSIVE_PROMO_CARD_VIEW_H_
