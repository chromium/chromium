// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/credential_exchange/model/credential_importer.h"

#import "ios/chrome/browser/credential_exchange/model/credential_exchange_passkey.h"
#import "ios/chrome/browser/credential_exchange/model/credential_exchange_password.h"
#import "ios/chrome/browser/credential_exchange/model/credential_import_manager_swift.h"

@interface CredentialImporter () <CredentialImportManagerDelegate>
@end

@implementation CredentialImporter {
  // Imports credentials through the OS ASCredentialImportManager API.
  CredentialImportManager* _credentialImportManager;

  // Delegate for CredentialImporter.
  id<CredentialImporterDelegate> _delegate;

  // Passwords received from the exporting credential manager.
  NSArray<CredentialExchangePassword*>* _passwords;

  // Passkeys received from the exporting credential manager.
  NSArray<CredentialExchangePasskey*>* _passkeys;
}

- (instancetype)initWithDelegate:(id<CredentialImporterDelegate>)delegate {
  self = [super init];
  if (self) {
    _credentialImportManager = [[CredentialImportManager alloc] init];
    _credentialImportManager.delegate = self;
    _delegate = delegate;
  }
  return self;
}

- (void)prepareImport:(NSUUID*)UUID {
  if (@available(iOS 26, *)) {
    [_credentialImportManager prepareImport:UUID];
  }
}

#pragma mark - Public

- (void)startImportingCredentialsWithSecurityDomainSecrets:
    (NSArray<NSData*>*)securityDomainSecrets {
  // TODO(crbug.com/449701042): Import passwords and passkeys.
}

#pragma mark - CredentialImportManagerDelegate

- (void)onCredentialsTranslatedWithPasswords:
            (NSArray<CredentialExchangePassword*>*)passwords
                                    passkeys:
                                        (NSArray<CredentialExchangePasskey*>*)
                                            passkeys {
  _passwords = passwords;
  _passkeys = passkeys;
  [_delegate showImportScreenWithPasswordCount:passwords.count
                                  passkeyCount:passkeys.count];
}

@end
