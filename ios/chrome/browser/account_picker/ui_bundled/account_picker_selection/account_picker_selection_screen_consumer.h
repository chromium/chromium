// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ACCOUNT_PICKER_UI_BUNDLED_ACCOUNT_PICKER_SELECTION_ACCOUNT_PICKER_SELECTION_SCREEN_CONSUMER_H_
#define IOS_CHROME_BROWSER_ACCOUNT_PICKER_UI_BUNDLED_ACCOUNT_PICKER_SELECTION_ACCOUNT_PICKER_SELECTION_SCREEN_CONSUMER_H_

#import <Foundation/Foundation.h>

@class AccountPickerSelectionScreenIdentityItemConfigurator;

// Consumer for consistency default account.
@protocol AccountPickerSelectionScreenConsumer <NSObject>

// Invoked when all identities have to be reloaded.
- (void)reloadAllIdentities;
// Invoked when an identity has to be updated.
- (void)reloadIdentityForIdentityItemConfigurator:
    (AccountPickerSelectionScreenIdentityItemConfigurator*)configurator;

@end

#endif  // IOS_CHROME_BROWSER_ACCOUNT_PICKER_UI_BUNDLED_ACCOUNT_PICKER_SELECTION_ACCOUNT_PICKER_SELECTION_SCREEN_CONSUMER_H_
