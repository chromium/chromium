// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_MEDIATOR_TESTING_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_MEDIATOR_TESTING_H_

#import "ios/chrome/browser/ui/settings/password/password_details/credential_details.h"

// Testing category to expose a private property used for tests.
@interface PasswordDetailsMediator (Testing)

// The context in which the password details are accessed.
@property(nonatomic, assign) DetailsContext context;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_MEDIATOR_TESTING_H_
