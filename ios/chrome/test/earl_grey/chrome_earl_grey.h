// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_EARL_GREY_CHROME_EARL_GREY_H_
#define IOS_CHROME_TEST_EARL_GREY_CHROME_EARL_GREY_H_

#import <UIKit/UIKit.h>

#include <string>

#include "base/compiler_specific.h"
#import "components/content_settings/core/common/content_settings.h"
#include "components/sync/base/model_type.h"
#import "ios/testing/earl_grey/base_eg_test_helper_impl.h"
#include "url/gurl.h"

@class ElementSelector;
@protocol GREYMatcher;

namespace chrome_test_util {

// TODO(crbug.com/788813): Evaluate if JS helpers can be consolidated.
// Execute |javascript| on current web state, and wait for either the completion
// of execution or timeout. If |out_error| is not nil, it is set to the
// error resulting from the execution, if one occurs. The return value is the
// result of the JavaScript execution. If the request is timed out, then nil is
// returned.
id ExecuteJavaScript(NSString* javascript, NSError* __autoreleasing* out_error);

}  // namespace chrome_test_util

#define ChromeEarlGrey \
  [ChromeEarlGreyImpl invokedFromFile:@"" __FILE__ lineNumber:__LINE__]

// Test methods that perform actions on Chrome. These methods may read or alter
// Chrome's internal state programmatically or via the UI, but in both cases
// will properly synchronize the UI for Earl Grey tests.
@interface ChromeEarlGreyImpl : BaseEGTestHelperImpl

#pragma mark - Device Utilities

// Simulate the user action to rotate the device to a certain orientation.
// TODO(crbug.com/1017265): Remove along EG1 support.
- (void)rotateDeviceToOrientation:(UIDeviceOrientation)deviceOrientation
                            error:(NSError**)error;

// Returns |YES| if the keyboard is on screen. |error| is only supported if the
// test is running in EG2.
// TODO(crbug.com/1017281): Remove along EG1 support.
- (BOOL)isKeyboardShownWithError:(NSError**)error;

// Returns YES if running on an iPad.
- (BOOL)isIPadIdiom;

// Returns YES if the main application window's rootViewController has a compact
// horizontal size class.
- (BOOL)isCompactWidth;

// Returns YES if the main application window's rootViewController has a compact
// vertical size class.
- (BOOL)isCompactHeight;

// Returns whether the toolbar is split between top and bottom toolbar or if it
// is displayed as only one toolbar.
- (BOOL)isSplitToolbarMode;

// Whether the the main application window's rootViewController has a regular
// vertical and regular horizontal size class.
- (BOOL)isRegularXRegularSizeClass;

#pragma mark - History Utilities (EG2)

// Clears browsing history. Raises an EarlGrey exception if history is not
// cleared within a timeout.
- (void)clearBrowsingHistory;

// Clears browsing cache. Raises an EarlGrey exception if history is not
// cleared within a timeout.
- (void)removeBrowsingCache;

#pragma mark - Navigation Utilities (EG2)

// Loads |URL| in the current WebState with transition type
// ui::PAGE_TRANSITION_TYPED, and if waitForCompletion is YES
// waits for the loading to complete within a timeout.
// Returns nil on success, or else an NSError indicating why the operation
// failed.
- (void)loadURL:(const GURL&)URL waitForCompletion:(BOOL)wait;

// Loads |URL| in the current WebState with transition type
// ui::PAGE_TRANSITION_TYPED, and waits for the loading to complete within a
// timeout.
// If the condition is not met within a timeout returns an NSError indicating
// why the operation failed, otherwise nil.
- (void)loadURL:(const GURL&)URL;

// Returns YES if the current WebState is loading.
- (BOOL)isLoading WARN_UNUSED_RESULT;

// Reloads the page and waits for the loading to complete within a timeout, or a
// GREYAssert is induced.
- (void)reload;

// Reloads the page. If |wait| is YES, waits for the loading to complete within
// a timeout, or a GREYAssert is induced.
- (void)reloadAndWaitForCompletion:(BOOL)wait;

// Navigates back to the previous page and waits for the loading to complete
// within a timeout, or a GREYAssert is induced.
- (void)goBack;

// Navigates forward to the next page and waits for the loading to complete
// within a timeout, or a GREYAssert is induced.
- (void)goForward;

// Waits for the page to finish loading within a timeout, or a GREYAssert is
// induced.
- (void)waitForPageToFinishLoading;

// Waits for the matcher to return an element that is sufficiently visible.
- (void)waitForSufficientlyVisibleElementWithMatcher:(id<GREYMatcher>)matcher;

