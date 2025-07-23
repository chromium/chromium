/// Copyright 2025 The Chromium Authors
/// Use of this source code is governed by a BSD-style license that can be
/// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_COORDINATOR_SAFARI_DATA_IMPORT_IMPORT_MEDIATOR_H_
#define IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_COORDINATOR_SAFARI_DATA_IMPORT_IMPORT_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import <memory>

#import "components/password_manager/core/browser/ui/saved_passwords_presenter.h"

namespace autofill {
class PaymentsDataManager;
}
namespace bookmarks {
class BookmarkModel;
}
namespace history {
class HistoryService;
}
class ReadingListModel;
@protocol SafariDataImportImportStageConsumer;
@protocol SafariDataItemConsumer;

/// Mediator for the safari data import screen. Handles stages of importing a
/// .zip file generated from Safari data to Chrome.
@interface SafariDataImportImportMediator : NSObject <UIDocumentPickerDelegate>

/// Consumer object handling import stage transitions. This needs to be set
/// before selecting a file.
@property(nonatomic, weak) id<SafariDataImportImportStageConsumer>
    importStageConsumer;

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
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

/// Disconnect mediator dependencies; needs to be invoked before deallocating
/// the coordinator.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_COORDINATOR_SAFARI_DATA_IMPORT_IMPORT_MEDIATOR_H_
