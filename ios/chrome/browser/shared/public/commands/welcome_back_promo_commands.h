// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_WELCOME_BACK_PROMO_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_WELCOME_BACK_PROMO_COMMANDS_H_

#import <UIKit/UIKit.h>

// Commands for interacting with the Welcome Back promo.
@protocol WelcomeBackPromoCommands <NSObject>

// Dismisses the Welcome Back promo.
- (void)hideWelcomeBackPromo;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_WELCOME_BACK_PROMO_COMMANDS_H_