// Waits for there to be |count| number of non-incognito tabs within a timeout,
// or a GREYAssert is induced.
- (void)waitForMainTabCount:(NSUInteger)count;

// Waits for there to be |count| number of incognito tabs within a timeout, or a
// GREYAssert is induced.
- (void)waitForIncognitoTabCount:(NSUInteger)count;

#pragma mark - Settings Utilities (EG2)

// Sets value for content setting.
- (void)setContentSettings:(ContentSetting)setting;

#pragma mark - Sync Utilities (EG2)

// Clears fake sync server data.
- (void)clearSyncServerData;

// Starts the sync server. The server should not be running when calling this.
- (void)startSync;

// Stops the sync server. The server should be running when calling this.
- (void)stopSync;

// Clears the autofill profile for the given |GUID|.
- (void)clearAutofillProfileWithGUID:(const std::string&)GUID;

// Injects an autofill profile into the fake sync server with |GUID| and
// |full_name|.
- (void)injectAutofillProfileOnFakeSyncServerWithGUID:(const std::string&)GUID
                                  autofillProfileName:
                                      (const std::string&)fullName;

// Returns YES if there is an autofilll profile with the corresponding |GUID|
// and |full_name|.
- (BOOL)isAutofillProfilePresentWithGUID:(const std::string&)GUID
                     autofillProfileName:(const std::string&)fullName
    WARN_UNUSED_RESULT;

// Sets up a fake sync server to be used by the ProfileSyncService.
- (void)setUpFakeSyncServer;

// Tears down the fake sync server used by the ProfileSyncService and restores
// the real one.
- (void)tearDownFakeSyncServer;

// Gets the number of entities of the given |type|.
- (int)numberOfSyncEntitiesWithType:(syncer::ModelType)type WARN_UNUSED_RESULT;

// Adds typed URL into HistoryService.
- (void)addHistoryServiceTypedURL:(const GURL&)URL;

// Deletes typed URL from HistoryService.
- (void)deleteHistoryServiceTypedURL:(const GURL&)URL;

// Injects a bookmark with |URL| and |title| into the fake sync server.
- (void)addFakeSyncServerBookmarkWithURL:(const GURL&)URL
                                   title:(const std::string&)title;

// Injects typed URL to sync FakeServer.
- (void)addFakeSyncServerTypedURL:(const GURL&)URL;

// Triggers a sync cycle for a |type|.
- (void)triggerSyncCycleForType:(syncer::ModelType)type;

// Deletes an autofill profile from the fake sync server with |GUID|, if it
// exists. If it doesn't exist, nothing is done.
- (void)deleteAutofillProfileOnFakeSyncServerWithGUID:(const std::string&)GUID;

// Verifies the sessions hierarchy on the Sync FakeServer. |URLs| is
// the collection of URLs that are to be expected for a single window. A
// GREYAssert is induced on failure. See the SessionsHierarchy class for
// documentation regarding the verification.
- (void)verifySyncServerURLs:(NSArray<NSString*>*)URLs;

// Waits until sync server contains |count| entities of the given |type| and
// |name|. Folders are not included in this count.
// If the condition is not met within a timeout a GREYAssert is induced.
- (void)waitForSyncServerEntitiesWithType:(syncer::ModelType)type
                                     name:(const std::string&)UTF8Name
                                    count:(size_t)count
                                  timeout:(NSTimeInterval)timeout;

// Induces a GREYAssert if |expected_present| is YES and the provided |url| is
// not present, or vice versa.
- (void)waitForTypedURL:(const GURL&)URL
          expectPresent:(BOOL)expectPresent
                timeout:(NSTimeInterval)timeout;

#pragma mark - Tab Utilities (EG2)

// Opens a new tab and waits for the new tab animation to complete within a
// timeout, or a GREYAssert is induced.
- (void)openNewTab;

// Closes the current tab and waits for the UI to complete.
- (void)closeCurrentTab;

// Opens a new incognito tab and waits for the new tab animation to complete.
- (void)openNewIncognitoTab;

// Closes all tabs in the current mode (incognito or normal), and waits for the
// UI to complete. If current mode is Incognito, mode will be switched to
// normal after closing all tabs.
- (void)closeAllTabsInCurrentMode;

// Closes all normal (non-incognito) tabs and waits for the UI to complete
// within a timeout, or a GREYAssert is induced.
- (void)closeAllNormalTabs;

// Closes all incognito tabs and waits for the UI to complete within a
// timeout, or a GREYAssert is induced.
- (void)closeAllIncognitoTabs;

// Closes all tabs in the all modes (incognito and main (non-incognito)), and
// does not wait for the UI to complete. If current mode is Incognito, mode will
// be switched to main (non-incognito) after closing the incognito tabs.
- (void)closeAllTabs;

