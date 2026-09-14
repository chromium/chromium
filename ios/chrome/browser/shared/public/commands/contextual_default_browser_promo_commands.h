// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_CONTEXTUAL_DEFAULT_BROWSER_PROMO_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_CONTEXTUAL_DEFAULT_BROWSER_PROMO_COMMANDS_H_

#import <Foundation/Foundation.h>

enum class ContextualDefaultBrowserPromoType;

// Commands to display contextual default browser promos.
@protocol ContextualDefaultBrowserPromoCommands <NSObject>

// Shows a specific contextual default browser promo `promoType`.
- (void)showContextualDefaultBrowserPromoWithType:
    (ContextualDefaultBrowserPromoType)promoType;

// Hides the contextual default browser promo.
- (void)hideContextualDefaultBrowserPromo;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_CONTEXTUAL_DEFAULT_BROWSER_PROMO_COMMANDS_H_
