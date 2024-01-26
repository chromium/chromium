// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_PRIVACY_GUIDE_PRIVACY_GUIDE_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_PRIVACY_GUIDE_PRIVACY_GUIDE_COORDINATOR_DELEGATE_H_

@class ChromeCoordinator;

// Delegate for Privacy Guide coordinators.
@protocol PrivacyGuideCoordinatorDelegate

// Called when the coordinator's view controller is removed from the navigation
// controller.
- (void)privacyGuideCoordinatorDidRemove:(ChromeCoordinator*)coordinator;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_PRIVACY_GUIDE_PRIVACY_GUIDE_COORDINATOR_DELEGATE_H_
