// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safari_data_import/coordinator/safari_data_import_import_mediator.h"

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "components/application_locale_storage/application_locale_storage.h"
#import "components/user_data_importer/ios/ios_bookmark_parser.h"
#import "components/user_data_importer/utility/safari_data_importer.h"
#import "ios/chrome/browser/safari_data_import/model/ios_safari_data_import_client.h"
#import "ios/chrome/browser/safari_data_import/public/safari_data_import_stage.h"
#import "ios/chrome/browser/safari_data_import/public/safari_data_item.h"
#import "ios/chrome/browser/safari_data_import/public/safari_data_item_consumer.h"
#import "ios/chrome/browser/safari_data_import/ui/safari_data_import_import_stage_transition_handler.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"

@implementation SafariDataImportImportMediator {
  /// The importer instance and the client.
  std::unique_ptr<user_data_importer::SafariDataImporter> _importer;
  std::unique_ptr<IOSSafariDataImportClient> _importClient;
  /// If `YES`, the import client is configured; otherwise it needs to be
  /// configured.
  BOOL _importClientReady;
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
                   readingListModel:(ReadingListModel*)readingListModel {
  self = [super init];
  if (self) {
    _importClient = std::make_unique<IOSSafariDataImportClient>();
    _savedPasswordsPresenter = std::move(savedPasswordsPresenter);
    std::unique_ptr<user_data_importer::IOSBookmarkParser> bookmarkParser =
        std::make_unique<user_data_importer::IOSBookmarkParser>();
    std::string locale =
        GetApplicationContext()->GetApplicationLocaleStorage()->Get();
    _importer = std::make_unique<user_data_importer::SafariDataImporter>(
        _importClient.get(), _savedPasswordsPresenter.get(),
        paymentsDataManager, historyService, bookmarkModel, readingListModel,
        std::move(bookmarkParser), locale);
  }
  return self;
}

- (void)dealloc {
  CHECK(_disconnected, base::NotFatalUntil::M143);
}

- (void)reset {
  _disableFileSelection = NO;
}

- (void)disconnect {
  [self reset];
  _importer.reset();
  _savedPasswordsPresenter.reset();
  _importClient.reset();
  _disconnected = YES;
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

- (void)setUpImportClient {
  if (_importClientReady) {
    return;
  }
  _importClient->SetSafariDataItemConsumer(self.itemConsumer);
  __weak __typeof(self) weakSelf = self;
  _importClient->RegisterCallbackOnImportFailure(base::BindOnce(^{
    [weakSelf reset];
    [weakSelf.importStageTransitionHandler resetToInitialImportStage:NO];
  }));
  _importClientReady = YES;
}

@end
