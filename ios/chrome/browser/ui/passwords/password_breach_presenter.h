// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PASSWORDS_PASSWORD_BREACH_PRESENTER_H_
#define IOS_CHROME_BROWSER_UI_PASSWORDS_PASSWORD_BREACH_PRESENTER_H_

#import <Foundation/Foundation.h>

// Object presenting the feature.
@protocol PasswordBreachPresenter <NSObject>

// Presents more information related to the feature.
- (void)presentLearnMore;

// Informs the presenter that the feature should dismiss.
- (void)stop;

// Informs the presenter that the Password page should be open.
- (void)openSavedPasswordsSettings;

@end

#endif  // IOS_CHROME_BROWSER_UI_PASSWORDS_PASSWORD_BREACH_PRESENTER_H_
