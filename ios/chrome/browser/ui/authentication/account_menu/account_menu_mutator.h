// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_ACCOUNT_MENU_ACCOUNT_MENU_MUTATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_ACCOUNT_MENU_ACCOUNT_MENU_MUTATOR_H_

#import <Foundation/Foundation.h>

// Mutator for account menu.
@protocol AccountMenuMutator <NSObject>

// The user tapped on the `index`-th account.
- (void)accountTappedWithGaiaID:(NSString*)index targetRect:(CGRect)targetRect;

// The user tapped on the error button.
- (void)didTapErrorButton;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_ACCOUNT_MENU_ACCOUNT_MENU_MUTATOR_H_