// Selects tab with given index in current mode (incognito or main
// (non-incognito)).
- (void)selectTabAtIndex:(NSUInteger)index;

// Closes tab with the given index in current mode (incognito or main
// (non-incognito)).
- (void)closeTabAtIndex:(NSUInteger)index;

// Returns YES if the browser is in incognito mode, and NO otherwise.
- (BOOL)isIncognitoMode WARN_UNUSED_RESULT;

// Returns the number of main (non-incognito) tabs.
- (NSUInteger)mainTabCount WARN_UNUSED_RESULT;

// Returns the number of incognito tabs.
- (NSUInteger)incognitoTabCount WARN_UNUSED_RESULT;

// Simulates a backgrounding and raises an EarlGrey exception if simulation not
// succeeded.
- (void)simulateTabsBackgrounding;

// Persists the current list of tabs to disk immediately.
- (void)saveSessionImmediately;

// Returns the number of main (non-incognito) tabs currently evicted.
- (NSUInteger)evictedMainTabCount WARN_UNUSED_RESULT;

// Evicts the tabs associated with the non-current browser mode.
- (void)evictOtherTabModelTabs;

// Sets the normal tabs as 'cold start' tabs and raises an EarlGrey exception if
// operation not succeeded.
- (void)setCurrentTabsToBeColdStartTabs;

// Resets the tab usage recorder on current mode and raises an EarlGrey
// exception if operation not succeeded.
- (void)resetTabUsageRecorder;

// Returns the tab title of the current tab.
- (NSString*)currentTabTitle;

// Returns the tab title of the next tab. Assumes that next tab exists.
- (NSString*)nextTabTitle;

// Returns a unique identifier for the current Tab.
- (NSString*)currentTabID;

// Returns a unique identifier for the next Tab.
- (NSString*)nextTabID;

// Shows the tab switcher by tapping the switcher button.  Works on both phone
// and tablet.
- (void)showTabSwitcher;

#pragma mark - SignIn Utilities (EG2)

// Signs the user out, clears the known accounts entirely and checks whether the
// accounts were correctly removed from the keychain. Induces a GREYAssert if
// the operation fails.
- (void)signOutAndClearAccounts;

#pragma mark - Sync Utilities (EG2)

// Waits for sync to be initialized or not. If not succeeded a GREYAssert is
// induced.
- (void)waitForSyncInitialized:(BOOL)isInitialized
                   syncTimeout:(NSTimeInterval)timeout;

// Returns the current sync cache GUID. The sync server must be running when
// calling this.
- (std::string)syncCacheGUID;

#pragma mark - WebState Utilities (EG2)

// Taps html element with |elementID| in the current web state.
// A GREYAssert is induced on failure.
- (void)tapWebStateElementWithID:(NSString*)elementID;

// Attempts to tap the element with |element_id| within window.frames[0] of the
// current WebState using a JavaScript click() event. This only works on
// same-origin iframes.
// A GREYAssert is induced on failure.
- (void)tapWebStateElementInIFrameWithID:(const std::string&)elementID;

// Waits for the current web state to contain an element matching |selector|.
// If the condition is not met within a timeout a GREYAssert is induced.
- (void)waitForWebStateContainingElement:(ElementSelector*)selector;

// Attempts to submit form with |formID| in the current WebState.
// Induces a GREYAssert if the operation fails.
- (void)submitWebStateFormWithID:(const std::string&)formID;

// Waits for the current web state to contain |UTF8Text|. If the condition is
// not met within a timeout a GREYAssert is induced.
- (void)waitForWebStateContainingText:(const std::string&)UTF8Text;

// Waits for the current web state to contain |UTF8Text|. If the condition is
// not met within the given |timeout| a GREYAssert is induced.
- (void)waitForWebStateContainingText:(const std::string&)UTF8Text
                              timeout:(NSTimeInterval)timeout;

// Waits for there to be no web state containing |UTF8Text|.
// If the condition is not met within a timeout a GREYAssert is induced.
- (void)waitForWebStateNotContainingText:(const std::string&)UTF8Text;

// Waits for there to be a web state containing a blocked |imageID|.  When
// blocked, the image element will be smaller than the actual image size.
// If the condition is not met within a timeout a GREYAssert is induced.
- (void)waitForWebStateContainingBlockedImageElementWithID:
    (const std::string&)UTF8ImageID;

// Waits for there to be a web state containing loaded image with |imageID|.
// When loaded, the image element will have the same size as actual image.
// If the condition is not met within a timeout a GREYAssert is induced.
- (void)waitForWebStateContainingLoadedImageElementWithID:
    (const std::string&)UTF8ImageID;

