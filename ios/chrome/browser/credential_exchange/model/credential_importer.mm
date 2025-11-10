// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/credential_exchange/model/credential_importer.h"

#import <vector>

#import "base/apple/foundation_util.h"
#import "base/rand_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/password_manager/core/browser/import/csv_password.h"
#import "components/password_manager/core/browser/import/import_results.h"
#import "components/password_manager/core/browser/import/password_importer.h"
#import "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#import "components/sync/protocol/webauthn_credential_specifics.pb.h"
#import "components/webauthn/core/browser/import/import_processing_result.h"
#import "components/webauthn/core/browser/import/passkey_importer.h"
#import "components/webauthn/core/browser/passkey_model.h"
#import "components/webauthn/core/browser/passkey_model_utils.h"
#import "ios/chrome/browser/credential_exchange/model/credential_exchange_passkey.h"
#import "ios/chrome/browser/credential_exchange/model/credential_exchange_password.h"
#import "ios/chrome/browser/credential_exchange/model/credential_import_manager_swift.h"
#import "net/base/apple/url_conversions.h"
#import "url/gurl.h"

namespace {

std::string DataToString(NSData* data) {
  return std::string(static_cast<const char*>(data.bytes), data.length);
}

}  // namespace

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

  // Used to import passkeys and handle conflicts with existing passkeys.
  std::unique_ptr<webauthn::PasskeyImporter> _passkeyImporter;
}

- (instancetype)initWithDelegate:(id<CredentialImporterDelegate>)delegate
         savedPasswordsPresenter:
             (password_manager::SavedPasswordsPresenter*)savedPasswordsPresenter
                    passkeyModel:(webauthn::PasskeyModel*)passkeyModel {
  self = [super init];
  if (self) {
    _credentialImportManager = [[CredentialImportManager alloc] init];
    _credentialImportManager.delegate = self;
    _delegate = delegate;
    _passwordImporter = std::make_unique<password_manager::PasswordImporter>(
        savedPasswordsPresenter, /*user_confirmation_required=*/true);
    CHECK(passkeyModel);
    _passkeyImporter =
        std::make_unique<webauthn::PasskeyImporter>(*passkeyModel);
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
  [self startImportingPasskeys:securityDomainSecrets];
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

// Converts `_passkeys` into protos used by `_passkeyImporter` and starts the
// importing process.
- (void)startImportingPasskeys:(NSArray<NSData*>*)securityDomainSecrets {
  if (_passkeys.count == 0) {
    return;
  }

  // `hw_protected` security domain currently supports a single secret.
  CHECK(securityDomainSecrets.count == 1);
  base::span<const uint8_t> securityDomainSecret =
      base::apple::NSDataToSpan(securityDomainSecrets[0]);
  int64_t timeNow = base::Time::Now().InMillisecondsSinceUnixEpoch();
  std::vector<sync_pb::WebauthnCredentialSpecifics> passkeys;

  for (CredentialExchangePasskey* passkey : _passkeys) {
    // TODO(crbug.com/458337350): Handle extensions.
    sync_pb::WebauthnCredentialSpecifics specifics;
    sync_pb::WebauthnCredentialSpecifics_Encrypted encrypted;
    encrypted.set_private_key(passkey.privateKey.bytes,
                              passkey.privateKey.length);

    // Encrypting might fail here. Don't skip the passkey, as `passkeyImporter`
    // deals with parsing all the errors.
    // TODO(crbug.com/458337350): Consider passing CredentialExchangePasskey or
    // NSData instead or just log failure here.
    webauthn::passkey_model_utils::EncryptWebauthnCredentialSpecificsData(
        securityDomainSecret, encrypted, &specifics);

    specifics.set_sync_id(
        base::RandBytesAsString(webauthn::passkey_model_utils::kSyncIdLength));
    specifics.set_credential_id(DataToString(passkey.credentialId));
    specifics.set_user_id(DataToString(passkey.userId));
    specifics.set_rp_id(base::SysNSStringToUTF8(passkey.rpId));
    specifics.set_user_name(base::SysNSStringToUTF8(passkey.userName));
    specifics.set_user_display_name(
        base::SysNSStringToUTF8(passkey.userDisplayName));
    specifics.set_creation_time(timeNow);
    passkeys.push_back(specifics);
  }

  __weak __typeof(self) weakSelf = self;
  _passkeyImporter->StartImport(
      std::move(passkeys),
      base::BindOnce(^(const webauthn::ImportProcessingResult& result) {
        [weakSelf onPasskeyImporterParsingFinishedWithResult:result];
      }));
}

// Called when `_passkeyImporter` finishes processing passkeys.
// TODO(crbug.com/450982128): Handle next stages of import.
- (void)onPasskeyImporterParsingFinishedWithResult:
    (const webauthn::ImportProcessingResult&)result {
}

@end
