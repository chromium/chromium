// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/ui/mock_credential_response_handler.h"

#import "base/strings/string_number_conversions.h"
#import "ios/chrome/credential_provider_extension/passkey_request_details.h"

namespace {

std::vector<std::vector<uint8_t>> TrustedVaultKeys() {
  const std::vector<char> key_values = {
      '\x1f', '\xfa', '\x97', '\x98', '\xdf', '\n',   '\xc7', '\xe4',
      '\xf6', 'G',    '\xd5', 'm',    'C',    '\xa2', 'P',    '\xe0',
      '\xa2', 'E',    '\x90', '\xb2', '\x86', '\xbf', '\xfc', 'E',
      '\e',   'N',    '\x15', '\xea', 'G',    '\x9b', '\x9b', '\xc8'};
  return {std::vector<uint8_t>(key_values.begin(), key_values.end())};
}

}  // namespace

@implementation MockCredentialResponseHandler

- (void)userSelectedPassword:(ASPasswordCredential*)credential {
  self.passwordCredential = credential;
  if (self.receivedCredentialBlock) {
    self.receivedCredentialBlock();
  }
}

- (void)userSelectedPasskey:(ASPasskeyAssertionCredential*)credential {
  self.passkeyCredential = credential;
  if (self.receivedPasskeyBlock) {
    self.receivedPasskeyBlock();
  }
}

- (void)userSelectedPasskey:(id<Credential>)passkey
      passkeyRequestDetails:(PasskeyRequestDetails*)passkeyRequestDetails {
  [self userSelectedPasskey:[passkeyRequestDetails
                                    assertPasskeyCredential:passkey
                                           trustedVaultKeys:TrustedVaultKeys()
                                didCompleteUserVerification:NO]];
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

- (NSString*)gaia {
  return nil;
}

@end
