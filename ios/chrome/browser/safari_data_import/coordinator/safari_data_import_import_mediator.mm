// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safari_data_import/coordinator/safari_data_import_import_mediator.h"

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/strings/sys_string_conversions.h"
#import "components/application_locale_storage/application_locale_storage.h"
#import "components/user_data_importer/ios/ios_bookmark_parser.h"
#import "components/user_data_importer/utility/safari_data_importer.h"
#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/browser/safari_data_import/model/ios_safari_data_import_client.h"
#import "ios/chrome/browser/safari_data_import/public/password_import_item.h"
#import "ios/chrome/browser/safari_data_import/public/password_import_item_favicon_data_source.h"
#import "ios/chrome/browser/safari_data_import/public/safari_data_import_stage.h"
#import "ios/chrome/browser/safari_data_import/public/safari_data_item.h"
#import "ios/chrome/browser/safari_data_import/public/safari_data_item_consumer.h"
#import "ios/chrome/browser/safari_data_import/ui/safari_data_import_import_stage_transition_handler.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ui/gfx/favicon_size.h"
#import "url/gurl.h"

@interface SafariDataImportImportMediator () <
    PasswordImportItemFaviconDataSource>

@end

@implementation SafariDataImportImportMediator {
  /// The importer instance and the client.
  std::unique_ptr<user_data_importer::SafariDataImporter> _importer;
  std::unique_ptr<IOSSafariDataImportClient> _importClient;
  /// If `YES`, the import client is configured; otherwise it needs to be
  /// configured.
  BOOL _importClientReady;
  /// Favicon loader for password items.
  raw_ptr<FaviconLoader> _faviconLoader;
  /// The `SavedPasswordsPresenter` instance being used by the imported. Needs
  /// to be kept alive in the duration of the importer.
  std::unique_ptr<password_manager::SavedPasswordsPresenter>
      _savedPasswordsPresenter;
  /// Whether we should allow the user to select a file. Workaround for file
  /// picker latencies.
  BOOL _disableFileSelection;
  /// Whether the mediator is disconnected.
  BOOL _disconnected;
}

- (instancetype)
    initWithSavedPasswordsPresenter:
        (std::unique_ptr<password_manager::SavedPasswordsPresenter>)
            savedPasswordsPresenter
                paymentsDataManager:
                    (autofill::PaymentsDataManager*)paymentsDataManager
                     historyService:(history::HistoryService*)historyService
                      bookmarkModel:(bookmarks::BookmarkModel*)bookmarkModel
                   readingListModel:(ReadingListModel*)readingListModel
                        syncService:(syncer::SyncService*)syncService
                      faviconLoader:(FaviconLoader*)faviconLoader {
  self = [super init];
  if (self) {
    CHECK(faviconLoader);
    _importClient = std::make_unique<IOSSafariDataImportClient>();
    _savedPasswordsPresenter = std::move(savedPasswordsPresenter);
    _savedPasswordsPresenter->Init();
    std::unique_ptr<user_data_importer::IOSBookmarkParser> bookmarkParser =
        std::make_unique<user_data_importer::IOSBookmarkParser>();
    std::string locale =
        GetApplicationContext()->GetApplicationLocaleStorage()->Get();
    _importer = std::make_unique<user_data_importer::SafariDataImporter>(
        _importClient.get(), _savedPasswordsPresenter.get(),
        paymentsDataManager, historyService, bookmarkModel, readingListModel,
        syncService, std::move(bookmarkParser), locale);
    _faviconLoader = faviconLoader;
  }
  return self;
}

- (void)dealloc {
  CHECK(_disconnected, base::NotFatalUntil::M143);
}

- (void)reset {
  _disableFileSelection = NO;
}

- (void)importItems {
  [self continueToImportPasswords:[NSArray array]];
}

- (NSArray<PasswordImportItem*>*)conflictingPasswords {
  return
      [self passwordItemsWithFaviconDataSource:_importClient
                                                   ->GetConflictingPasswords()];
}

- (NSArray<PasswordImportItem*>*)invalidPasswords {
  return [self
      passwordItemsWithFaviconDataSource:_importClient->GetInvalidPasswords()];
}

- (void)disconnect {
  [self reset];
  _importer.reset();
  _savedPasswordsPresenter.reset();
  _importClient.reset();
  _disconnected = YES;
}

#pragma mark - PasswordImportItemFaviconDataSource

- (BOOL)passwordImportItem:(PasswordImportItem*)item
    loadFaviconAttributesWithCompletion:(ProceduralBlock)completion {
  GURL url(base::SysNSStringToUTF8(item.url));
  auto faviconLoadedBlock = ^(FaviconAttributes* attributes) {
    item.faviconAttributes = attributes;
    completion();
  };
  _faviconLoader->FaviconForPageUrlOrHost(url, gfx::kFaviconSize,
                                          faviconLoadedBlock);
  return YES;
}

#pragma mark - SafariDataImportPasswordConflictMutator

- (void)continueToImportPasswords:(NSArray<NSNumber*>*)passwordIdentifiers {
  std::vector<int> selected_password_ids;
  for (NSNumber* identifier in passwordIdentifiers) {
    selected_password_ids.push_back([identifier intValue]);
  }
  [self.importStageTransitionHandler transitionToNextImportStage];
  _importer->CompleteImport(selected_password_ids);
}

#pragma mark - UIDocumentPickerDelegate

- (void)documentPicker:(UIDocumentPickerViewController*)controller
    didPickDocumentsAtURLs:(NSArray<NSURL*>*)urls {
  if (_disableFileSelection) {
    return;
  }
  NSURL* file = urls.firstObject;
  if (!file) {
    return;
  }
  _disableFileSelection = YES;
  [self setUpImportClient];
  _importer->PrepareImport(base::apple::NSURLToFilePath(file));
}

- (void)documentPickerWasCancelled:(UIDocumentPickerViewController*)controller {
  [self.importStageTransitionHandler resetToInitialImportStage:YES];
}

#pragma mark - Private

/// Registers protcols and callbacks to the import client. Only after this
/// method has been called will the import client work as expected.
- (void)setUpImportClient {
  if (_importClientReady) {
    return;
  }
  _importClient->SetSafariDataItemConsumer(self.itemConsumer);
  __weak SafariDataImportImportMediator* weakSelf = self;
  _importClient->RegisterCallbackOnImportFailure(base::BindOnce(^{
    __strong SafariDataImportImportMediator* strongSelf = weakSelf;
    [strongSelf reset];
    [strongSelf.importStageTransitionHandler resetToInitialImportStage:NO];
  }));
  _importClientReady = YES;
}

/// Attach favicon loader to each element in `passwords`.
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
