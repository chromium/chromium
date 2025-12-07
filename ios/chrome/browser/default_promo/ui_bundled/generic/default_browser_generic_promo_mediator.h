// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DEFAULT_PROMO_UI_BUNDLED_GENERIC_DEFAULT_BROWSER_GENERIC_PROMO_MEDIATOR_H_
#define IOS_CHROME_BROWSER_DEFAULT_PROMO_UI_BUNDLED_GENERIC_DEFAULT_BROWSER_GENERIC_PROMO_MEDIATOR_H_

#import <UIKit/UIKit.h>

// The mediator for the generic default browser promo.
@interface DefaultBrowserGenericPromoMediator : NSObject

// Handles user tap on primary action. Sends the user to the iOS settings. If
// `useDefaultAppsDestination` is set to YES, and the current device supports
// it, the user is sent to the Default Apps settings page rather than the
// Chromium settings.
- (void)didTapPrimaryActionButton:(BOOL)useDefaultAppsDestination;

@end

#endif  // IOS_CHROME_BROWSER_DEFAULT_PROMO_UI_BUNDLED_GENERIC_DEFAULT_BROWSER_GENERIC_PROMO_MEDIATOR_H_
