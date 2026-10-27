// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_PRIMARY_ACCOUNT_REAUTH_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_PRIMARY_ACCOUNT_REAUTH_COMMANDS_H_

#import <Foundation/Foundation.h>

#import "base/ios/block_types.h"

namespace signin_metrics {
enum class AccessPoint;
enum class ReauthAccessPoint;
}  // namespace signin_metrics

// Protocol used to display the primary account reauthentication UI.
@protocol PrimaryAccountReauthCommands

// For the following commands, `accessPoint` is used when
// `kMigrateToSigninReauthCoordinator` is disabled. `reauthAccessPoint` is used
// when `kMigrateToSigninReauthCoordinator` is enabled.

// Asks the presenter to display the UI to reauthenticate the primary account.
// The primary account should be available.
- (void)showPrimaryAccountReauthWithAccessPoint:
            (signin_metrics::AccessPoint)accessPoint
                              reauthAccessPoint:
                                  (signin_metrics::ReauthAccessPoint)
                                      reauthAccessPoint;

// Asks the presenter to display the UI to reauthenticate the primary account.
// The primary account should be available.
// `completion` is executed after the UI is dismissed, irrespective of whether
// it was interrupted, cancelled, or successful.
- (void)showPrimaryAccountReauthWithAccessPoint:
            (signin_metrics::AccessPoint)accessPoint
                              reauthAccessPoint:
                                  (signin_metrics::ReauthAccessPoint)
                                      reauthAccessPoint
                            dismissalCompletion:(ProceduralBlock)completion;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_PRIMARY_ACCOUNT_REAUTH_COMMANDS_H_
