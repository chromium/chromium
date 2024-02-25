// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_DOWNLOADS_SAVE_TO_PHOTOS_FAKE_SAVE_TO_PHOTOS_SETTINGS_MUTATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_DOWNLOADS_SAVE_TO_PHOTOS_FAKE_SAVE_TO_PHOTOS_SETTINGS_MUTATOR_H_

#import "ios/chrome/browser/ui/settings/downloads/save_to_photos/save_to_photos_settings_mutator.h"

// Fake implementation for the SaveToPhotosSettingsMutator protocol.
@interface FakeSaveToPhotosSettingsMutator
    : NSObject <SaveToPhotosSettingsMutator>

@property(nonatomic, copy) NSString* selectedIdentityGaiaID;

@property(nonatomic, assign) BOOL askWhichAccountToUseEveryTime;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_DOWNLOADS_SAVE_TO_PHOTOS_FAKE_SAVE_TO_PHOTOS_SETTINGS_MUTATOR_H_
