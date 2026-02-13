// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CREDENTIAL_EXCHANGE_COORDINATOR_CREDENTIAL_IMPORT_COORDINATOR_H_
#define IOS_CHROME_BROWSER_CREDENTIAL_EXCHANGE_COORDINATOR_CREDENTIAL_IMPORT_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@class CredentialImportCoordinator;
@protocol ReauthenticationProtocol;

// Delegate for CredentialImportCoordinator.
API_AVAILABLE(ios(26.0))
@protocol CredentialImportCoordinatorDelegate

// Called when the import coordinator should be stopped.
- (void)credentialImportCoordinatorDidFinish:
    (CredentialImportCoordinator*)coordinator;

@end

// Coordinator for the credential exchange import flow.
API_AVAILABLE(ios(26.0))
@interface CredentialImportCoordinator : ChromeCoordinator

// Delegate for this coordinator.
@property(nonatomic, weak) id<CredentialImportCoordinatorDelegate> delegate;

// `UUID` is a token received from the OS during app launch, required to be
// passed back to the OS to receive the credential data.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                      UUID:(NSUUID*)UUID
                              reauthModule:
                                  (id<ReauthenticationProtocol>)reauthModule
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_CREDENTIAL_EXCHANGE_COORDINATOR_CREDENTIAL_IMPORT_COORDINATOR_H_
