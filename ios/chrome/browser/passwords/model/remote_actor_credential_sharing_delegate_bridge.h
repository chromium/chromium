// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_MODEL_REMOTE_ACTOR_CREDENTIAL_SHARING_DELEGATE_BRIDGE_H_
#define IOS_CHROME_BROWSER_PASSWORDS_MODEL_REMOTE_ACTOR_CREDENTIAL_SHARING_DELEGATE_BRIDGE_H_

#import <Foundation/Foundation.h>

#import <memory>
#import <vector>

#import "base/functional/callback.h"
#import "components/password_manager/core/browser/remote_actor/remote_actor_credential_sharing_controller.h"

namespace url {
class Origin;
}  // namespace url

// Objective-C protocol mirroring
// `password_manager::RemoteActorCredentialSharingController::Delegate`.
@protocol RemoteActorCredentialSharingDelegate <NSObject>

// Presents the credential selection UI for `origin` with `credentials`.
- (void)showCredentialSelection:
            (std::vector<std::unique_ptr<password_manager::PasswordForm>>)
                credentials
                         origin:(const url::Origin&)origin
                       callback:
                           (base::OnceCallback<void(
                                password_manager::
                                    RemoteActorCredentialSharingController::
                                        Delegate::CredentialOrError)>)callback;

// Prompts the user for device reauthentication if required before sharing.
- (void)
    authenticateIfNeededForOrigin:(const url::Origin&)origin
                         callback:
                             (base::OnceCallback<void(
                                  password_manager::
                                      RemoteActorCredentialSharingController::
                                          Delegate::AuthResult)>)callback;

@end

// Bridge forwarding C++ `RemoteActorCredentialSharingController::Delegate`
// calls to an Objective-C `RemoteActorCredentialSharingDelegate`.
class RemoteActorCredentialSharingDelegateBridge
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
  void AuthenticateIfNeeded(
      const url::Origin& origin,
      base::OnceCallback<void(AuthResult)> callback) override;

 private:
  __weak id<RemoteActorCredentialSharingDelegate> delegate_ = nil;
};

#endif  // IOS_CHROME_BROWSER_PASSWORDS_MODEL_REMOTE_ACTOR_CREDENTIAL_SHARING_DELEGATE_BRIDGE_H_
