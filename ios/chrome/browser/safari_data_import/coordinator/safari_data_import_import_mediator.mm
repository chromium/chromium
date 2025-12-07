// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safari_data_import/coordinator/safari_data_import_import_mediator.h"

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#import "components/application_locale_storage/application_locale_storage.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/sync/service/sync_service.h"
#import "components/user_data_importer/ios/ios_bookmark_parser.h"
#import "components/user_data_importer/utility/safari_data_importer.h"
#import "ios/chrome/browser/data_import/public/import_data_item.h"
#import "ios/chrome/browser/data_import/public/import_data_item_consumer.h"
#import "ios/chrome/browser/data_import/public/password_import_item.h"
#import "ios/chrome/browser/data_import/public/password_import_item_favicon_data_source.h"
#import "ios/chrome/browser/data_import/ui/data_import_import_stage_transition_handler.h"
#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/browser/safari_data_import/model/ios_safari_data_import_client.h"
#import "ios/chrome/browser/safari_data_import/public/safari_data_import_stage.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/ui/util/url_with_title.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
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
  /// Whether the mediator is disconnected.
  BOOL _disconnected;
  /// The URL of the file being imported, which requires security-scoped access.
  NSURL* _currentSecurityScopedURL;
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
                        prefService:(PrefService*)prefService
                      faviconLoader:(FaviconLoader*)faviconLoader {
  self = [super init];
  if (self) {
    CHECK(faviconLoader);
    if (syncService) {
      std::string email = syncService->GetAccountInfo().email;
      if (!email.empty()) {
        _email = [NSString stringWithUTF8String:email.c_str()];
      }
    }
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
        syncService, prefService, std::move(bookmarkParser), locale);
    _faviconLoader = faviconLoader;
  }
  return self;
}

- (void)dealloc {
  CHECK(_disconnected, base::NotFatalUntil::M143);
}

- (void)reset {
  if (_currentSecurityScopedURL) {
    [_currentSecurityScopedURL stopAccessingSecurityScopedResource];
    _currentSecurityScopedURL = nil;
  }
}

- (NSString*)filename {
  return _currentSecurityScopedURL.lastPathComponent;
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

- (NSError*)deleteFile {
  NSError* error = nil;
  [[NSFileManager defaultManager] removeItemAtURL:_currentSecurityScopedURL
                                            error:&error];
  [self reset];
  return error;
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
    loadFaviconAttributesWithUIHandler:(ProceduralBlock)UIHandler {
  auto faviconLoadedBlock = ^(FaviconAttributes* attributes, bool cached) {
    item.faviconAttributes = attributes;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(UIHandler));
  };
  if (item.url) {
    _faviconLoader->FaviconForPageUrlOrHost(item.url.URL, gfx::kFaviconSize,
                                            faviconLoadedBlock);
  } else {
    /// If the URL does not exist, return the monogram for the username.
    CHECK(item.username.length > 0);
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
        /*cached*/ true);
  }
  return YES;
}

#pragma mark - PasswordConflictMutator

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
  /// Early exit. Workaround for file picker latencies.
  if (_currentSecurityScopedURL) {
    return;
  }
  NSURL* securityScopedURL = urls.firstObject;
  if (![securityScopedURL startAccessingSecurityScopedResource]) {
    [self.importStageTransitionHandler
        resetToInitialImportStage:DataImportResetReason::kNoImportableData];
    return;
  }
  _currentSecurityScopedURL = securityScopedURL;
  [self setUpImportClient];
  _importer->PrepareImport(
      base::apple::NSURLToFilePath(_currentSecurityScopedURL));
}

- (void)documentPickerWasCancelled:(UIDocumentPickerViewController*)controller {
  [self.importStageTransitionHandler
      resetToInitialImportStage:DataImportResetReason::kUserInitiated];
}

#pragma mark - Private

/// Registers protcols and callbacks to the import client. Only after this
/// method has been called will the import client work as expected.
- (void)setUpImportClient {
  if (_importClientReady) {
    return;
  }
  _importClient->SetImportDataItemConsumer(self.itemConsumer);
  __weak SafariDataImportImportMediator* weakSelf = self;
  _importClient->RegisterCallbackOnImportFailure(base::BindOnce(^{
    __strong SafariDataImportImportMediator* strongSelf = weakSelf;
    [strongSelf reset];
    [strongSelf.importStageTransitionHandler
        resetToInitialImportStage:DataImportResetReason::kNoImportableData];
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
