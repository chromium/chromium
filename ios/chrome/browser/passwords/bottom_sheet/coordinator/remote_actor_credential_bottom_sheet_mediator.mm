// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/bottom_sheet/coordinator/remote_actor_credential_bottom_sheet_mediator.h"

#import <memory>
#import <vector>

#import "base/memory/raw_ptr.h"
#import "components/password_manager/core/browser/password_form.h"

@implementation RemoteActorCredentialBottomSheetMediator {
  // Dependencies of the sharing controller.
  raw_ptr<signin::IdentityManager> _identityManager;
  raw_ptr<syncer::SyncService> _syncService;
  raw_ptr<password_manager::PasswordStoreInterface> _profilePasswordStore;
  raw_ptr<password_manager::PasswordStoreInterface> _accountPasswordStore;

  // Client used by the sharing controller for device reauthentication.
  raw_ptr<password_manager::PasswordManagerClient> _passwordManagerClient;

  // Stored password forms matching `_domain`.
  std::vector<std::unique_ptr<password_manager::PasswordForm>> _credentials;

  // `FaviconLoader` used to load the favicon.
  raw_ptr<FaviconLoader> _faviconLoader;

  // Domain for which credentials were requested.
  NSString* _domain;

  // GAIA ID of the requesting user.
  NSString* _gaiaID;

  // Task ID associated with the remote actor request.
  NSString* _taskID;
}

- (instancetype)
    initWithIdentityManager:(signin::IdentityManager*)identityManager
                syncService:(syncer::SyncService*)syncService
       profilePasswordStore:
           (password_manager::PasswordStoreInterface*)profilePasswordStore
       accountPasswordStore:
           (password_manager::PasswordStoreInterface*)accountPasswordStore
      passwordManagerClient:
          (password_manager::PasswordManagerClient*)passwordManagerClient
              faviconLoader:(FaviconLoader*)faviconLoader
                     domain:(NSString*)domain
                     gaiaID:(NSString*)gaiaID
                     taskID:(NSString*)taskID {
  self = [super init];
  if (self) {
    _identityManager = identityManager;
    _syncService = syncService;
    _profilePasswordStore = profilePasswordStore;
    _accountPasswordStore = accountPasswordStore;
    _passwordManagerClient = passwordManagerClient;
    _faviconLoader = faviconLoader;
    _domain = [domain copy];
    _gaiaID = [gaiaID copy];
    _taskID = [taskID copy];
  }
  return self;
}

- (void)start {
  // TODO(crbug.com/579010739): Retrieve credentials via
  // `RemoteActorCredentialSharingController` and
  // `RemoteActorCredentialSharingDelegateBridge`.
  if (_credentials.empty()) {
    [self.delegate mediator:self didFinishWithSuccess:NO];
  }
}

- (void)disconnect {
  _identityManager = nullptr;
  _syncService = nullptr;
  _profilePasswordStore = nullptr;
  _accountPasswordStore = nullptr;
  _passwordManagerClient = nullptr;
  _delegate = nil;
  _faviconLoader = nullptr;
  _credentials.clear();
}

@end
