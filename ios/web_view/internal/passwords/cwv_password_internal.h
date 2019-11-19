// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_PASSWORDS_CWV_PASSWORD_INTERNAL_H_
#define IOS_WEB_VIEW_INTERNAL_PASSWORDS_CWV_PASSWORD_INTERNAL_H_

#include "components/autofill/core/common/password_form.h"
#import "ios/web_view/public/cwv_password.h"

@interface CWVPassword ()

- (instancetype)initWithPasswordForm:(const autofill::PasswordForm&)passwordForm
    NS_DESIGNATED_INITIALIZER;

// The internal autofill credit card that is wrapped by this object.
// Intentionally not declared as a property to avoid issues when read by
// -[NSObject valueForKey:].
- (autofill::PasswordForm*)internalPasswordForm;

@end

#endif  // IOS_WEB_VIEW_INTERNAL_PASSWORDS_CWV_PASSWORD_INTERNAL_H_
