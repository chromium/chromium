// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PASSWORDS_PASSWORD_BREACH_ACTION_HANDLER_H_
#define IOS_CHROME_BROWSER_UI_PASSWORDS_PASSWORD_BREACH_ACTION_HANDLER_H_

#import <Foundation/Foundation.h>

@protocol PasswordBreachActionHandler <NSObject>

// Password Breach is done and should be dismissed.
- (void)passwordBreachDone;

// The "Primary Action" should be excecuted.
- (void)passwordBreachPrimaryAction;

@end

#endif  // IOS_CHROME_BROWSER_UI_PASSWORDS_PASSWORD_BREACH_ACTION_HANDLER_H_
