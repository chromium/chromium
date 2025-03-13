// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_NON_MODAL_SIGNIN_PROMO_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_NON_MODAL_SIGNIN_PROMO_COMMANDS_H_

#import <Foundation/Foundation.h>

enum class SignInPromoType;

// Commands related to the Non-Modal Sign-In Promo.
@protocol NonModalSignInPromoCommands <NSObject>

// Triggers the business logic to display the non-modal sign-in promo with the
// given type. The mediator may introduce a delay before actually showing the
// promo.
- (void)showNonModalSignInPromoWithType:(SignInPromoType)promoType;

// Dismisses the non-modal sign-in promo.
- (void)dismissNonModalSignInPromo;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_NON_MODAL_SIGNIN_PROMO_COMMANDS_H_
