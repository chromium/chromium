// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/credential_exchange/coordinator/credential_import_mediator.h"

#import "base/functional/callback_helpers.h"
#import "base/task/bind_post_task.h"
#import "components/password_manager/core/browser/import/import_results.h"
#import "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
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
#import "ios/chrome/browser/shared/ui/util/url_with_title.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ui/gfx/favicon_size.h"
#import "url/gurl.h"

@interface CredentialImportMediator () <CredentialImporterDelegate,
                                        CredentialImportItemFaviconDataSource>
@end

@implementation CredentialImportMediator {
  // Responsible for interacting with the OS credential exchange libraries.
  CredentialImporter* _credentialImporter;

  // Delegate for this mediator.
  id<CredentialImportMediatorDelegate> _delegate;

  // Email of the signed in user account.
  std::string _userEmail;

  // Used by the `PasswordImporter` class. Needs to be kept alive during import.
  std::unique_ptr<password_manager::SavedPasswordsPresenter>
      _savedPasswordsPresenter;

  // Fetches favicons for credentials items.
  raw_ptr<FaviconLoader> _faviconLoader;
}

- (instancetype)initWithUUID:(NSUUID*)UUID
                    delegate:(id<CredentialImportMediatorDelegate>)delegate
                   userEmail:(std::string)userEmail
     savedPasswordsPresenter:
         (std::unique_ptr<password_manager::SavedPasswordsPresenter>)
             savedPasswordsPresenter
                passkeyModel:(webauthn::PasskeyModel*)passkeyModel
               faviconLoader:(FaviconLoader*)faviconLoader {
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
    _userEmail = std::move(userEmail);
    _faviconLoader = faviconLoader;
  }
  return self;
}

- (void)setConsumer:(id<CredentialImportConsumer>)consumer {
  if (_consumer == consumer) {
    return;
  }

  _consumer = consumer;
  [_consumer setUserEmail:_userEmail];
}

#pragma mark - Public

- (void)startImportingCredentialsWithTrustedVaultKeys:
    (NSArray<NSData*>*)trustedVaultKeys {
  self.importStage = CredentialImportStage::kImporting;
  [_consumer transitionToImportStage:self.importStage];
  [_credentialImporter
      startImportingCredentialsWithTrustedVaultKeys:trustedVaultKeys];
}

#pragma mark - CredentialImporterDelegate

- (void)showImportScreenWithPasswordCount:(NSInteger)passwordCount
                             passkeyCount:(NSInteger)passkeyCount {
  self.importingPasskeys = passkeyCount > 0;
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
  [_delegate showConflictResolutionScreenWithPasswords:
                 [self passwordItemsWithFaviconDataSource:passwords]
                                              passkeys:passkeys];
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

- (void)onPasskeysImported:(int)passkeysImported {
  // TODO(crbug.com/450982128): Handle displaying errors.
  [_consumer
      setImportDataItem:[[ImportDataItem alloc]
                            initWithType:ImportDataItemType::kPasskeys
                                  status:ImportDataItemImportStatus::kImported
                                   count:passkeysImported]];
}

- (void)onImportFinished {
  self.importStage = CredentialImportStage::kImported;
  [_consumer transitionToImportStage:self.importStage];
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
    _faviconLoader->FaviconForPageUrlOrHost(item.url.URL, gfx::kFaviconSize,
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

@end
