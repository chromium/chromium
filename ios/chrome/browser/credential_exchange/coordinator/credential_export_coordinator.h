// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CREDENTIAL_EXCHANGE_COORDINATOR_CREDENTIAL_EXPORT_COORDINATOR_H_
#define IOS_CHROME_BROWSER_CREDENTIAL_EXCHANGE_COORDINATOR_CREDENTIAL_EXPORT_COORDINATOR_H_

#import <vector>

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

namespace password_manager {
class AffiliatedGroup;
}  // namespace password_manager

@class CredentialExportCoordinator;

// Delegate for CredentialExportCoordinator.
@protocol CredentialExportCoordinatorDelegate

// Called when the export coordinator should be stopped.
- (void)credentialExportCoordinatorDidFinish:
    (CredentialExportCoordinator*)coordinator;

@end

API_AVAILABLE(ios(26.0))
// Coordinator for the credential exchange export flow.
@interface CredentialExportCoordinator : ChromeCoordinator

// Delegate for this coordinator.
@property(nonatomic, weak) id<CredentialExportCoordinatorDelegate> delegate;

// Passing `affiliatedGroups` in the constructor instead of accessing it from
// the `browser` is not the best practice, but it can only be accessed from
// `password_manager::SavedPasswordPresenter`, which is heavy to initialize.
// It is a common exception in password settings code.
- (instancetype)
    initWithBaseNavigationController:
        (UINavigationController*)navigationController
                             browser:(Browser*)browser
                    affiliatedGroups:
                        (std::vector<password_manager::AffiliatedGroup>)
                            affiliatedGroups NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_CREDENTIAL_EXCHANGE_COORDINATOR_CREDENTIAL_EXPORT_COORDINATOR_H_
