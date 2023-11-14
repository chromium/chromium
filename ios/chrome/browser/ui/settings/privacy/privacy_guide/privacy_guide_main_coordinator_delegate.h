// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_PRIVACY_GUIDE_PRIVACY_GUIDE_MAIN_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_PRIVACY_GUIDE_PRIVACY_GUIDE_MAIN_COORDINATOR_DELEGATE_H_

@class PrivacyGuideMainCoordinator;

// Delegate for PrivacyGuideMainCoordinator.
@protocol PrivacyGuideMainCoordinatorDelegate

// Called when all view controllers are removed from navigation controller.
- (void)privacyGuideMainCoordinatorDidRemove:
    (PrivacyGuideMainCoordinator*)coordinator;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_PRIVACY_GUIDE_PRIVACY_GUIDE_MAIN_COORDINATOR_DELEGATE_H_
