// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORDS_IN_OTHER_APPS_PASSWORDS_IN_OTHER_APPS_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORDS_IN_OTHER_APPS_PASSWORDS_IN_OTHER_APPS_VIEW_CONTROLLER_DELEGATE_H_

#import <UIKit/UIKit.h>

// Base delegate protocol for the base passwords in other apps view controller
// to communicate with screen-specific coordinators.
@protocol PasswordsInOtherAppsViewControllerDelegate

// Invoked when the action button is tapped.
- (void)openApplicationSettings;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORDS_IN_OTHER_APPS_PASSWORDS_IN_OTHER_APPS_VIEW_CONTROLLER_DELEGATE_H_
