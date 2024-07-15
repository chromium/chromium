// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/ui/mock_credential_list_ui_handler.h"

@implementation MockCredentialListUIHandler

@synthesize allowedCredentials = _allowedCredentials;
@synthesize isRequestingPasskey = _isRequestingPasskey;

- (instancetype)initWithAllowedCredentials:(NSArray<NSData*>*)allowedCredentials
                       isRequestingPasskey:(BOOL)isRequestingPasskey {
  self = [super init];
  if (self) {
    _allowedCredentials = allowedCredentials;
    _isRequestingPasskey = isRequestingPasskey;
  }
  return self;
}

- (void)showEmptyCredentials {
}

- (void)userSelectedCredential:(id<Credential>)credential {
}

- (void)showDetailsForCredential:(id<Credential>)credential {
}

- (void)showCreateNewPasswordUI {
}

@end
