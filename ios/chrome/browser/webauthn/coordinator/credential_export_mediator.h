// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBAUTHN_COORDINATOR_CREDENTIAL_EXPORT_MEDIATOR_H_
#define IOS_CHROME_BROWSER_WEBAUTHN_COORDINATOR_CREDENTIAL_EXPORT_MEDIATOR_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

namespace password_manager {
class SavedPasswordsPresenter;
}  // namespace password_manager

namespace webauthn {
class PasskeyModel;
}  // namespace webauthn

// Mediator for the credential exchange export flow.
@interface CredentialExportMediator : NSObject

- (instancetype)initWithWindow:(UIWindow*)window
       savedPasswordsPresenter:
           (password_manager::SavedPasswordsPresenter*)savedPasswordsPresenter
                  passkeyModel:(webauthn::PasskeyModel*)passkeyModel
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Called when the user confirms the export flow.
- (void)startExportWithSecurityDomainSecrets:
    (NSArray<NSData*>*)securityDomainSecrets API_AVAILABLE(ios(26.0));

@end

#endif  // IOS_CHROME_BROWSER_WEBAUTHN_COORDINATOR_CREDENTIAL_EXPORT_MEDIATOR_H_
