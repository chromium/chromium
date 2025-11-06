// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/credential_exchange/model/credential_importer.h"

#import <vector>

#import "base/strings/sys_string_conversions.h"
#import "components/password_manager/core/browser/import/csv_password.h"
#import "components/password_manager/core/browser/import/import_results.h"
#import "components/password_manager/core/browser/import/password_importer.h"
#import "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#import "ios/chrome/browser/credential_exchange/model/credential_exchange_passkey.h"
#import "ios/chrome/browser/credential_exchange/model/credential_exchange_password.h"
#import "ios/chrome/browser/credential_exchange/model/credential_import_manager_swift.h"
#import "net/base/apple/url_conversions.h"
#import "url/gurl.h"

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

  // Used to import passwords to the password store. Handles identifying errors
  // and conflicts.
  std::unique_ptr<password_manager::PasswordImporter> _passwordImporter;
}

- (instancetype)initWithDelegate:(id<CredentialImporterDelegate>)delegate
         savedPasswordsPresenter:(password_manager::SavedPasswordsPresenter*)
                                     savedPasswordsPresenter {
  self = [super init];
  if (self) {
    _credentialImportManager = [[CredentialImportManager alloc] init];
    _credentialImportManager.delegate = self;
    _delegate = delegate;
    _passwordImporter = std::make_unique<password_manager::PasswordImporter>(
        savedPasswordsPresenter, /*user_confirmation_required=*/true);
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
  [self startImportingPasswords];
  // TODO(crbug.com/449701042): Start importing passkeys.
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

#pragma mark - Private

// Converts `_passwords` into structures used by `_passwordImporter` and starts
// the importing process.
- (void)startImportingPasswords {
  std::vector<password_manager::CSVPassword> csvPasswords;
  csvPasswords.reserve(_passwords.count);
  for (CredentialExchangePassword* password : _passwords) {
    csvPasswords.push_back(password_manager::CSVPassword(
        net::GURLWithNSURL(password.URL),
        base::SysNSStringToUTF8(password.username),
        base::SysNSStringToUTF8(password.password),
        base::SysNSStringToUTF8(password.note),
        password_manager::CSVPassword::Status::kOK));
  }

  __weak __typeof(self) weakSelf = self;
  _passwordImporter->Import(
      csvPasswords, password_manager::PasswordForm::Store::kAccountStore,
      base::BindOnce(^(const password_manager::ImportResults& results) {
        [weakSelf onPasswordImporterParsingFinishedWithResults:results];
      }));
}

// Called when `_passwordImporter` finishes processing passwords.
// TODO(crbug.com/449701042): Handle next stages of import.
- (void)onPasswordImporterParsingFinishedWithResults:
    (const password_manager::ImportResults&)results {
}

@end
