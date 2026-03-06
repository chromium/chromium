// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_PRIVACY_INCOGNITO_INCOGNITO_LOCK_MUTATOR_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_PRIVACY_INCOGNITO_INCOGNITO_LOCK_MUTATOR_H_

#import "ios/chrome/browser/shared/coordinator/scene/state/incognito_lock_state.h"

// Mutator for the IncognitoLockViewController to update the
// IncognitoLockMediator.
@protocol IncognitoLockMutator <NSObject>

// Updates the incognito lock prefs based on the lock state.
- (void)updateIncognitoLockState:(IncognitoLockState)state;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_PRIVACY_INCOGNITO_INCOGNITO_LOCK_MUTATOR_H_
