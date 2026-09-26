// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_EPHEMERAL_THEME_PROMO_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_EPHEMERAL_THEME_PROMO_COMMANDS_H_

#import <Foundation/Foundation.h>

// Commands to display and hide the New Tab Page ephemeral theme promo.
@protocol EphemeralThemePromoCommands <NSObject>

// Shows the ephemeral theme promo bottom sheet.
- (void)showEphemeralThemePromo;

// Hides the ephemeral theme promo bottom sheet.
- (void)hideEphemeralThemePromo;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_EPHEMERAL_THEME_PROMO_COMMANDS_H_
