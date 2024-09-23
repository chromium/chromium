// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_UI_BUNDLED_IDLE_IDLE_TIMEOUT_CONFIRMATION_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_POLICY_UI_BUNDLED_IDLE_IDLE_TIMEOUT_CONFIRMATION_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/policy/ui_bundled/idle/idle_timeout_confirmation_consumer.h"

#import "base/timer/timer.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_view_controller.h"

// View controller for the idle timeout confirmation dialog.
@interface IdleTimeoutConfirmationViewController
    : ConfirmationAlertViewController <IdleTimeoutConfirmationConsumer>

// Initializes controller with the string ids and idle threshold that will be
// shown in the dialog.
- (instancetype)initWithIdleTimeoutTitleId:(int)titleId
                     idleTimeoutSubtitleId:(int)subtitleId
                      idleTimeoutThreshold:(int)threshold
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithNibName:(NSString*)name
                         bundle:(NSBundle*)bundle NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_POLICY_UI_BUNDLED_IDLE_IDLE_TIMEOUT_CONFIRMATION_VIEW_CONTROLLER_H_
