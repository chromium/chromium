// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_CREDENTIAL_PASSWORDFORM_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_CREDENTIAL_PASSWORDFORM_H_

#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_credential.h"

namespace password_manager {
struct PasswordForm;
}  // namespace password_manager

@interface ManualFillCredential (PasswordForm)

// Convenience initializer from a PasswordForm.
- (instancetype)initWithPasswordForm:
    (const password_manager::PasswordForm&)passwordForm;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_CREDENTIAL_PASSWORDFORM_H_
