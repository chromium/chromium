// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/credential_exchange/model/credential_importer.h"

#import <vector>

#import "base/apple/foundation_util.h"
#import "base/barrier_closure.h"
#import "base/rand_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/thread_pool.h"
#import "components/password_manager/core/browser/import/csv_password.h"
#import "components/password_manager/core/browser/import/import_results.h"
#import "components/password_manager/core/browser/import/password_importer.h"
#import "components/password_manager/core/browser/password_manager_util.h"
#import "components/password_manager/core/browser/ui/credential_utils.h"
#import "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#import "components/sync/protocol/webauthn_credential_specifics.pb.h"
#import "components/webauthn/core/browser/import/import_processing_result.h"
#import "components/webauthn/core/browser/import/passkey_importer.h"
#import "components/webauthn/core/browser/passkey_model.h"
#import "components/webauthn/core/browser/passkey_model_utils.h"
#import "ios/chrome/browser/credential_exchange/model/credential_exchange_passkey.h"
#import "ios/chrome/browser/credential_exchange/model/credential_exchange_password.h"
#import "ios/chrome/browser/data_import/public/password_import_item.h"
#import "net/base/apple/url_conversions.h"
#import "url/gurl.h"

namespace {

// Count of credential types that are currently supported by the importer.
constexpr int kSupportedCredentialTypesCount = 2;

std::string DataToString(NSData* data) {
  return std::string(static_cast<const char*>(data.bytes), data.length);
}

}  // namespace

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

  // Caches the results of initial processing of passwords.
  password_manager::ImportResults _passwordImportResult;

  // Caches the results of initial processing of passkeys.
  webauthn::ImportProcessingResult _passkeyImportResult;

  // Barrier closure that should run after initial processing finishes for all
  // supported credential types.
  base::RepeatingClosure _allCredentialTypesProcessedClosure;

  // Count of different credential types that are present on the import list.
  NSInteger _presentCredentialTypesCount;
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
  __weak __typeof(self) weakSelf = self;
  _allCredentialTypesProcessedClosure =
      base::BarrierClosure(kSupportedCredentialTypesCount, base::BindOnce(^{
                             [weakSelf onAllCredentialTypesProcessed];
                           }));
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE}, base::BindOnce(^{
        return [weakSelf
            translateCredentialExchangePasskeys:securityDomainSecrets];
      }),
      base::BindOnce(
          ^(std::vector<sync_pb::WebauthnCredentialSpecifics> passkeys) {
            [weakSelf startImportingPasskeys:std::move(passkeys)];
          }));
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE}, base::BindOnce(^{
        return [weakSelf translateCredentialExchangePasswords];
      }),
      base::BindOnce(
          ^(std::vector<password_manager::CSVPassword> csvPasswords) {
            [weakSelf startImportingPasswords:std::move(csvPasswords)];
          }));
}

- (void)finishImportWithSelectedPasswordIds:
    (const std::vector<int>&)selectedPasswordIds {
  __weak __typeof(_delegate) weakDelegate = _delegate;
  base::RepeatingClosure allCredentialTypesImportedClosure =
      base::BarrierClosure(_presentCredentialTypesCount, base::BindOnce(^{
                             [weakDelegate onImportFinished];
                           }));

  if (_passwords.count > 0) {
    _passwordImporter->ContinueImport(
        selectedPasswordIds,
        base::BindOnce(^(const password_manager::ImportResults& results) {
          [weakDelegate onPasswordsImported:results];
        }).Then(allCredentialTypesImportedClosure));
  }
  if (_passkeys.count > 0) {
    // TODO(crbug.com/450982128): Pass chosen ids from the conflict UI.
    _passkeyImporter->FinishImport(
        /*selected_conflicting_passkey_ids=*/{},
        base::BindOnce(^(int passkeysImported) {
          [weakDelegate onPasskeysImported:passkeysImported];
        }).Then(allCredentialTypesImportedClosure));
  }
}

#pragma mark - CredentialImportManagerDelegate

- (void)onCredentialsTranslatedWithPasswords:
            (NSArray<CredentialExchangePassword*>*)passwords
                                    passkeys:
                                        (NSArray<CredentialExchangePasskey*>*)
                                            passkeys {
  _passwords = passwords;
  _passkeys = passkeys;
  _presentCredentialTypesCount =
      (passwords.count > 0 ? 1 : 0) + (passkeys.count > 0 ? 1 : 0);
  [_delegate showImportScreenWithPasswordCount:passwords.count
                                  passkeyCount:passkeys.count];
}

