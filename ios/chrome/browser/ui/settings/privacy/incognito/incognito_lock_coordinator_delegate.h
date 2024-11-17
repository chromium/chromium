// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_INCOGNITO_INCOGNITO_LOCK_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_INCOGNITO_INCOGNITO_LOCK_COORDINATOR_DELEGATE_H_

@class IncognitoLockCoordinator;

// Delegate for IncognitoLockCoordinator.
@protocol IncognitoLockCoordinatorDelegate

// Called when the view controller is removed from navigation controller.
- (void)incognitoLockCoordinatorDidRemove:
    (IncognitoLockCoordinator*)coordinator;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_INCOGNITO_INCOGNITO_LOCK_COORDINATOR_DELEGATE_H_
