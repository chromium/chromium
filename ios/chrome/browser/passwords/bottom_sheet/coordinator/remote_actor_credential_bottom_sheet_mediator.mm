// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/bottom_sheet/coordinator/remote_actor_credential_bottom_sheet_mediator.h"

#import <memory>
#import <utility>
#import <vector>

#import "base/check.h"
#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "components/password_manager/core/browser/password_form.h"
#import "components/password_manager/core/browser/remote_actor/remote_actor_credential_sharing_controller.h"
#import "components/password_manager/core/browser/remote_actor/remote_actor_credential_sharing_result.h"
#import "ios/chrome/browser/passwords/model/remote_actor/proto/remote_actor_password_upload_request.pb.h"
#import "ios/chrome/browser/passwords/model/remote_actor/remote_actor_credential_sharing_delegate_bridge.h"
#import "url/origin.h"

namespace {

using password_manager::RemoteActorCredentialSharingResult;

}  // namespace

@interface RemoteActorCredentialBottomSheetMediator () <
    RemoteActorCredentialSharingDelegate>
@end

@implementation RemoteActorCredentialBottomSheetMediator {
  // Dependencies of the sharing controller.
  raw_ptr<signin::IdentityManager> _identityManager;
  raw_ptr<syncer::SyncService> _syncService;
  raw_ptr<password_manager::PasswordStoreInterface> _profilePasswordStore;
  raw_ptr<password_manager::PasswordStoreInterface> _accountPasswordStore;
  raw_ptr<password_manager::RemoteActorCredentialSharingService>
      _sharingService;

  // Client used by the sharing controller for device reauthentication.
  raw_ptr<password_manager::PasswordManagerClient> _passwordManagerClient;

  // C++ bridge implementing the controller's `Delegate` interface.
  std::unique_ptr<RemoteActorCredentialSharingDelegateBridge> _delegateBridge;

  // Controller coordinating the sharing flow with password stores and backend.
  std::unique_ptr<password_manager::RemoteActorCredentialSharingController>
      _controller;

  // Stored password forms matching the requested domain.
  std::vector<std::unique_ptr<password_manager::PasswordForm>> _credentials;

  // `FaviconLoader` used to load the favicon.
  raw_ptr<FaviconLoader> _faviconLoader;

  // Remote actor request forwarded to the sharing controller on start.
  remote_actor::proto::RemoteActorPasswordUploadRequest _request;
}

- (instancetype)
    initWithIdentityManager:(signin::IdentityManager*)identityManager
                syncService:(syncer::SyncService*)syncService
       profilePasswordStore:
           (password_manager::PasswordStoreInterface*)profilePasswordStore
       accountPasswordStore:
           (password_manager::PasswordStoreInterface*)accountPasswordStore
             sharingService:
                 (password_manager::RemoteActorCredentialSharingService*)
                     sharingService
      passwordManagerClient:
          (password_manager::PasswordManagerClient*)passwordManagerClient
              faviconLoader:(FaviconLoader*)faviconLoader
                    request:
                        (remote_actor::proto::RemoteActorPasswordUploadRequest)
                            request {
  self = [super init];
  if (self) {
    _identityManager = identityManager;
    _syncService = syncService;
    _profilePasswordStore = profilePasswordStore;
    _accountPasswordStore = accountPasswordStore;
    _sharingService = sharingService;
    _passwordManagerClient = passwordManagerClient;
    _faviconLoader = faviconLoader;
    _request = std::move(request);
  }
  return self;
}

- (void)start {
  CHECK(!_controller);
  _delegateBridge =
      std::make_unique<RemoteActorCredentialSharingDelegateBridge>(self);
  _controller = std::make_unique<
      password_manager::RemoteActorCredentialSharingController>(
      _identityManager, _syncService, _profilePasswordStore,
      _accountPasswordStore, _sharingService, _passwordManagerClient,
      _delegateBridge.get());

  __weak __typeof(self) weakSelf = self;
  _controller->Start(
      _request.gaia_id(), _request.domain(), _request.task_id(),
      base::BindOnce(^(RemoteActorCredentialSharingResult result) {
        [weakSelf handleSharingResult:result];
      }));
}

- (void)disconnect {
  _controller.reset();
  _delegateBridge.reset();
  _identityManager = nullptr;
  _syncService = nullptr;
  _profilePasswordStore = nullptr;
  _accountPasswordStore = nullptr;
  _sharingService = nullptr;
  _passwordManagerClient = nullptr;
  _delegate = nil;
  _faviconLoader = nullptr;
  _credentials.clear();
  _request.Clear();
}

#pragma mark - RemoteActorCredentialSharingDelegate

- (void)
    showCredentialSelectionWithCredentials:
        (std::vector<std::unique_ptr<password_manager::PasswordForm>>)
            credentials
                                    origin:(const url::Origin&)origin
                                  callback:
                                      (RemoteActorCredentialSelectionCallback)
                                          callback {
  // TODO(crbug.com/579011876): Pass `credentials` to the consumer and resolve
  // `callback` from the mutator once the UI is added.
  _credentials = std::move(credentials);
  [self.delegate mediatorDidRequestPresentation:self];
}

#pragma mark - Private

// Handles the final sharing `result` from the controller.
- (void)handleSharingResult:(RemoteActorCredentialSharingResult)result {
  [self.delegate mediator:self
      didFinishWithSuccess:(result ==
                            RemoteActorCredentialSharingResult::kSuccess)];
}

@end