// Returns the current web state's VisibleURL.
- (GURL)webStateVisibleURL;

// Purges cached web view pages, so the next time back navigation will not use
// a cached page. Browsers don't have to use a fresh version for back/forward
// navigation for HTTP pages and may serve a version from the cache even if the
// Cache-Control response header says otherwise.
- (void)purgeCachedWebViewPages;

// Simulators background, killing, and restoring the app within the limitations
// of EG1, by simply doing a tab grid close all / undo / done.
- (void)triggerRestoreViaTabGridRemoveAllUndo;

// Returns YES if the current WebState's web view uses the content inset to
// correctly align the top of the content with the bottom of the top bar.
- (BOOL)webStateWebViewUsesContentInset;

// Returns the size of the current WebState's web view.
- (CGSize)webStateWebViewSize;

#pragma mark - Bookmarks Utilities (EG2)

// Waits for the bookmark internal state to be done loading.
// If the condition is not met within a timeout a GREYAssert is induced.
- (void)waitForBookmarksToFinishLoading;

// Clears bookmarks if any bookmark still presents. A GREYAssert is induced if
// bookmarks can not be cleared.
- (void)clearBookmarks;

#pragma mark - URL Utilities (EG2)

// Returns the title string to be used for a page with |URL| if that page
// doesn't specify a title.
- (NSString*)displayTitleForURL:(const GURL&)URL;

#pragma mark - JavaScript Utilities (EG2)

// Executes JavaScript on current WebState, and waits for either the completion
// or timeout. If execution does not complete within a timeout a GREYAssert is
// induced.
- (id)executeJavaScript:(NSString*)javaScript;

#pragma mark - Cookie Utilities (EG2)

// Returns cookies as key value pairs, where key is a cookie name and value is a
// cookie value.
// A GREYAssert is induced if cookies can not be returned.
- (NSDictionary*)cookies;

#pragma mark - Accessibility Utilities (EG2)

// Verifies that all interactive elements on screen (or at least one of their
// descendants) are accessible.
// A GREYAssert is induced if not all elements are accessible.
- (void)verifyAccessibilityForCurrentScreen;

#pragma mark - Feature enables checkers (EG2)

// Returns YES if SlimNavigationManager feature is enabled.
- (BOOL)isSlimNavigationManagerEnabled WARN_UNUSED_RESULT;

// Returns YES if BlockNewTabPagePendingLoad feature is enabled.
- (BOOL)isBlockNewTabPagePendingLoadEnabled WARN_UNUSED_RESULT;

// Returns YES if NewOmniboxPopupLayout feature is enabled.
- (BOOL)isNewOmniboxPopupLayoutEnabled WARN_UNUSED_RESULT;

// Returns YES if UmaCellular feature is enabled.
- (BOOL)isUMACellularEnabled WARN_UNUSED_RESULT;

// Returns YES if UKM feature is enabled.
- (BOOL)isUKMEnabled WARN_UNUSED_RESULT;

// Returns YES if WebPaymentsModifiers feature is enabled.
- (BOOL)isWebPaymentsModifiersEnabled WARN_UNUSED_RESULT;

// Returns YES if SettingsAddPaymentMethod feature is enabled.
- (BOOL)isSettingsAddPaymentMethodEnabled WARN_UNUSED_RESULT;

// Returns YES if CreditCardScanner feature is enabled.
- (BOOL)isCreditCardScannerEnabled WARN_UNUSED_RESULT;

// Returns YES if AutofillEnableCompanyName feature is enabled.
- (BOOL)isAutofillCompanyNameEnabled WARN_UNUSED_RESULT;

// Returns YES if custom WebKit frameworks were properly loaded, rather than
// system frameworks. Always returns YES if the app was not requested to run
// with custom WebKit frameworks.
- (BOOL)isCustomWebKitLoadedIfRequested WARN_UNUSED_RESULT;

#pragma mark - Popup Blocking

// Gets the current value of the popup content setting preference for the
// original browser state.
- (ContentSetting)popupPrefValue;

// Sets the popup content setting preference to the given value for the original
// browser state.
- (void)setPopupPrefValue:(ContentSetting)value;

// The count of key commands registered with the currently active BVC.
- (NSInteger)registeredKeyCommandCount;

@end

// Helpers that only compile under EarlGrey 1 are included in this "EG1"
// category.
// TODO(crbug.com/922813): Update these helpers to compile under EG2 and move
// them into the main class declaration as they are converted.
@interface ChromeEarlGreyImpl (EG1)

@end

#endif  // IOS_CHROME_TEST_EARL_GREY_CHROME_EARL_GREY_H_
