// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_BOTTOM_SHEET_COORDINATOR_REMOTE_ACTOR_CREDENTIAL_BOTTOM_SHEET_MEDIATOR_H_
#define IOS_CHROME_BROWSER_PASSWORDS_BOTTOM_SHEET_COORDINATOR_REMOTE_ACTOR_CREDENTIAL_BOTTOM_SHEET_MEDIATOR_H_

#import <Foundation/Foundation.h>

@class RemoteActorCredentialBottomSheetMediator;

// Delegate protocol for `RemoteActorCredentialBottomSheetMediator` to
// communicate the progress of the sharing flow back to the coordinator.
@protocol RemoteActorCredentialBottomSheetMediatorDelegate <NSObject>

// Called when the sharing flow finished, with `success` set to `YES` if a
// credential was successfully shared.
- (void)mediator:(RemoteActorCredentialBottomSheetMediator*)mediator
    didFinishWithSuccess:(BOOL)success;

@end

// Mediator for the remote actor credential bottom sheet. Drives the
// `RemoteActorCredentialSharingController` flow which retrieves the
// credentials, lets the user pick one and shares the chosen credential.
@interface RemoteActorCredentialBottomSheetMediator : NSObject

// Delegate handling the flow progress.
@property(nonatomic, weak) id<RemoteActorCredentialBottomSheetMediatorDelegate>
    delegate;

// Initializes the mediator for the remote actor request identified by
// `domain`, `gaiaID` and `taskID`.
- (instancetype)initWithDomain:(NSString*)domain
                        gaiaID:(NSString*)gaiaID
                        taskID:(NSString*)taskID NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Starts the sharing flow by retrieving the credentials for the domain.
// Notifies the delegate that the flow finished unsuccessfully if there are no
// credentials to suggest.
- (void)start;

// Disconnects the mediator. Must be called before deallocation.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_PASSWORDS_BOTTOM_SHEET_COORDINATOR_REMOTE_ACTOR_CREDENTIAL_BOTTOM_SHEET_MEDIATOR_H_
