// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/credential_exchange/coordinator/credential_import_mediator.h"

#import "base/check.h"
#import "base/functional/callback_helpers.h"
#import "base/not_fatal_until.h"
#import "base/task/bind_post_task.h"
#import "components/password_manager/core/browser/import/import_results.h"
#import "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#import "components/password_manager/core/common/password_manager_pref_names.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/sync/service/sync_service.h"
#import "components/webauthn/core/browser/passkey_model.h"
#import "ios/chrome/browser/credential_exchange/model/credential_importer.h"
#import "ios/chrome/browser/credential_exchange/public/credential_import_stage.h"
#import "ios/chrome/browser/credential_exchange/ui/credential_import_consumer.h"
#import "ios/chrome/browser/data_import/public/credential_import_item.h"
#import "ios/chrome/browser/data_import/public/credential_import_item_favicon_data_source.h"
#import "ios/chrome/browser/data_import/public/import_data_item.h"
#import "ios/chrome/browser/data_import/public/passkey_import_item.h"
#import "ios/chrome/browser/data_import/public/password_import_item.h"
#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/browser/passwords/model/password_manager_util_ios.h"
#import "ios/chrome/browser/shared/ui/util/url_with_title.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/chrome/common/ui/favicon/favicon_constants.h"
#import "ui/gfx/favicon_size.h"
#import "url/gurl.h"

namespace {

using ::password_manager::prefs::kCredentialsEnablePasskeys;
using ::password_manager::prefs::kCredentialsEnableService;
using ::signin::ConsentLevel;

// Returns true if an import is blocked by a policy with `pref_name`.
bool ImportBlockedByPolicy(const PrefService* pref_service,
                           const char* pref_name) {
  return pref_service && pref_service->IsManagedPreference(pref_name) &&
         !pref_service->GetBoolean(pref_name);
}

}  // namespace

@interface CredentialImportMediator () <CredentialImporterDelegate,
                                        CredentialImportItemFaviconDataSource>
@end

@implementation CredentialImportMediator {
  // Responsible for interacting with the OS credential exchange libraries.
  CredentialImporter* _credentialImporter;

  // Delegate for this mediator.
  id<CredentialImportMediatorDelegate> _delegate;

  // Used to provide information about the user's account.
  raw_ptr<signin::IdentityManager> _identityManager;

  // Used by the `PasswordImporter` class. Needs to be kept alive during import.
  std::unique_ptr<password_manager::SavedPasswordsPresenter>
      _savedPasswordsPresenter;

  // Fetches favicons for credentials items.
  raw_ptr<FaviconLoader> _faviconLoader;

  // Used to check whether the user is syncing passwords.
  raw_ptr<syncer::SyncService> _syncService;

  // Used to check the state of user's policies.
  raw_ptr<PrefService> _prefService;
}

- (instancetype)initWithUUID:(NSUUID*)UUID
                    delegate:(id<CredentialImportMediatorDelegate>)delegate
             identityManager:(signin::IdentityManager*)identityManager
     savedPasswordsPresenter:
         (std::unique_ptr<password_manager::SavedPasswordsPresenter>)
             savedPasswordsPresenter
                passkeyModel:(webauthn::PasskeyModel*)passkeyModel
               faviconLoader:(FaviconLoader*)faviconLoader
                 syncService:(syncer::SyncService*)syncService
                 prefService:(PrefService*)prefService {
  self = [super init];
  if (self) {
    _savedPasswordsPresenter = std::move(savedPasswordsPresenter);
    _savedPasswordsPresenter->Init();
    _credentialImporter = [[CredentialImporter alloc]
               initWithDelegate:self
        savedPasswordsPresenter:_savedPasswordsPresenter.get()
                   passkeyModel:passkeyModel];
    [_credentialImporter prepareImport:UUID];
    _delegate = delegate;
    _identityManager = identityManager;
    _faviconLoader = faviconLoader;
    _syncService = syncService;
    _prefService = prefService;
  }
  return self;
}

