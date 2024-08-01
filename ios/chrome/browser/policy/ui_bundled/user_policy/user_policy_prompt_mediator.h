// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_UI_BUNDLED_USER_POLICY_USER_POLICY_PROMPT_MEDIATOR_H_
#define IOS_CHROME_BROWSER_POLICY_UI_BUNDLED_USER_POLICY_USER_POLICY_PROMPT_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"

@protocol UserPolicyPromptPresenter;
class AuthenticationService;

@interface UserPolicyPromptMediator
    : NSObject <ConfirmationAlertActionHandler,
                UIAdaptivePresentationControllerDelegate>

- (instancetype)initWithPresenter:(id<UserPolicyPromptPresenter>)presenter
                      authService:(AuthenticationService*)authService
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_POLICY_UI_BUNDLED_USER_POLICY_USER_POLICY_PROMPT_MEDIATOR_H_
