// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_PRIVACY_GUIDE_PRIVACY_GUIDE_WELCOME_VIEW_CONTROLLER_PRESENTATION_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_PRIVACY_GUIDE_PRIVACY_GUIDE_WELCOME_VIEW_CONTROLLER_PRESENTATION_DELEGATE_H_

@class PrivacyGuideWelcomeViewController;

// Delegate for presentation events related to PrivacyGuideWelcomeViewController
// which is usually handled by a class that holds the view controller.
@protocol PrivacyGuideWelcomeViewControllerPresentationDelegate

// Called when the view controller is removed from its parent.
- (void)privacyGuideWelcomeViewControllerDidRemove:
    (PrivacyGuideWelcomeViewController*)controller;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_PRIVACY_GUIDE_PRIVACY_GUIDE_WELCOME_VIEW_CONTROLLER_PRESENTATION_DELEGATE_H_
