// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_DOWNLOADS_SAVE_TO_PHOTOS_SAVE_TO_PHOTOS_SETTINGS_MUTATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_DOWNLOADS_SAVE_TO_PHOTOS_SAVE_TO_PHOTOS_SETTINGS_MUTATOR_H_

#import <Foundation/Foundation.h>

@protocol SaveToPhotosSettingsMutator <NSObject>

// Save `gaiaID` to preferences to use the associated identity as a default to
// save images to Google Photos.
- (void)setSelectedIdentityGaiaID:(NSString*)gaiaID;

// Opt-in to use the account picker to choose which account to use every time
// the user saves an image to Google Photos.
- (void)setAskWhichAccountToUseEveryTime:(BOOL)askWhichAccountToUseEveryTime;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_DOWNLOADS_SAVE_TO_PHOTOS_SAVE_TO_PHOTOS_SETTINGS_MUTATOR_H_
