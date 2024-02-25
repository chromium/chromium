// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_PASSWORD_SHARING_VIEW_CONTROLLER_PRESENTATION_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_PASSWORD_SHARING_VIEW_CONTROLLER_PRESENTATION_DELEGATE_H_

@class PasswordSharingViewController;

// Delegate for PasswordSharingViewController.
@protocol PasswordSharingViewControllerPresentationDelegate <NSObject>

// Called when the user clicks the cancel button.
- (void)sharingSpinnerViewWasDismissed:
    (PasswordSharingViewController*)controller;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_PASSWORD_SHARING_VIEW_CONTROLLER_PRESENTATION_DELEGATE_H_
