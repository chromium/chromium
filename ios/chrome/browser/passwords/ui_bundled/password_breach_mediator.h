// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PASSWORDS_PASSWORD_BREACH_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_PASSWORDS_PASSWORD_BREACH_MEDIATOR_H_

#import <Foundation/Foundation.h>

#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"

@protocol ApplicationCommands;
@protocol PasswordBreachConsumer;
@protocol PasswordBreachPresenter;

// Manages the state and interactions of the consumer.
@interface PasswordBreachMediator : NSObject <ConfirmationAlertActionHandler>

- (instancetype)
    initWithConsumer:(id<PasswordBreachConsumer>)consumer
           presenter:(id<PasswordBreachPresenter>)presenter
    metrics_recorder:
        (std::unique_ptr<
            password_manager::metrics_util::LeakDialogMetricsRecorder>)
            metrics_recorder
            leakType:(password_manager::CredentialLeakType)leakType;

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithCoder NS_UNAVAILABLE;

// Informs the mediator that its about to be destroyed, so it can perform any
// logging or clean up needed.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_UI_PASSWORDS_PASSWORD_BREACH_MEDIATOR_H_
