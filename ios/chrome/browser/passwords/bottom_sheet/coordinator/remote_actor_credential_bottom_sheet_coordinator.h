// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_BOTTOM_SHEET_COORDINATOR_REMOTE_ACTOR_CREDENTIAL_BOTTOM_SHEET_COORDINATOR_H_
#define IOS_CHROME_BROWSER_PASSWORDS_BOTTOM_SHEET_COORDINATOR_REMOTE_ACTOR_CREDENTIAL_BOTTOM_SHEET_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

// Completion block called with `success` set to `YES` if a credential was
// successfully shared, or `NO` otherwise.
typedef void (^RemoteActorCredentialBottomSheetCompletion)(BOOL success);

// Coordinator that orchestrates retrieving credentials for a remote actor (e.g.
// Gemini) via `RemoteActorCredentialSharingController`, presenting the bottom
// sheet for credential selection, authenticating the user, and reporting the
// result.
@interface RemoteActorCredentialBottomSheetCoordinator : ChromeCoordinator

// Initializes the coordinator presenting on top of `viewController` within
// `browser`, to share a credential for `domain` on behalf of the user
// identified by `gaiaID` for the remote actor task identified by `taskID`.
// `completion` is called once with the result of the flow, after which the
// owner is responsible for stopping the coordinator. `completion` is called
// with `NO` if the coordinator is stopped before the flow finished.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                    domain:(NSString*)domain
                                    gaiaID:(NSString*)gaiaID
                                    taskID:(NSString*)taskID
                                completion:
                                    (RemoteActorCredentialBottomSheetCompletion)
                                        completion NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_PASSWORDS_BOTTOM_SHEET_COORDINATOR_REMOTE_ACTOR_CREDENTIAL_BOTTOM_SHEET_COORDINATOR_H_
