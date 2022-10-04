// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PROMOS_MANAGER_STANDARD_PROMO_ACTION_HANDLER_H_
#define IOS_CHROME_BROWSER_UI_PROMOS_MANAGER_STANDARD_PROMO_ACTION_HANDLER_H_

#import <Foundation/Foundation.h>

@protocol StandardPromoActionHandler <NSObject>

// The "Primary Action" was touched.
- (void)standardPromoPrimaryAction;

@optional

// - - - - - - - - - - - - - - - - - - - - - - - - - -
// Button-based actions. (Invoked via a button press.)
// - - - - - - - - - - - - - - - - - - - - - - - - - -

// The "Dismiss" button was touched.
- (void)standardPromoDismissAction;

// The "Secondary Action" was touched.
- (void)standardPromoSecondaryAction;

// The "Learn More" button was touched.
- (void)standardPromoLearnMoreAction;

// The "Tertiary Action" was touched.
- (void)standardPromoTertiaryAction;

// - - - - - - - - - - - - - - - - - - - - - - - -
// Gesture-based actions. (Invoked via a gesture.)
// - - - - - - - - - - - - - - - - - - - - - - - -

// Important: If `standardPromoDismissSwipe` is not implemented, but
// `standardPromoDismissAction` is, `standardPromoDismissAction` will be called
// for both `standardPromoDismissAction` and `standardPromoDismissSwipe`.
// However, if both `standardPromoDismissAction` and `standardPromoDismissSwipe`
// are implemented, they will be called separately at their respective points of
// invocation.
- (void)standardPromoDismissSwipe;

@end

#endif  // IOS_CHROME_BROWSER_UI_PROMOS_MANAGER_STANDARD_PROMO_ACTION_HANDLER_H_
