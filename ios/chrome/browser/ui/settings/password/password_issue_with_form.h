// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_ISSUE_WITH_FORM_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_ISSUE_WITH_FORM_H_

#include "components/password_manager/core/browser/password_form_forward.h"
#import "ios/chrome/browser/ui/settings/password/password_issue.h"

// Class based on PasswordIssue which adds PasswordForm as a property.
@interface PasswordIssueWithForm : NSObject <PasswordIssue>

// Password form is used to display Password Details screen.
@property(nonatomic, readonly) password_manager::PasswordForm form;

- (instancetype)initWithPasswordForm:(password_manager::PasswordForm)form
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_ISSUE_WITH_FORM_H_
