// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MANUAL_FILL_MODEL_MANUAL_FILL_CREDENTIAL_PASSWORDFORM_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MANUAL_FILL_MODEL_MANUAL_FILL_CREDENTIAL_PASSWORDFORM_H_

#import "ios/chrome/browser/autofill/manual_fill/model/manual_fill_credential.h"

namespace password_manager {
struct PasswordForm;
}  // namespace password_manager

@interface ManualFillCredential (PasswordForm)

// Convenience initializer from a PasswordForm. `isBackup` indicates whether the
// given `passwordForm` represents a regular or backup password.
- (instancetype)initWithPasswordForm:
                    (const password_manager::PasswordForm&)passwordForm
                            isBackup:(BOOL)isBackup;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_MANUAL_FILL_MODEL_MANUAL_FILL_CREDENTIAL_PASSWORDFORM_H_
