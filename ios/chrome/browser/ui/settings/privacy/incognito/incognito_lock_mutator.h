// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_INCOGNITO_INCOGNITO_LOCK_MUTATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_INCOGNITO_INCOGNITO_LOCK_MUTATOR_H_

#import "ios/chrome/browser/incognito_reauth/ui_bundled/incognito_reauth_constants.h"

// Mutator for the IncognitoLockViewController to update the
// IncognitoLockMediator.
@protocol IncognitoLockMutator <NSObject>

// Updates the incognito lock prefs based on the lock state.
- (void)updateIncognitoLockState:(IncognitoLockState)state;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_INCOGNITO_INCOGNITO_LOCK_MUTATOR_H_