- (void)setConsumer:(id<CredentialImportConsumer>)consumer {
  if (_consumer == consumer) {
    return;
  }

  _consumer = consumer;

  // Sign-in is required as a first step in the import flow.
  CHECK(_identityManager->HasPrimaryAccount(ConsentLevel::kSignin),
        base::NotFatalUntil::M152);
  [_consumer setUserEmail:_identityManager
                              ->GetPrimaryAccountInfo(ConsentLevel::kSignin)
                              .email];
}

#pragma mark - Public

- (void)startImportingCredentialsWithTrustedVaultKeys:
    (webauthn::SharedKeyList)trustedVaultKeys {
  [_consumer transitionToImportStage:CredentialImportStage::kImporting];
  self.importStage = CredentialImportStage::kImporting;
  [_credentialImporter
      startImportingCredentialsWithTrustedVaultKeys:std::move(
                                                        trustedVaultKeys)];
}

#pragma mark - CredentialImporterDelegate

- (void)showImportScreenWithPasswordCount:(NSInteger)passwordCount
                             passkeyCount:(NSInteger)passkeyCount
                      exporterDisplayName:(NSString*)exporterDisplayName {
  if (passwordCount == 0 && passkeyCount == 0) {
    [_delegate showNothingImportedScreen];
    return;
  }

  // Check blocking policies only if there are some credentials of given type.
  BOOL passwordsBlockedByPolicy =
      passwordCount > 0 &&
      ImportBlockedByPolicy(_prefService, kCredentialsEnableService);
  BOOL passkeysBlockedByPolicy =
      passkeyCount > 0 &&
      ImportBlockedByPolicy(_prefService, kCredentialsEnablePasskeys);
  if (passwordsBlockedByPolicy && passkeysBlockedByPolicy) {
    [_delegate showNothingImportedEnterpriseScreen];
    return;
  }

  self.importingPasskeys = passkeyCount > 0;
  [_consumer setExporterDisplayName:exporterDisplayName];
  [_consumer
      setImportDataItem:[[ImportDataItem alloc]
                            initWithType:ImportDataItemType::kPasswords
                                  status:ImportDataItemImportStatus::kReady
                                   count:passwordCount]];
  [_consumer
      setImportDataItem:[[ImportDataItem alloc]
                            initWithType:ImportDataItemType::kPasskeys
                                  status:ImportDataItemImportStatus::kReady
                                   count:passkeyCount]];

  [_delegate showImportScreen];
}

- (void)showConflictResolutionScreenWithPasswords:
            (NSArray<PasswordImportItem*>*)passwords
                                         passkeys:(NSArray<PasskeyImportItem*>*)
                                                      passkeys {
  CHECK(passwords.count > 0ul || passkeys.count > 0ul);
  NSArray<PasswordImportItem*>* passwordsWithFaviconDataSource =
      [self passwordItemsWithFaviconDataSource:passwords];
  NSArray<PasskeyImportItem*>* passkeysWithFaviconDataSource =
      [self passkeyItemsWithFaviconDataSource:passkeys];
  [_delegate
      showConflictResolutionScreenWithPasswords:passwordsWithFaviconDataSource
                                       passkeys:passkeysWithFaviconDataSource];
}

- (void)onPasswordsImported:(const password_manager::ImportResults&)results {
  self.invalidPasswords = [self
      passwordItemsWithFaviconDataSource:
          [PasswordImportItem passwordImportItemsFromImportResults:results]];
  ImportDataItem* item =
      [[ImportDataItem alloc] initWithType:ImportDataItemType::kPasswords
                                    status:ImportDataItemImportStatus::kImported
                                     count:results.number_imported];
  item.invalidCount = self.invalidPasswords.count;
  [_consumer setImportDataItem:item];
}

- (void)onPasskeysImported:(int)passkeysImported
                   invalid:(NSArray<PasskeyImportItem*>*)invalid {
  _invalidPasskeys = [self passkeyItemsWithFaviconDataSource:invalid];
  ImportDataItem* item =
      [[ImportDataItem alloc] initWithType:ImportDataItemType::kPasskeys
                                    status:ImportDataItemImportStatus::kImported
                                     count:passkeysImported];
  item.invalidCount = self.invalidPasskeys.count;
  [_consumer setImportDataItem:item];
}

