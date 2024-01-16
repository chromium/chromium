// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_PRIVACY_GUIDE_PRIVACY_GUIDE_URL_USAGE_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_PRIVACY_GUIDE_PRIVACY_GUIDE_URL_USAGE_COORDINATOR_DELEGATE_H_

@class PrivacyGuideURLUsageCoordinator;

// Delegate for PrivacyGuideURLUsageCoordinator.
@protocol PrivacyGuideURLUsageCoordinatorDelegate

// Called when the coordinator's view controller is removed from the navigation
// controller.
- (void)privacyGuideURLUsageCoordinatorDidRemove:
    (PrivacyGuideURLUsageCoordinator*)coordinator;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_PRIVACY_GUIDE_PRIVACY_GUIDE_URL_USAGE_COORDINATOR_DELEGATE_H_
