// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_SHARING_STATUS_VIEW_CONTROLLER_PRESENTATION_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_SHARING_STATUS_VIEW_CONTROLLER_PRESENTATION_DELEGATE_H_

@class SharingStatusViewController;

// Delegate for SharingStatusViewController.
@protocol SharingStatusViewControllerPresentationDelegate <NSObject>

// Called when the user clicks done button.
- (void)sharingStatusWasDismissed:(SharingStatusViewController*)controller;

// Called when the sharing progress animation finishes (is not cancelled by the
// user). The actual sharing should kick off at this point in the main password
// sharing coordinator.
- (void)startPasswordSharing;

// Handles taps on the link to the site where the user can change the password
// that was shared.
- (void)changePasswordLinkWasTapped;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_SHARING_STATUS_VIEW_CONTROLLER_PRESENTATION_DELEGATE_H_
