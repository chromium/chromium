// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PASSWORDS_PASSWORD_BREACH_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_PASSWORDS_PASSWORD_BREACH_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/passwords/password_breach_consumer.h"

@protocol PasswordBreachActionHandler;

@interface PasswordBreachViewController
    : UIViewController <PasswordBreachConsumer>

// The action handler for interactions in this View Controller.
@property(nonatomic, weak) id<PasswordBreachActionHandler> actionHandler;

@end

#endif  // IOS_CHROME_BROWSER_UI_PASSWORDS_PASSWORD_BREACH_VIEW_CONTROLLER_H_