- (void)onImportFinished {
  self.importStage = CredentialImportStage::kImported;
  [_consumer transitionToImportStage:self.importStage];
}

- (void)onImportError {
  [_delegate showGenericError];
}

#pragma mark - DataImportCredentialConflictMutator

- (void)continueToImportPasswords:(NSArray<NSNumber*>*)passwordIdentifiers
                         passkeys:(NSArray<NSNumber*>*)passkeyIdentifiers {
  std::vector<int> selectedPasswordIds;
  for (NSNumber* identifier in passwordIdentifiers) {
    selectedPasswordIds.push_back([identifier intValue]);
  }
  std::vector<int> selectedPasskeyIds;
  for (NSNumber* identifier in passkeyIdentifiers) {
    selectedPasskeyIds.push_back([identifier intValue]);
  }
  [_credentialImporter finishImportWithSelectedPasswordIds:selectedPasswordIds
                                        selectedPasskeyIds:selectedPasskeyIds];
}

#pragma mark - CredentialImportItemFaviconDataSource

- (BOOL)credentialImportItem:(CredentialImportItem*)item
    loadFaviconAttributesWithUIHandler:(ProceduralBlock)handler {
  // Make sure `handler` is run on the original sequence.
  base::RepeatingClosure faviconLoadClosure =
      base::BindPostTask(base::SequencedTaskRunner::GetCurrentDefault(),
                         base::BindRepeating(handler));
  ProceduralBlock faviconLoadCompletion =
      base::CallbackToBlock(std::move(faviconLoadClosure));
  auto faviconLoadedBlock = ^(FaviconAttributes* attributes, bool cached) {
    item.faviconAttributes = attributes;
    faviconLoadCompletion();
  };
  if (item.url) {
    // Only fallback to Google server if the user is syncing passwords, ensuring
    // privacy for non-syncing users.
    bool fallbackToGoogleServer =
        password_manager_util::IsSavingPasswordsToAccountWithNormalEncryption(
            _syncService);
    _faviconLoader->FaviconForPageUrl(item.url.URL, kDesiredSmallFaviconSizePt,
                                      kMinFaviconSizePt, fallbackToGoogleServer,
                                      faviconLoadedBlock);
  } else {
    // If the URL does not exist, return the monogram for the username.
    CHECK_GT(item.username.length, 0u);
    NSString* monogram =
        [[item.username substringToIndex:1] localizedUppercaseString];
    faviconLoadedBlock(
        [FaviconAttributes
            attributesWithMonogram:monogram
                         textColor:
                             [UIColor colorWithWhite:
                                          kFallbackIconDefaultTextColorGrayscale
                                               alpha:1]
                   backgroundColor:UIColor.clearColor
            defaultBackgroundColor:YES],
        /*cached=*/true);
  }
  return YES;
}

#pragma mark - Private

// Attach favicon loader to each element in `passwords`.
- (NSArray<PasswordImportItem*>*)passwordItemsWithFaviconDataSource:
    (NSArray<PasswordImportItem*>*)passwords {
  NSArray<PasswordImportItem*>* newPasswords =
      [NSArray arrayWithArray:passwords];
  for (PasswordImportItem* password in newPasswords) {
    password.faviconDataSource = self;
  }
  return newPasswords;
}

// Attach favicon loader to each element in `passkeys`.
- (NSArray<PasskeyImportItem*>*)passkeyItemsWithFaviconDataSource:
    (NSArray<PasskeyImportItem*>*)passkeys {
  NSArray<PasskeyImportItem*>* newPasskeys = [NSArray arrayWithArray:passkeys];
  for (PasskeyImportItem* passkey in newPasskeys) {
    passkey.faviconDataSource = self;
  }
  return newPasskeys;
}

@end
