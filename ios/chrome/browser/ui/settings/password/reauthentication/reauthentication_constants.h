// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_REAUTHENTICATION_REAUTHENTICATION_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_REAUTHENTICATION_REAUTHENTICATION_CONSTANTS_H_

#import <Foundation/Foundation.h>

namespace password_manager {
// Name of histogram tracking events in the password manager reauthentication
// UI.
extern const char kReauthenticationUIEventHistogram[];

// Accessibility for ReauthenticationViewController.
extern NSString* const kReauthenticationViewControllerAccessibilityIdentifier;

}  // namespace password_manager

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_REAUTHENTICATION_REAUTHENTICATION_CONSTANTS_H_
