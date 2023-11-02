// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_UI_CONFIRMATION_ALERT_CONFIRMATION_ALERT_ACTION_HANDLER_H_
#define IOS_CHROME_COMMON_UI_CONFIRMATION_ALERT_CONFIRMATION_ALERT_ACTION_HANDLER_H_

#import <Foundation/Foundation.h>

@protocol ConfirmationAlertActionHandler <NSObject>

// The "Primary Action" was touched.
- (void)confirmationAlertPrimaryAction;

@optional

// The "Dismiss" button was touched.
- (void)confirmationAlertDismissAction;

// The "Secondary Action" was touched.
- (void)confirmationAlertSecondaryAction;

// The "Learn More" button was touched.
- (void)confirmationAlertLearnMoreAction;

// The "Tertiary Action" was touched.
- (void)confirmationAlertTertiaryAction;

@end

#endif  // IOS_CHROME_COMMON_UI_CONFIRMATION_ALERT_CONFIRMATION_ALERT_ACTION_HANDLER_H_
