/// Copyright 2025 The Chromium Authors
/// Use of this source code is governed by a BSD-style license that can be
/// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_COORDINATOR_SAFARI_DATA_IMPORT_IMPORT_MEDIATOR_H_
#define IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_COORDINATOR_SAFARI_DATA_IMPORT_IMPORT_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import <memory>

#import "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#import "ios/chrome/browser/safari_data_import/ui/safari_data_import_password_conflict_mutator.h"

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
@protocol SafariDataImportImportStageTransitionHandler;
@protocol SafariDataItemConsumer;

/// Mediator for the safari data import screen. Handles stages of importing a
/// .zip file generated from Safari data to Chrome.
@interface SafariDataImportImportMediator
    : NSObject <SafariDataImportPasswordConflictMutator,
                UIDocumentPickerDelegate>

/// Transition handler for import stage. This needs to be set before selecting a
/// file.
@property(nonatomic, weak) id<SafariDataImportImportStageTransitionHandler>
    importStageTransitionHandler;

/// Consumer object displaying Safari item import status. This needs to be set
/// before selecting a file.
@property(nonatomic, weak) id<SafariDataItemConsumer> itemConsumer;

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
                      faviconLoader:(FaviconLoader*)faviconLoader
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

/// Resets the mediator to the state before any file is selected or processed.
- (void)reset;

/// Imports the items that are ready for import, and increments the import stage
/// . Should only be invoked when items are ready.
- (void)importItems;

/// List of password conflicts with the information retrieved from the source
/// of import. Only available when passwords are ready.
- (NSArray<PasswordImportItem*>*)conflictingPasswords;

/// List of passwords failed to be imported. Only available when passwords are
/// imported.
- (NSArray<PasswordImportItem*>*)invalidPasswords;

/// Disconnect mediator dependencies; needs to be invoked before deallocating
/// the coordinator.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_COORDINATOR_SAFARI_DATA_IMPORT_IMPORT_MEDIATOR_H_
