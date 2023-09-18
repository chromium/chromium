// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_DOWNLOADS_SAVE_TO_PHOTOS_SAVE_TO_PHOTOS_SETTINGS_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_DOWNLOADS_SAVE_TO_PHOTOS_SAVE_TO_PHOTOS_SETTINGS_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/settings/downloads/save_to_photos/save_to_photos_settings_mutator.h"

class ChromeAccountManagerService;
class PrefService;
@protocol SaveToPhotosSettingsMediatorDelegate;
@protocol SaveToPhotosSettingsAccountConfirmationConsumer;
@protocol SaveToPhotosSettingsAccountSelectionConsumer;

namespace signin {
class IdentityManager;
}  // namespace signin

// Mediator for Save to Photos settings.
@interface SaveToPhotosSettingsMediator : NSObject <SaveToPhotosSettingsMutator>

// Delegate.
@property(nonatomic, weak) id<SaveToPhotosSettingsMediatorDelegate> delegate;

// Two consumers (for two different screens).
@property(nonatomic, weak) id<SaveToPhotosSettingsAccountConfirmationConsumer>
    accountConfirmationConsumer;
@property(nonatomic, weak) id<SaveToPhotosSettingsAccountSelectionConsumer>
    accountSelectionConsumer;

// Initialization.
- (instancetype)initWithAccountManagerService:
                    (ChromeAccountManagerService*)accountManagerService
                                  prefService:(PrefService*)prefService
                              identityManager:
                                  (signin::IdentityManager*)identityManager
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_DOWNLOADS_SAVE_TO_PHOTOS_SAVE_TO_PHOTOS_SETTINGS_MEDIATOR_H_
