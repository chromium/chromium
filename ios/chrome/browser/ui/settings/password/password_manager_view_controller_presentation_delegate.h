// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_MANAGER_VIEW_CONTROLLER_PRESENTATION_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_MANAGER_VIEW_CONTROLLER_PRESENTATION_DELEGATE_H_

#import <Foundation/Foundation.h>

// Presentation delegate for `PasswordManagerViewController`.
@protocol PasswordManagerViewControllerPresentationDelegate

// Called when `PasswordManagerViewController` is dismissed.
- (void)PasswordManagerViewControllerDismissed;

// Called when the user has requested that the Password Settings submenu be
// presented.
- (void)showPasswordSettingsSubmenu;

// Called when the user has tapped the "Show Me How" button of the Password
// Manager widget promo. This method presents the instruction view associated
// with that promo.
- (void)showPasswordManagerWidgetPromoInstructions;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_MANAGER_VIEW_CONTROLLER_PRESENTATION_DELEGATE_H_
