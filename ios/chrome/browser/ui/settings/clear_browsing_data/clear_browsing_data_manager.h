// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_CLEAR_BROWSING_DATA_MANAGER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_CLEAR_BROWSING_DATA_MANAGER_H_

#import "components/browsing_data/core/counters/browsing_data_counter.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"

class Browser;
class BrowsingDataRemover;
enum class BrowsingDataRemoveMask;

@class ActionSheetCoordinator;
@class BrowsingDataCounterWrapperProducer;
@protocol ClearBrowsingDataConsumer;
@protocol CollectionViewFooterLinkDelegate;

// Only used in this folder to offer the user to log out
extern const char kCBDSignOutOfChromeURL[];

// Clear Browsing Data Section Identifiers.
enum ClearBrowsingDataSectionIdentifier {
  // Section holding types of data that can be cleared.
  SectionIdentifierDataTypes = kSectionIdentifierEnumZero,
  // Section for informational footnote about user's Google Account data.
  SectionIdentifierGoogleAccount,
  // Section for footnote about synced data being cleared & site settings
  // remaining.
  SectionIdentifierSavedSiteData,
  // Section containing cell displaying time range to remove data.
  SectionIdentifierTimeRange,
};

// Clear Browsing Data Item Types.
enum ClearBrowsingDataItemType {
  // Item representing browsing history data.
  ItemTypeDataTypeBrowsingHistory = kItemTypeEnumZero,
  // Item representing cookies and site data.
  ItemTypeDataTypeCookiesSiteData,
  // Items representing cached data.
  ItemTypeDataTypeCache,
  // Items representing saved passwords.
  ItemTypeDataTypeSavedPasswords,
  // Items representing autofill data.
  ItemTypeDataTypeAutofill,
  // Footer noting account will not be signed out.
  ItemTypeFooterGoogleAccount,
  // Footer noting user will not be signed out of chrome and other forms of
  // browsing history will still be available.
  ItemTypeFooterGoogleAccountAndMyActivity,
  // Footer offering to sign out of Google account in Chrome.
  ItemTypeFooterSignoutOfGoogle,
  // Item showing time range to remove data and allowing user to edit time
  // range.
  ItemTypeTimeRange,
  // Footer noting user will not be signed out of chrome and how to delete
  // search history based on set default search engine (DSE).
  ItemTypeFooterGoogleAccountDSEBased,
};

// Manager that serves as the bulk of the logic for
// ClearBrowsingDataConsumer.
@interface ClearBrowsingDataManager : NSObject

// The manager's consumer.
@property(nonatomic, weak) id<ClearBrowsingDataConsumer> consumer;

// Default init method. `profile` can't be nil.
- (instancetype)initWithProfile:(ProfileIOS*)profile;

// Designated initializer to allow dependency injection (in tests).
- (instancetype)initWithProfile:(ProfileIOS*)profile
                   browsingDataRemover:(BrowsingDataRemover*)remover
    browsingDataCounterWrapperProducer:
        (BrowsingDataCounterWrapperProducer*)producer NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Call this method before actually using the data manager.
- (void)prepare;

// Call this method when data manager is not used anymore.
- (void)disconnect;

// Fills `model` with appropriate sections and items.
- (void)loadModel:(ListModel*)model;

// Update the footer depending on whether the user signed in or out.
- (void)updateModel:(ListModel*)model withTableView:(UITableView*)tableView;

// Restarts browsing data counters, which in turn updates UI, with those data
// types specified by `mask`.
- (void)restartCounters:(BrowsingDataRemoveMask)mask;

// Returns a ActionSheetCoordinator that has action block to clear data of type
// `dataTypeMaskToRemove`.
- (ActionSheetCoordinator*)
    actionSheetCoordinatorWithDataTypesToRemove:
        (BrowsingDataRemoveMask)dataTypeMaskToRemove
                             baseViewController:
                                 (UIViewController*)baseViewController
                                        browser:(Browser*)browser
                            sourceBarButtonItem:
                                (UIBarButtonItem*)sourceBarButtonItem;

// Get the text to be displayed by a counter from the given `result`
- (NSString*)counterTextFromResult:
    (const browsing_data::BrowsingDataCounter::Result&)result;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_CLEAR_BROWSING_DATA_MANAGER_H_
