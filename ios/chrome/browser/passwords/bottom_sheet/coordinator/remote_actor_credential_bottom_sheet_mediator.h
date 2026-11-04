// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_BOTTOM_SHEET_COORDINATOR_REMOTE_ACTOR_CREDENTIAL_BOTTOM_SHEET_MEDIATOR_H_
#define IOS_CHROME_BROWSER_PASSWORDS_BOTTOM_SHEET_COORDINATOR_REMOTE_ACTOR_CREDENTIAL_BOTTOM_SHEET_MEDIATOR_H_

#import <Foundation/Foundation.h>

class FaviconLoader;
@class RemoteActorCredentialBottomSheetMediator;

namespace password_manager {
class PasswordManagerClient;
class PasswordStoreInterface;
class RemoteActorCredentialSharingService;
}  // namespace password_manager

namespace remote_actor::proto {
class RemoteActorPasswordUploadRequest;
}  // namespace remote_actor::proto

namespace signin {
class IdentityManager;
}  // namespace signin

namespace syncer {
class SyncService;
}  // namespace syncer

// Delegate protocol for `RemoteActorCredentialBottomSheetMediator` to
// communicate the progress of the sharing flow back to the coordinator.
@protocol RemoteActorCredentialBottomSheetMediatorDelegate <NSObject>

// Called when credentials were retrieved and the bottom sheet should be
// presented.
- (void)mediatorDidRequestPresentation:
    (RemoteActorCredentialBottomSheetMediator*)mediator;

// Called when the sharing flow finished, with `success` set to `YES` if a
// credential was successfully shared.
- (void)mediator:(RemoteActorCredentialBottomSheetMediator*)mediator
    didFinishWithSuccess:(BOOL)success;

@end

// Mediator for the remote actor credential bottom sheet. Drives the
// `RemoteActorCredentialSharingController` flow which retrieves the
// credentials, lets the user pick one and shares the chosen credential. Device
// reauthentication is handled by the controller through
// `passwordManagerClient`.
@interface RemoteActorCredentialBottomSheetMediator : NSObject

// Delegate handling the flow progress.
@property(nonatomic, weak) id<RemoteActorCredentialBottomSheetMediatorDelegate>
    delegate;

// Initializes the mediator for the remote actor `request`. `identityManager`,
// `syncService`, `profilePasswordStore`, `accountPasswordStore`,
// `sharingService` and `passwordManagerClient` are passed to
// `RemoteActorCredentialSharingController` to retrieve the credentials,
// reauthenticate the user and share the chosen credential; `identityManager`,
// `syncService`, `sharingService` and `passwordManagerClient` may be null when
// unavailable, and the store matching the user's sync state must be non-null.
// `faviconLoader` is used to load the favicon of the requested domain.
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
                            request NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Starts the sharing flow by retrieving the credentials for the domain. The
// delegate is notified when the bottom sheet should be presented and when the
// flow finished.
- (void)start;

// Disconnects the mediator and cleans up dependencies. Must be called before
// deallocation.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_PASSWORDS_BOTTOM_SHEET_COORDINATOR_REMOTE_ACTOR_CREDENTIAL_BOTTOM_SHEET_MEDIATOR_H_
