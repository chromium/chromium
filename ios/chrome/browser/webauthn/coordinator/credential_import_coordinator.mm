// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webauthn/coordinator/credential_import_coordinator.h"

#import "ios/chrome/browser/webauthn/coordinator/credential_import_mediator.h"

@implementation CredentialImportCoordinator {
  // Handles interaction with the model.
  CredentialImportMediator* _mediator;

  // Token received from the OS during app launch needed to receive credentials.
  NSUUID* _UUID;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                      UUID:(NSUUID*)UUID {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _UUID = UUID;
  }
  return self;
}

- (void)start {
  _mediator = [[CredentialImportMediator alloc] initWithUUID:_UUID];
}

@end
