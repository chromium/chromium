/// Copyright 2025 The Chromium Authors
/// Use of this source code is governed by a BSD-style license that can be
/// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_COORDINATOR_SAFARI_DATA_IMPORT_IMPORT_MEDIATOR_H_
#define IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_COORDINATOR_SAFARI_DATA_IMPORT_IMPORT_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import <memory>

#import "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#import "ios/chrome/browser/data_import/ui/data_import_credential_conflict_mutator.h"

namespace autofill {
class PaymentsDataManager;
}
namespace bookmarks {
class BookmarkModel;
}
class FaviconLoader;
namespace history {
class HistoryService;
}
namespace syncer {
class SyncService;
}  // namespace syncer
@class PasswordImportItem;
class ReadingListModel;
class PrefService;
@protocol DataImportImportStageTransitionHandler;
@protocol ImportDataItemConsumer;

/// Mediator for the safari data import screen. Handles stages of importing a
/// .zip file generated from Safari data to Chrome.
@interface SafariDataImportImportMediator
    : NSObject <DataImportCredentialConflictMutator, UIDocumentPickerDelegate>

/// Email address of the user. `nil` if not logged in.
@property(nonatomic, readonly) NSString* email;

/// Transition handler for import stage. This needs to be set before selecting a
/// file.
@property(nonatomic, weak) id<DataImportImportStageTransitionHandler>
    importStageTransitionHandler;

/// Consumer object displaying Safari item import status. This needs to be set
/// before selecting a file.
@property(nonatomic, weak) id<ImportDataItemConsumer> itemConsumer;

/// Initializer.
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
                      faviconLoader:(FaviconLoader*)faviconLoader
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

/// Resets the mediator to the state before any file is selected or processed.
- (void)reset;

/// Name of the ZIP file containing Safari data. `Nil` until the file is
/// selected.
- (NSString*)filename;

/// List of password conflicts with the information retrieved from the source
/// of import. Only available when passwords are ready.
- (NSArray<PasswordImportItem*>*)conflictingPasswords;

/// List of passwords failed to be imported. Only available when passwords are
/// imported.
- (NSArray<PasswordImportItem*>*)invalidPasswords;

/// Delete the imported ZIP file. Returns an error if deletion could not be
/// performed, otherwise return `nil`.
- (NSError*)deleteFile;

/// Disconnect mediator dependencies; needs to be invoked before deallocating
/// the coordinator.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_COORDINATOR_SAFARI_DATA_IMPORT_IMPORT_MEDIATOR_H_
