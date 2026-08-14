// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DEFAULT_BROWSER_PROMO_UI_DEFAULT_BROWSER_PASSIVE_CARD_VIEW_DELEGATE_H_
#define IOS_CHROME_BROWSER_DEFAULT_BROWSER_PROMO_UI_DEFAULT_BROWSER_PASSIVE_CARD_VIEW_DELEGATE_H_

#import <UIKit/UIKit.h>

@class DefaultBrowserPassivePromoCardView;

// Delegate protocol to handle button tap events on the
// DefaultBrowserPassivePromoCardView.
@protocol DefaultBrowserPassivePromoCardViewDelegate <NSObject>

// Called by DefaultBrowserPassivePromoCardView when the user taps the close
// button.
- (void)didTapCloseInDefaultBrowserPassivePromoCardView:
    (DefaultBrowserPassivePromoCardView*)view;

// Called by DefaultBrowserPassivePromoCardView when the user taps the action
// button.
- (void)didTapActionInDefaultBrowserPassivePromoCardView:
    (DefaultBrowserPassivePromoCardView*)view;

@end

#endif  // IOS_CHROME_BROWSER_DEFAULT_BROWSER_PROMO_UI_DEFAULT_BROWSER_PASSIVE_CARD_VIEW_DELEGATE_H_
