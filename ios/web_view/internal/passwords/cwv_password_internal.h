// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_PASSWORDS_CWV_PASSWORD_INTERNAL_H_
#define IOS_WEB_VIEW_INTERNAL_PASSWORDS_CWV_PASSWORD_INTERNAL_H_

#include "components/password_manager/core/browser/password_form.h"
#import "ios/web_view/public/cwv_password.h"

@interface CWVPassword ()

- (instancetype)initWithPasswordForm:
    (const password_manager::PasswordForm&)passwordForm
    NS_DESIGNATED_INITIALIZER;

// The internal autofill credit card that is wrapped by this object.
// Intentionally not declared as a property to avoid issues when read by
// -[NSObject valueForKey:].
- (password_manager::PasswordForm*)internalPasswordForm;

@end

#endif  // IOS_WEB_VIEW_INTERNAL_PASSWORDS_CWV_PASSWORD_INTERNAL_H_
