// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/credential_exchange/coordinator/credential_import_mediator.h"

#import "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#import "ios/chrome/browser/credential_exchange/model/credential_importer.h"
#import "ios/chrome/browser/credential_exchange/ui/credential_import_consumer.h"
#import "ios/chrome/browser/data_import/public/import_data_item.h"

@interface CredentialImportMediator () <CredentialImporterDelegate>
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
}

- (instancetype)initWithUUID:(NSUUID*)UUID
                    delegate:(id<CredentialImportMediatorDelegate>)delegate
                   userEmail:(std::string)userEmail
     savedPasswordsPresenter:
         (std::unique_ptr<password_manager::SavedPasswordsPresenter>)
             savedPasswordsPresenter {
  self = [super init];
  if (self) {
    _savedPasswordsPresenter = std::move(savedPasswordsPresenter);
    _savedPasswordsPresenter->Init();
    _credentialImporter = [[CredentialImporter alloc]
               initWithDelegate:self
        savedPasswordsPresenter:_savedPasswordsPresenter.get()];
    [_credentialImporter prepareImport:UUID];
    _delegate = delegate;
    _userEmail = std::move(userEmail);
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

- (void)startImportingCredentialsWithSecurityDomainSecrets:
    (NSArray<NSData*>*)securityDomainSecrets {
  [_credentialImporter
      startImportingCredentialsWithSecurityDomainSecrets:securityDomainSecrets];
}

#pragma mark - CredentialImporterDelegate

- (void)showImportScreenWithPasswordCount:(NSInteger)passwordCount
                             passkeyCount:(NSInteger)passkeyCount {
  [_consumer
      setImportDataItem:[[ImportDataItem alloc]
                            initWithType:ImportDataItemType::kPasswords
                                  status:ImportDataItemImportStatus::kReady
                                   count:passwordCount]];
  [_consumer
      setImportDataItem:[[ImportDataItem alloc]
                            // TODO(crbug.com/450982128): Add passkey type.
                            initWithType:ImportDataItemType::kBookmarks
                                  status:ImportDataItemImportStatus::kReady
                                   count:passkeyCount]];

  [_delegate showImportScreen];
}

@end
