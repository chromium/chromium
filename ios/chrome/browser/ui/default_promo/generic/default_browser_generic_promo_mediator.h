// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_DEFAULT_PROMO_GENERIC_DEFAULT_BROWSER_GENERIC_PROMO_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_DEFAULT_PROMO_GENERIC_DEFAULT_BROWSER_GENERIC_PROMO_MEDIATOR_H_

#import <UIKit/UIKit.h>

// The mediator for the generic default browser promo.
@interface DefaultBrowserGenericPromoMediator : NSObject

// Handles user tap on primary action.
- (void)didTapPrimaryActionButton;

@end

#endif  // IOS_CHROME_BROWSER_UI_DEFAULT_PROMO_GENERIC_DEFAULT_BROWSER_GENERIC_PROMO_MEDIATOR_H_