#pragma mark - Private

// Converts `_passwords` into structures used by `_passwordImporter`.
- (std::vector<password_manager::CSVPassword>)
    translateCredentialExchangePasswords {
  std::vector<password_manager::CSVPassword> csvPasswords;
  csvPasswords.reserve(_passwords.count);
  for (CredentialExchangePassword* password : _passwords) {
    std::string username = base::SysNSStringToUTF8(password.username);
    std::string passwordStr = base::SysNSStringToUTF8(password.password);
    std::string note = base::SysNSStringToUTF8(password.note);

    // Even though the URL might not be valid, this status is just about parsing
    // the fields. `_passwordImporter` will handle the invalid URL internally.
    password_manager::CSVPassword::Status status =
        password_manager::CSVPassword::Status::kOK;

    // URL field is optional, so it might be nil. Pass as empty and continue.
    if (!password.URL) {
      csvPasswords.emplace_back(password_manager::CSVPassword(
          /*invalid_url=*/"", username, passwordStr, note, status));
      continue;
    }

    // Password manager expects urls to be in HTTP or HTTPS scheme. The imported
    // password might not contain it and e.g. just be an eTLD+1. Try adding the
    // scheme and validate the result.
    std::string urlStr = password.URL.absoluteString.UTF8String;
    GURL url = password_manager_util::ConstructGURLWithScheme(urlStr);
    if (password_manager::IsValidPasswordURL(url)) {
      csvPasswords.emplace_back(password_manager::CSVPassword(
          url, username, passwordStr, note, status));
    } else {
      csvPasswords.emplace_back(password_manager::CSVPassword(
          urlStr, username, passwordStr, note, status));
    }
  }
  return csvPasswords;
}

// Triggers initial processing of `passwords` handled by `_passwordImporter`.
- (void)startImportingPasswords:
    (std::vector<password_manager::CSVPassword>)passwords {
  if (passwords.empty()) {
    _allCredentialTypesProcessedClosure.Run();
    return;
  }

  __weak __typeof(self) weakSelf = self;
  _passwordImporter->Import(
      passwords, password_manager::PasswordForm::Store::kAccountStore,
      base::BindOnce(^(const password_manager::ImportResults& results) {
        [weakSelf onPasswordParsingFinished:results];
      }));
}

// Called when `_passwordImporter` finishes processing passwords. Caches the
// `results` and runs the barrier closure.
- (void)onPasswordParsingFinished:
    (const password_manager::ImportResults&)results {
  _passwordImportResult = results;
  _allCredentialTypesProcessedClosure.Run();
}

// Converts `_passkeys` into structures used by `_passkeyImporter`.
- (std::vector<sync_pb::WebauthnCredentialSpecifics>)
    translateCredentialExchangePasskeys:
        (NSArray<NSData*>*)securityDomainSecrets {
  if (_passkeys.count == 0) {
    return {};
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

  return passkeys;
}

// Triggers initial processing of `passkeys` handled by `_passkeyImporter`.
- (void)startImportingPasskeys:
    (std::vector<sync_pb::WebauthnCredentialSpecifics>)passkeys {
  if (passkeys.empty()) {
    _allCredentialTypesProcessedClosure.Run();
    return;
  }

  __weak __typeof(self) weakSelf = self;
  _passkeyImporter->StartImport(
      std::move(passkeys),
      base::BindOnce(^(const webauthn::ImportProcessingResult& result) {
        [weakSelf onPasskeyParsingFinished:result];
      }));
}

// Called when `_passkeyImporter` finishes processing passkeys. Caches the
// `result` and runs the barrier closure.
- (void)onPasskeyParsingFinished:
    (const webauthn::ImportProcessingResult&)result {
  _passkeyImportResult = result;
  _allCredentialTypesProcessedClosure.Run();
}

// Called when initial processing of all supported credentials types finishes.
// If there are no conflicts to be resolved by the user across all credential
// types, triggers actual import of the data. Otherwise, notifies the delegate
// to display conflict resolution UI first.
- (void)onAllCredentialTypesProcessed {
  if (_passkeyImportResult.conflicts.empty() &&
      _passwordImportResult.displayed_entries.empty()) {
    [self finishImportWithSelectedPasswordIds:{}];
    return;
  }

  // TODO(crbug.com/450982128): Pass passkey conflicts.
  [_delegate
      showConflictResolutionScreenWithPasswords:
          [PasswordImportItem
              passwordImportItemsFromImportResults:_passwordImportResult]];
}

@end
