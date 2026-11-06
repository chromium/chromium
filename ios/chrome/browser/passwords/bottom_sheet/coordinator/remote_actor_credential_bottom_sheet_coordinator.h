// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_BOTTOM_SHEET_COORDINATOR_REMOTE_ACTOR_CREDENTIAL_BOTTOM_SHEET_COORDINATOR_H_
#define IOS_CHROME_BROWSER_PASSWORDS_BOTTOM_SHEET_COORDINATOR_REMOTE_ACTOR_CREDENTIAL_BOTTOM_SHEET_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

namespace password_manager {
enum class RemoteActorCredentialSharingResult;
}  // namespace password_manager

namespace remote_actor::proto {
class RemoteActorPasswordUploadRequest;
}  // namespace remote_actor::proto

// Completion block called with the result of the remote actor credential
// sharing flow.
typedef void (^RemoteActorCredentialBottomSheetCompletion)(
    password_manager::RemoteActorCredentialSharingResult result);

// Coordinator that orchestrates retrieving credentials for a remote actor (e.g.
// Gemini) via `RemoteActorCredentialSharingController`, presenting the bottom
// sheet for credential selection, authenticating the user, and reporting the
// result.
@interface RemoteActorCredentialBottomSheetCoordinator : ChromeCoordinator

// Initializes the coordinator presenting on top of `viewController` within
// `browser`, to share a credential for the remote actor `request`.
// `completion` is called once with the result of the flow, after which the
// owner is responsible for stopping the coordinator.
- (instancetype)
    initWithBaseViewController:(UIViewController*)viewController
                       browser:(Browser*)browser
                       request:(remote_actor::proto::
                                    RemoteActorPasswordUploadRequest)request
                    completion:
                        (RemoteActorCredentialBottomSheetCompletion)completion
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_PASSWORDS_BOTTOM_SHEET_COORDINATOR_REMOTE_ACTOR_CREDENTIAL_BOTTOM_SHEET_COORDINATOR_H_
