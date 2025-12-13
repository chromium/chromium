// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PASSWORDS_PASSWORD_BREACH_PRESENTER_H_
#define IOS_CHROME_BROWSER_UI_PASSWORDS_PASSWORD_BREACH_PRESENTER_H_

#import <Foundation/Foundation.h>

// Object presenting the feature.
@protocol PasswordBreachPresenter <NSObject>

// Informs the presenter that the feature should dismiss.
- (void)stop;

// Informs the presenter that the Password Checkup homepage should be opened.
- (void)openPasswordCheckup;

// Informs the presenter that the Password Manager page should be opened.
- (void)openPasswordManager;

@end

#endif  // IOS_CHROME_BROWSER_UI_PASSWORDS_PASSWORD_BREACH_PRESENTER_H_
