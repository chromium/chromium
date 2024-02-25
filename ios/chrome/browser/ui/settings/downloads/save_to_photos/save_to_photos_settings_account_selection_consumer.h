// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_DOWNLOADS_SAVE_TO_PHOTOS_SAVE_TO_PHOTOS_SETTINGS_ACCOUNT_SELECTION_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_DOWNLOADS_SAVE_TO_PHOTOS_SAVE_TO_PHOTOS_SETTINGS_ACCOUNT_SELECTION_CONSUMER_H_

#import <Foundation/Foundation.h>

@class AccountPickerSelectionScreenIdentityItemConfigurator;

// Protocol for the second screen of Save to Photos settings.
@protocol SaveToPhotosSettingsAccountSelectionConsumer <NSObject>

// Sets the presented list of accounts on the device from which the user can
// choose a default Save to Photos account.
- (void)populateAccountsOnDevice:
    (NSArray<AccountPickerSelectionScreenIdentityItemConfigurator*>*)
        configurators;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_DOWNLOADS_SAVE_TO_PHOTOS_SAVE_TO_PHOTOS_SETTINGS_ACCOUNT_SELECTION_CONSUMER_H_
