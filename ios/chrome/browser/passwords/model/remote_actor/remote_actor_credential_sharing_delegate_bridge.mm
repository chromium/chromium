// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/model/remote_actor/remote_actor_credential_sharing_delegate_bridge.h"

#import <utility>

#import "base/functional/callback.h"
#import "base/types/expected.h"
#import "components/password_manager/core/browser/password_form.h"
#import "components/password_manager/core/browser/remote_actor/remote_actor_credential_sharing_result.h"
#import "url/origin.h"

RemoteActorCredentialSharingDelegateBridge::
    RemoteActorCredentialSharingDelegateBridge(
        id<RemoteActorCredentialSharingDelegate> delegate)
    : delegate_(delegate) {}

RemoteActorCredentialSharingDelegateBridge::
    ~RemoteActorCredentialSharingDelegateBridge() = default;

void RemoteActorCredentialSharingDelegateBridge::ShowCredentialSelection(
    std::vector<std::unique_ptr<password_manager::PasswordForm>> credentials,
    const url::Origin& origin,
    base::OnceCallback<void(CredentialOrError)> callback) {
  if (!delegate_) {
    std::move(callback).Run(base::unexpected(
        password_manager::RemoteActorCredentialSharingResult::kOtherError));
    return;
  }
  [delegate_ showCredentialSelectionWithCredentials:std::move(credentials)
                                             origin:origin
                                           callback:std::move(callback)];
}
