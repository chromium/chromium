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
#import "ios/chrome/browser/safari_data_import/ui/safari_data_import_import_stage_consumer.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"

@implementation SafariDataImportImportMediator {
  /// The importer instance and the client.
  std::unique_ptr<user_data_importer::SafariDataImporter> _importer;
  std::unique_ptr<IOSSafariDataImportClient> _importClient;
  /// The `SavedPasswordsPresenter` instance being used by the imported. Needs
  /// to be kept alive in the duration of the importer.
  std::unique_ptr<password_manager::SavedPasswordsPresenter>
      _savedPasswordsPresenter;
  /// The file being processed.
  NSURL* _file;
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

- (void)disconnect {
  _importer.reset();
  _savedPasswordsPresenter.reset();
  _importClient.reset();
  _disconnected = YES;
}

#pragma mark - UIDocumentPickerDelegate

- (void)documentPicker:(UIDocumentPickerViewController*)controller
    didPickDocumentsAtURLs:(NSArray<NSURL*>*)urls {
  if (_file) {
    return;
  }
  _file = urls[0];
  _importClient->SetSafariDataItemConsumer(self.itemConsumer);
  _importer->PrepareImport(base::apple::NSURLToFilePath(_file));
}

- (void)documentPickerWasCancelled:(UIDocumentPickerViewController*)controller {
  [self.importStageConsumer
      transitionToImportStage:SafariDataImportStage::kNotStarted];
}

@end
