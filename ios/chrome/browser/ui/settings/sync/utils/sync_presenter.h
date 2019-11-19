// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_SYNC_UTILS_SYNC_PRESENTER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_SYNC_UTILS_SYNC_PRESENTER_H_

#import <Foundation/Foundation.h>

// Protocol used to display sync-related UI.
@protocol SyncPresenter

// Asks the presenter to display the reauthenticate signin UI.
- (void)showReauthenticateSignin;

// Asks the presenter to display the sync encryption passphrase UI.
- (void)showSyncPassphraseSettings;

// Presents the Google services settings.
- (void)showGoogleServicesSettings;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_SYNC_UTILS_SYNC_PRESENTER_H_
