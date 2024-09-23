// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_MODEL_ARCHIVABLE_CREDENTIAL_PASSWORD_FORM_H_
#define IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_MODEL_ARCHIVABLE_CREDENTIAL_PASSWORD_FORM_H_

#import "ios/chrome/common/credential_provider/archivable_credential.h"

namespace password_manager {
struct PasswordForm;
}  // namespace password_manager

// Connivence method to create a PasswordForm from a Credential.
password_manager::PasswordForm PasswordFormFromCredential(
    id<Credential> credential);

// Category for adding convenience logic related to PasswordForms.
@interface ArchivableCredential (PasswordForm)

// Convenience initializer from a PasswordForm. Will return nil for forms
// blocked by the user, with an empty origin or Android forms.
- (instancetype)initWithPasswordForm:
                    (const password_manager::PasswordForm&)passwordForm
                             favicon:(NSString*)favicon
                                gaia:(NSString*)gaia;

@end

#endif  // IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_MODEL_ARCHIVABLE_CREDENTIAL_PASSWORD_FORM_H_
