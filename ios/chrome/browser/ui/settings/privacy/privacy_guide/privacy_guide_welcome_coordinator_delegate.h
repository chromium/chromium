// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_PRIVACY_GUIDE_PRIVACY_GUIDE_WELCOME_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_PRIVACY_GUIDE_PRIVACY_GUIDE_WELCOME_COORDINATOR_DELEGATE_H_

@class PrivacyGuideWelcomeCoordinator;

// Delegate for PrivacyGuideWelcomeCoordinator.
@protocol PrivacyGuideWelcomeCoordinatorDelegate

// Called when the view controller is removed from navigation controller.
- (void)privacyGuideWelcomeCoordinatorDidRemove:
    (PrivacyGuideWelcomeCoordinator*)coordinator;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_PRIVACY_GUIDE_PRIVACY_GUIDE_WELCOME_COORDINATOR_DELEGATE_H_
