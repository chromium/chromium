// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBAUTHN_MODEL_CREDENTIAL_EXPORTER_H_
#define IOS_CHROME_BROWSER_WEBAUTHN_MODEL_CREDENTIAL_EXPORTER_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

namespace password_manager {
class SavedPasswordsPresenter;
}  // namespace password_manager

namespace webauthn {
class PasskeyModel;
}  // namespace webauthn

// Handles exporting credentials using the Credential Exchange format
// (https://fidoalliance.org/specifications-credential-exchange-specifications).
@interface CredentialExporter : NSObject

// `window` is a presentantion anchor that will be used by the OS views.
// `savedPasswordsPresenter` must not be null and must outlive this object.
// `passkeyModel` must not be null and must outlive this object.
- (instancetype)initWithWindow:(UIWindow*)window
       savedPasswordsPresenter:
           (password_manager::SavedPasswordsPresenter*)savedPasswordsPresenter
         securityDomainSecrets:(NSArray<NSData*>*)securityDomainSecrets
                  passkeyModel:(webauthn::PasskeyModel*)passkeyModel
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Called when the user confirms the export flow.
- (void)startExport API_AVAILABLE(ios(26.0));

@end

#endif  // IOS_CHROME_BROWSER_WEBAUTHN_MODEL_CREDENTIAL_EXPORTER_H_
