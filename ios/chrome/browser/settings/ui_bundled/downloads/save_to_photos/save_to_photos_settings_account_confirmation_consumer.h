// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_DOWNLOADS_SAVE_TO_PHOTOS_SAVE_TO_PHOTOS_SETTINGS_ACCOUNT_CONFIRMATION_CONSUMER_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_DOWNLOADS_SAVE_TO_PHOTOS_SAVE_TO_PHOTOS_SETTINGS_ACCOUNT_CONFIRMATION_CONSUMER_H_

#import <UIKit/UIKit.h>

class GaiaId;

// Protocol for the first screen of Save to Photos settings.
@protocol SaveToPhotosSettingsAccountConfirmationConsumer <NSObject>

// Sets the values of the identity button and the value of the "Ask which
// account to use every time" switch.
- (void)setIdentityButtonAvatar:(UIImage*)avatar
                           name:(NSString*)name
                          email:(NSString*)email
                         gaiaID:(const GaiaId&)gaiaID
           askEveryTimeSwitchOn:(BOOL)on;

// Displays the Save to Photos UI in the Downloads settings menu.
- (void)displaySaveToPhotosSettingsUI;

// Hides the Save to Photos UI in the Downloads settings menu.
- (void)hideSaveToPhotosSettingsUI;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_DOWNLOADS_SAVE_TO_PHOTOS_SAVE_TO_PHOTOS_SETTINGS_ACCOUNT_CONFIRMATION_CONSUMER_H_
