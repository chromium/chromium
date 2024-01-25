// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_PRIVACY_GUIDE_PRIVACY_GUIDE_VIEW_CONTROLLER_PRESENTATION_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_PRIVACY_GUIDE_PRIVACY_GUIDE_VIEW_CONTROLLER_PRESENTATION_DELEGATE_H_

#include <UIKit/UIKit.h>

// Delegate for presentation events related to any Privacy Guide view
// controller, which are handled by a class that holds the view controller.
@protocol PrivacyGuideViewControllerPresentationDelegate <NSObject>

// Called when the view controller is removed from its parent.
- (void)privacyGuideViewControllerDidRemove:(UIViewController*)controller;

@end
#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_PRIVACY_GUIDE_PRIVACY_GUIDE_VIEW_CONTROLLER_PRESENTATION_DELEGATE_H_
