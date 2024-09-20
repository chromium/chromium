// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_GOOGLE_SERVICES_SETTINGS_COMMAND_HANDLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_GOOGLE_SERVICES_SETTINGS_COMMAND_HANDLER_H_

#import "ios/chrome/browser/signin/model/constants.h"
#import "ios/chrome/browser/ui/authentication/authentication_ui_util.h"

// Protocol to communicate user actions from the mediator to its coordinator.
@protocol GoogleServicesSettingsCommandHandler <NSObject>

// Presents the sign-out dialog to the user.
// `targetRect` rect in table view system coordinate to display the signout
// popover dialog.
- (void)showSignOutFromTargetRect:(CGRect)targetRect
                       completion:
                           (signin_ui::SignoutCompletionCallback)completion;

// Presents the parcel tracking feature settings page.
- (void)showParcelTrackingSettingsPage;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_GOOGLE_SERVICES_SETTINGS_COMMAND_HANDLER_H_
