// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FIRST_RUN_WELCOME_WELCOME_SCREEN_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_FIRST_RUN_WELCOME_WELCOME_SCREEN_MEDIATOR_H_

#import <Foundation/Foundation.h>

@protocol WelcomeScreenConsumer;

// Mediator that handles writing to prefs for the welcome screen.
@interface WelcomeScreenMediator : NSObject

// Contains the user choice for UMA reporting. This value is set to the default
// value when the coordinator is initialized.
@property(nonatomic, assign) BOOL UMAReportingUserChoice;
// Mediator's consumer.
@property(nonatomic, weak) id<WelcomeScreenConsumer> consumer;

- (instancetype)init NS_DESIGNATED_INITIALIZER;

// Returns whether the metrics reporting consent checkbox should be selected or
// not by default.
- (BOOL)isCheckboxSelectedByDefault;

// Persists the opt-in state for metrics reporting to the prefs.
- (void)setMetricsReportingEnabled:(BOOL)enabled;

// Accepts Terms of Services. Persists the acceptance of terms and conditions
// and to the prefs.
- (void)acceptToS;

@end

#endif  // IOS_CHROME_BROWSER_UI_FIRST_RUN_WELCOME_WELCOME_SCREEN_MEDIATOR_H_
