// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/bottom_sheet/coordinator/remote_actor_credential_bottom_sheet_mediator.h"

@implementation RemoteActorCredentialBottomSheetMediator {
  // Domain for which credentials were requested.
  NSString* _domain;

  // GAIA ID of the requesting user.
  NSString* _gaiaID;

  // Task ID associated with the remote actor request.
  NSString* _taskID;
}

- (instancetype)initWithDomain:(NSString*)domain
                        gaiaID:(NSString*)gaiaID
                        taskID:(NSString*)taskID {
  self = [super init];
  if (self) {
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
  [self.delegate mediator:self didFinishWithSuccess:NO];
}

- (void)disconnect {
  _delegate = nil;
}

@end
