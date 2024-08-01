// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_UI_BUNDLED_USER_POLICY_USER_POLICY_PROMPT_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_POLICY_UI_BUNDLED_USER_POLICY_USER_POLICY_PROMPT_VIEW_CONTROLLER_H_

#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_view_controller.h"

// View controller for the User Policy prompt.
@interface UserPolicyPromptViewController : ConfirmationAlertViewController

// Initializes with the `managedDomain` representing the domain of the
// administrator that hosts the managed account. The `managedDomain` is
// interpolated in the notification's subtitle to inform the user what
// organization manages their account.
- (instancetype)initWithManagedDomain:(NSString*)managedDomain
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithNibName:(NSString*)name
                         bundle:(NSBundle*)bundle NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_POLICY_UI_BUNDLED_USER_POLICY_USER_POLICY_PROMPT_VIEW_CONTROLLER_H_
