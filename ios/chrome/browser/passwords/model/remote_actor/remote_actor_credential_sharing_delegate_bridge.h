// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_MODEL_REMOTE_ACTOR_REMOTE_ACTOR_CREDENTIAL_SHARING_DELEGATE_BRIDGE_H_
#define IOS_CHROME_BROWSER_PASSWORDS_MODEL_REMOTE_ACTOR_REMOTE_ACTOR_CREDENTIAL_SHARING_DELEGATE_BRIDGE_H_

#import <Foundation/Foundation.h>

#import <memory>
#import <vector>

#import "base/functional/callback_forward.h"
#import "components/password_manager/core/browser/remote_actor/remote_actor_credential_sharing_controller.h"

namespace password_manager {
struct PasswordForm;
}  // namespace password_manager

namespace url {
class Origin;
}  // namespace url

// Callback run with the selected credential or the failure reason.
using RemoteActorCredentialSelectionCallback = base::OnceCallback<void(
    password_manager::RemoteActorCredentialSharingController::Delegate::
        CredentialOrError)>;

// Objective-C protocol mirroring
// `password_manager::RemoteActorCredentialSharingController::Delegate`.
@protocol RemoteActorCredentialSharingDelegate <NSObject>

// Presents the credential selection UI for `origin` with `credentials`.
// `callback` must be run exactly once, including when the UI is dismissed or
// torn down, otherwise the sharing request never completes.
- (void)
    showCredentialSelectionWithCredentials:
        (std::vector<std::unique_ptr<password_manager::PasswordForm>>)
            credentials
                                    origin:(const url::Origin&)origin
                                  callback:
                                      (RemoteActorCredentialSelectionCallback)
                                          callback;

@end

// Bridge forwarding C++ `RemoteActorCredentialSharingController::Delegate`
// calls to an Objective-C `RemoteActorCredentialSharingDelegate`.
class RemoteActorCredentialSharingDelegateBridge final
    : public password_manager::RemoteActorCredentialSharingController::
          Delegate {
 public:
  explicit RemoteActorCredentialSharingDelegateBridge(
      id<RemoteActorCredentialSharingDelegate> delegate);
  RemoteActorCredentialSharingDelegateBridge(
      const RemoteActorCredentialSharingDelegateBridge&) = delete;
  RemoteActorCredentialSharingDelegateBridge& operator=(
      const RemoteActorCredentialSharingDelegateBridge&) = delete;
  ~RemoteActorCredentialSharingDelegateBridge() override;

  // `password_manager::RemoteActorCredentialSharingController::Delegate`:
  void ShowCredentialSelection(
      std::vector<std::unique_ptr<password_manager::PasswordForm>> credentials,
      const url::Origin& origin,
      base::OnceCallback<void(CredentialOrError)> callback) override;

 private:
  __weak id<RemoteActorCredentialSharingDelegate> delegate_ = nil;
};

#endif  // IOS_CHROME_BROWSER_PASSWORDS_MODEL_REMOTE_ACTOR_REMOTE_ACTOR_CREDENTIAL_SHARING_DELEGATE_BRIDGE_H_
