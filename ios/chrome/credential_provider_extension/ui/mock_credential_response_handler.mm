// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/ui/mock_credential_response_handler.h"

@implementation MockCredentialResponseHandler

- (void)userSelectedPassword:(ASPasswordCredential*)credential {
  self.passwordCredential = credential;
  if (self.receivedCredentialBlock) {
    self.receivedCredentialBlock();
  }
}

- (void)userSelectedPasskey:(ASPasskeyAssertionCredential*)credential
    API_AVAILABLE(ios(17.0)) {
  self.passkeyCredential = credential;
  if (self.receivedPasskeyBlock) {
    self.receivedPasskeyBlock();
  }
}

- (void)userCancelledRequestWithErrorCode:(ASExtensionErrorCode)errorCode {
  self.errorCode = errorCode;
  if (self.receivedErrorCodeBlock) {
    self.receivedErrorCodeBlock();
  }
}

- (void)completeExtensionConfigurationRequest {
  // No-op.
}

@end
