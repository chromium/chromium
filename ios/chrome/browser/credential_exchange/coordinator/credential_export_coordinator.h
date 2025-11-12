// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CREDENTIAL_EXCHANGE_COORDINATOR_CREDENTIAL_EXPORT_COORDINATOR_H_
#define IOS_CHROME_BROWSER_CREDENTIAL_EXCHANGE_COORDINATOR_CREDENTIAL_EXPORT_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

namespace password_manager {
class SavedPasswordsPresenter;
}  // namespace password_manager

namespace webauthn {
class PasskeyModel;
}  // namespace webauthn

API_AVAILABLE(ios(26.0))
// Coordinator for the credential exchange export flow.
@interface CredentialExportCoordinator : ChromeCoordinator

// Passing `savedPasswordsPresenter` in the constructor instead of accessing it
// from the `browser` is not the best practice, but the initialization of this
// object is quite heavy and it is a common exception in password settings code.
// TODO(crbug.com/444112223): In case `savedPasswordsPresenter` will end up
// being used only one time to access credentials, just pass the credentials
// instead.
- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
                         savedPasswordsPresenter:
                             (password_manager::SavedPasswordsPresenter*)
                                 savedPasswordsPresenter
                                    passkeyModel:
                                        (webauthn::PasskeyModel*)passkeyModel
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_CREDENTIAL_EXCHANGE_COORDINATOR_CREDENTIAL_EXPORT_COORDINATOR_H_
