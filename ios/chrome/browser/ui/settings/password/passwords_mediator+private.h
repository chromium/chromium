// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORDS_MEDIATOR_PRIVATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORDS_MEDIATOR_PRIVATE_H_

#import "ios/chrome/browser/ui/settings/password/passwords_mediator.h"

// Class extension exposing a private property of PasswordsMediator for
// testing.
@interface PasswordsMediator ()

// Whether or not the Feature Engagement Tracker should be notified that the
// Password Manager widget promo is not displayed anymore. Will be `true` when
// the Password Manager view controller is dismissed while presenting the
// promo.
@property(nonatomic, assign)
    BOOL shouldNotifyFETToDismissPasswordManagerWidgetPromo;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORDS_MEDIATOR_PRIVATE_H_
