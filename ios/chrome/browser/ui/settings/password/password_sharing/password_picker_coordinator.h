// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_PASSWORD_PICKER_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_PASSWORD_PICKER_COORDINATOR_H_

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

namespace password_manager {
struct CredentialUIEntry;
}  // namespace password_manager

@protocol PasswordPickerCoordinatorDelegate;

// This coordinator presents the list of credential groups for a user that
// initiated password sharing from a password details view that contains more
// than 1 credential group and allows choosing groups that should be shared.
@interface PasswordPickerCoordinator : ChromeCoordinator

- (instancetype)
    initWithBaseViewController:(UIViewController*)viewController
                       browser:(Browser*)browser
                   credentials:
                       (const std::vector<password_manager::CredentialUIEntry>&)
                           credentials NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Delegate handling coordinator dismissal.
@property(nonatomic, weak) id<PasswordPickerCoordinatorDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_PASSWORD_PICKER_COORDINATOR_H_
