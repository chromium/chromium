// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBAUTHN_COORDINATOR_CREDENTIAL_IMPORT_COORDINATOR_H_
#define IOS_CHROME_BROWSER_WEBAUTHN_COORDINATOR_CREDENTIAL_IMPORT_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

// Coordinator for the credential exchange import flow.
@interface CredentialImportCoordinator : ChromeCoordinator

// `UUID` is a token received from the OS during app launch, required to be
// passed back to the OS to receive the credential data.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                      UUID:(NSUUID*)UUID
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_WEBAUTHN_COORDINATOR_CREDENTIAL_IMPORT_COORDINATOR_H_
