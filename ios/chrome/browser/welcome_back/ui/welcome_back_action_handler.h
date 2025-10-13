// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WELCOME_BACK_UI_WELCOME_BACK_ACTION_HANDLER_H_
#define IOS_CHROME_BROWSER_WELCOME_BACK_UI_WELCOME_BACK_ACTION_HANDLER_H_

#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"

@class BestFeaturesItem;

// Protocol to communicate user actions on the Welcome Back screen.
@protocol WelcomeBackActionHandler <ConfirmationAlertActionHandler>

// Called when a user taps on a Welcome Back item.
- (void)didTapBestFeatureItem:(BestFeaturesItem*)item;

@end

#endif  // IOS_CHROME_BROWSER_WELCOME_BACK_UI_WELCOME_BACK_ACTION_HANDLER_H_
