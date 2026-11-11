// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/bottom_sheet/coordinator/remote_actor_credential_bottom_sheet_coordinator.h"

#import "base/check.h"
#import "ios/chrome/browser/passwords/bottom_sheet/coordinator/remote_actor_credential_bottom_sheet_mediator.h"

@interface RemoteActorCredentialBottomSheetCoordinator () <
    RemoteActorCredentialBottomSheetMediatorDelegate>
@end

@implementation RemoteActorCredentialBottomSheetCoordinator {
  // Domain for which credentials were requested.
  NSString* _domain;

  // GAIA ID of the requesting user.
  NSString* _gaiaID;

  // Task ID associated with the remote actor request.
  NSString* _taskID;

  // Completion block called with the result of the flow.
  RemoteActorCredentialBottomSheetCompletion _completion;

  // Mediator for the remote actor credential bottom sheet.
  RemoteActorCredentialBottomSheetMediator* _mediator;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                    domain:(NSString*)domain
                                    gaiaID:(NSString*)gaiaID
                                    taskID:(NSString*)taskID
                                completion:
                                    (RemoteActorCredentialBottomSheetCompletion)
                                        completion {
  CHECK(completion);
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _domain = [domain copy];
    _gaiaID = [gaiaID copy];
    _taskID = [taskID copy];
    _completion = [completion copy];
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  _mediator =
      [[RemoteActorCredentialBottomSheetMediator alloc] initWithDomain:_domain
                                                                gaiaID:_gaiaID
                                                                taskID:_taskID];
  _mediator.delegate = self;
  [_mediator start];
}

- (void)stop {
  [_mediator disconnect];
  _mediator = nil;

  // Ensure the caller is always notified, even if the flow is torn down
  // before it finished.
  [self runCompletionWithSuccess:NO];

  [super stop];
}

#pragma mark - RemoteActorCredentialBottomSheetMediatorDelegate

- (void)mediator:(RemoteActorCredentialBottomSheetMediator*)mediator
    didFinishWithSuccess:(BOOL)success {
  // The owner is responsible for stopping the coordinator once notified.
  [self runCompletionWithSuccess:success];
}

#pragma mark - Private

// Runs `_completion` with `success` if it hasn't already been run.
- (void)runCompletionWithSuccess:(BOOL)success {
  if (!_completion) {
    return;
  }
  RemoteActorCredentialBottomSheetCompletion completion = _completion;
  _completion = nil;
  completion(success);
}

@end
