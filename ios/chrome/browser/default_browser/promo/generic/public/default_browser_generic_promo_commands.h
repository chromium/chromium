// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DEFAULT_BROWSER_PROMO_GENERIC_PUBLIC_DEFAULT_BROWSER_GENERIC_PROMO_COMMANDS_H_
#define IOS_CHROME_BROWSER_DEFAULT_BROWSER_PROMO_GENERIC_PUBLIC_DEFAULT_BROWSER_GENERIC_PROMO_COMMANDS_H_

#import <UIKit/UIKit.h>

@protocol DefaultBrowserGenericPromoCommands <NSObject>

// Command the promo to be hidden.
- (void)hidePromo;

@end

#endif  // IOS_CHROME_BROWSER_DEFAULT_BROWSER_PROMO_GENERIC_PUBLIC_DEFAULT_BROWSER_GENERIC_PROMO_COMMANDS_H_
