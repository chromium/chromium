// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_UI_BUNDLED_IDLE_IDLE_TIMEOUT_CONFIRMATION_MEDIATOR_H_
#define IOS_CHROME_BROWSER_POLICY_UI_BUNDLED_IDLE_IDLE_TIMEOUT_CONFIRMATION_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/policy/ui_bundled/idle/idle_timeout_confirmation_consumer.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"

@protocol IdleTimeoutConfirmationPresenter;

@interface IdleTimeoutConfirmationMediator
    : NSObject <ConfirmationAlertActionHandler>

// Consumer of this mediator.
@property(nonatomic, weak) id<IdleTimeoutConfirmationConsumer> consumer;

- (instancetype)initWithPresenter:
                    (id<IdleTimeoutConfirmationPresenter>)presenter
                   dialogDuration:(base::TimeDelta)duration
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

- (void)stop;

@end

#endif  // IOS_CHROME_BROWSER_POLICY_UI_BUNDLED_IDLE_IDLE_TIMEOUT_CONFIRMATION_MEDIATOR_H_
