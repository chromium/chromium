// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webauthn/model/credential_importer.h"

#import "ios/chrome/browser/webauthn/model/credential_exchange_passkey.h"
#import "ios/chrome/browser/webauthn/model/credential_exchange_password.h"
#import "ios/chrome/browser/webauthn/model/credential_import_manager_swift.h"

@interface CredentialImporter () <CredentialImportManagerDelegate>
@end

@implementation CredentialImporter {
  // Imports credentials through the OS ASCredentialImportManager API.
  CredentialImportManager* _credentialImportManager;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _credentialImportManager = [[CredentialImportManager alloc] init];
    _credentialImportManager.delegate = self;
  }
  return self;
}

- (void)startImport:(NSUUID*)UUID {
  if (@available(iOS 26, *)) {
    [_credentialImportManager startImport:UUID];
  }
}

#pragma mark - CredentialImportManagerDelegate

- (void)onCredentialsParsedWithPasswords:
            (NSArray<CredentialExchangePassword*>*)passwords
                                passkeys:(NSArray<CredentialExchangePasskey*>*)
                                             passkeys {
  // TODO(crbug.com/445889719): Handle imported data.
}

@end
