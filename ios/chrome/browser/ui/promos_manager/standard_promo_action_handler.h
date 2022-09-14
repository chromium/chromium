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

// The "Dismiss" button was touched.
- (void)standardPromoDismissAction;

// The "Secondary Action" was touched.
- (void)standardPromoSecondaryAction;

// The "Learn More" button was touched.
- (void)standardPromoLearnMoreAction;

// The "Tertiary Action" was touched.
- (void)standardPromoTertiaryAction;

@end

#endif  // IOS_CHROME_BROWSER_UI_PROMOS_MANAGER_STANDARD_PROMO_ACTION_HANDLER_H_
