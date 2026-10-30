// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_EARL_GREY_CHROME_EARL_GREY_APP_INTERFACE_H_
#define IOS_CHROME_TEST_EARL_GREY_CHROME_EARL_GREY_APP_INTERFACE_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"
#import "base/time/time.h"
#import "components/content_settings/core/common/content_settings.h"
#import "components/sync/base/data_type.h"
#import "third_party/metrics_proto/user_demographics.pb.h"

@class ElementSelector;
@protocol GREYMatcher;
enum class TipsNotificationType;

@interface JavaScriptExecutionResult : NSObject
@property(readonly, nonatomic) BOOL success;
@property(readonly, nonatomic) NSString* result;

// Initializes with execution `result` and `outcome`. Returns initialized
// instance.
- (instancetype)initWithResult:(NSString*)result
           successfulExecution:(BOOL)outcome;

- (instancetype)init NS_UNAVAILABLE;

@end

// ChromeEarlGreyAppInterface contains the app-side implementation for helpers
// that primarily work via direct model access. These helpers are compiled into
// the app binary and can be called from either app or test code.
@interface ChromeEarlGreyAppInterface : NSObject

// YES if the current interface language uses RTL layout.
+ (BOOL)isRTL;

// Stops startup tracing, flushes trace file to disk, and returns trace file
// path.
+ (NSString*)stopStartupTracing;

// Starts manual tracing. Returns YES on success.
+ (BOOL)startManualTracing;

// Starts manual tracing with `categoryFilter` (e.g., @"ui"). Returns YES on
// success.
+ (BOOL)startManualTracingWithCategoryFilter:(NSString*)categoryFilter;

// Stops manual tracing, flushes trace file to disk, and returns trace file
// path.
+ (NSString*)stopManualTracing;

// Returns YES if the trace file at `filePath` exists and has a size greater
// than 0.
+ (BOOL)isTraceFileReadyAtPath:(NSString*)filePath;

// Returns the content of the trace file at `filePath` as NSData, or nil if
// reading failed.
+ (NSData*)traceDataForPath:(NSString*)filePath;

// Removes the trace file at `filePath`. Returns YES if successfully deleted.
+ (BOOL)removeTraceFileAtPath:(NSString*)filePath;

// Returns YES if the application window is in windowed mode (multitasking).
+ (BOOL)isWindowedMode;

// Clears browsing history and waits for history to finish clearing before
// returning. Returns nil on success, or else an NSError indicating why the
// operation failed.
+ (NSError*)clearBrowsingHistory;

// Shuts down the network process in order to
// avoid tests from hanging when clearing browser history. Uses a private WebKit
// API and should be refactored or removed in the event that there's a different
// way to address hanging.
+ (void)killWebKitNetworkProcess;

// Clears all web state browsing data and waits to finish clearing before
// returning. Returns nil on success, otherwise an NSError indicating why
// the operation failed.
+ (NSError*)clearAllWebStateBrowsingData;

// Returns count of browsing history entries in the current profile.
// Sets `error` if reading history failed.
+ (NSInteger)browsingHistoryEntryCountWithError:
    (NSError* __autoreleasing*)error;

// Gets the number of items in the back list. Returns -1 in case of error.
+ (NSInteger)navigationBackListItemsCount;

// Clears browsing cache. Returns nil on success, or else an NSError indicating
// the operation failed.
+ (NSError*)removeBrowsingCache;

// Persists the current list of tabs to disk immediately.
+ (void)saveSessionImmediately;

// Opens a scene with URL `spec`.
+ (void)sceneOpenURL:(NSString*)spec;

// Continues user activity in active scene with `activityType` and `urlString`.
+ (void)sceneContinueUserActivityWithType:(NSString*)activityType
                                      url:(NSString*)urlString;

// Loads the URL `spec` in the current WebState with transition type
// ui::PAGE_TRANSITION_TYPED and returns without waiting for the page to load.
+ (void)startLoadingURL:(NSString*)spec;

// Returns YES if the current WebState is loading.
+ (BOOL)isLoading;

// Returns YES if any WebState across all foreground tabs is loading.
+ (BOOL)isAnyWebStateLoading;

// Starts polling the pending wait condition prepared by a preceding
// `prepareConditionFor...` helper that returned YES. Invokes `completion`
// once satisfied. Must only be called after a `prepareConditionFor...`
// helper returns YES.
+ (void)startPendingWaitWithCompletion:(ProceduralBlock)completion;

// Cancels any pending asynchronous in-app wait timer.
+ (void)cancelPendingWait;

// Prepares an in-app wait for the UI element matching `matcher` (scoped to
// `rootMatcher` if non-nil) to satisfy `assertionMatcher`. Returns YES if a
// wait was needed and prepared for `startPendingWaitWithCompletion:`, or NO
// immediately if already satisfied.
+ (BOOL)prepareConditionForUIElementWithMatcher:(id<GREYMatcher>)matcher
                                    rootMatcher:(id<GREYMatcher>)rootMatcher
                               assertionMatcher:
                                   (id<GREYMatcher>)assertionMatcher;

// Prepares an in-app wait for the current WebState to finish loading. Returns
// YES if a wait was needed and prepared for `startPendingWaitWithCompletion:`,
// or NO immediately if already not loading.
+ (BOOL)prepareConditionForPageToFinishLoading;

// Prepares an in-app wait for all foreground WebStates to finish loading.
// Returns YES if a wait was needed and prepared for
// `startPendingWaitWithCompletion:`, or NO immediately if none are loading.
+ (BOOL)prepareConditionForAllWebStatesToFinishLoading;

// Reloads the page without waiting for the page to load.
+ (void)startReloading;

// Programmatically dismisses settings screen.
+ (void)dismissSettings;

// Stops primes performance metrics logging by calling into the
// internal framework (should only be used by performance tests)
+ (void)primesStopLogging;

// Takes a memory snapshot with Primes using `eventName`.
+ (void)primesTakeMemorySnapshot:(NSString*)eventName;

#pragma mark - Profile Utilities (EG2)

// Returns the name (as in `ProfileIOS::GetProfileName()`) of the current
// profile, more precisely the profile associated with the foreground active
// scene.
+ (NSString*)currentProfileName;

// Returns the name (as in `ProfileIOS::GetProfileName()`) of the personal
// profile (as opposed to managed profiles), as per
// `ProfileAttributesStorageIOS::GetPersonalProfileName()`.
+ (NSString*)personalProfileName;

// Prepares an in-app wait for `currentProfileName` to equal `profileName`.
// Returns YES if a wait was needed and prepared for
// `startPendingWaitWithCompletion:`, or NO immediately if already equal.
+ (BOOL)prepareConditionForCurrentProfileName:(NSString*)profileName;

#pragma mark - Tab Utilities (EG2)

// Selects tab at `index`.
+ (void)selectTabAtIndex:(NSUInteger)index;

// Closes tab at `index`.
+ (void)closeTabAtIndex:(NSUInteger)index;

// Returns YES if the browser is in incognito mode, and NO otherwise.
+ (BOOL)isIncognitoMode [[nodiscard]];

// Returns the number of open non-incognito tabs.
+ (NSUInteger)mainTabCount [[nodiscard]];

// Returns the number of open inactive tabs.
+ (NSUInteger)inactiveTabCount [[nodiscard]];

// Returns the number of open incognito tabs.
+ (NSUInteger)incognitoTabCount [[nodiscard]];

// Returns the number of the realized web states from the existing web states.
+ (NSInteger)realizedWebStatesCount [[nodiscard]];

// Simulates a backgrounding.
// If not succeed returns an NSError indicating  why the
// operation failed, otherwise nil.
+ (NSError*)simulateTabsBackgrounding;

// Returns the number of main (non-incognito) tabs currently evicted.
+ (NSUInteger)evictedMainTabCount [[nodiscard]];

// Evicts the tabs associated with the non-current browser mode.
+ (void)evictOtherBrowserTabs;

// Sets the normal tabs as 'cold start' tabs
// If not succeed returns an NSError indicating  why the
// operation failed, otherwise nil.
+ (NSError*)setCurrentTabsToBeColdStartTabs;

// Resets the tab usage recorder on current mode.
// If not succeed returns an NSError indicating  why the
// operation failed, otherwise nil.
+ (NSError*)resetTabUsageRecorder;

// Opens a new tab, and does not wait for animations to complete.
+ (void)openNewTab;

// Opens a new tab with `url` and `textFragment`.
+ (void)openNewTabWithURL:(NSString*)url textFragment:(NSString*)textFragment;

// Opens Send Tab to Self new tab with `url`, `textFragment`, and `guid`.
+ (void)openSendTabToSelfNewTabWithURL:(NSString*)url
                          textFragment:(NSString*)textFragment
                             entryGUID:(NSString*)guid;

// Opens Send Tab to Self new background tab with `url`, `textFragment`, and
// `guid`.
+ (void)openSendTabToSelfNewBackgroundTabWithURL:(NSString*)url
                                    textFragment:(NSString*)textFragment
                                       entryGUID:(NSString*)guid;

// Simulates opening a custom `URL` from another application.
+ (void)simulateExternalAppURLOpeningWithURL:(NSURL*)URL;

// Closes current tab.
+ (void)closeCurrentTab;

// Pins current tab.
+ (void)pinCurrentTab;

// Opens a new incognito tab, and does not wait for animations to complete.
+ (void)openNewIncognitoTab;

// Closes all tabs in the current mode (incognito or normal), and does not wait
// for the UI to complete. If current mode is Incognito, mode will be switched
// normal after closing all tabs.
+ (void)closeAllTabsInCurrentMode;

// Closes all normal (non-incognito) tabs. If not succeed returns an NSError
// indicating why the operation failed, otherwise nil.
+ (NSError*)closeAllNormalTabs;

// Closes all incognito tabs. If not succeed returns an NSError indicating  why
// the operation failed, otherwise nil.
+ (NSError*)closeAllIncognitoTabs;

// Closes all tabs in all modes (incognito and main (non-incognito)) and does
// not wait for the UI to complete. If current mode is Incognito, mode will be
// switched to main (non-incognito) after closing the incognito tabs.
+ (void)closeAllTabs;

// Navigates back to the previous page without waiting for the page to load.
+ (void)startGoingBack;

// Navigates forward to the next page without waiting for the page to load.
+ (void)startGoingForward;

// Returns the title of the current selected tab.
+ (NSString*)currentTabTitle;

// Returns the title of the next tab. Assumes that there is a next tab.
+ (NSString*)nextTabTitle;

// Returns a unique identifier for the current Tab.
+ (NSString*)currentTabID;

// Returns a unique identifier for the next Tab.
+ (NSString*)nextTabID;

// Returns the index of active tab in normal mode.
+ (NSUInteger)indexOfActiveNormalTab;

// Returns YES if the current active WebState is showing a new tab page.
+ (BOOL)isCurrentTabNTP;

// Prepares an in-app wait for `currentTabTitle` to equal `tabTitle`. Returns
// YES if a wait was needed and prepared for `startPendingWaitWithCompletion:`,
// or NO immediately if already equal.
+ (BOOL)prepareConditionForCurrentTabTitle:(NSString*)tabTitle;

// Prepares an in-app wait for `mainTabCount` to equal `count`. Returns YES if a
// wait was needed and prepared for `startPendingWaitWithCompletion:`, or NO
// immediately if already equal.
+ (BOOL)prepareConditionForMainTabCount:(NSUInteger)count;

// Prepares an in-app wait for `inactiveTabCount` to equal `count`. Returns YES
// if a wait was needed and prepared for `startPendingWaitWithCompletion:`, or
// NO immediately if already equal.
+ (BOOL)prepareConditionForInactiveTabCount:(NSUInteger)count;

// Prepares an in-app wait for `incognitoTabCount` to equal `count`. Returns YES
// if a wait was needed and prepared for `startPendingWaitWithCompletion:`, or
// NO immediately if already equal.
+ (BOOL)prepareConditionForIncognitoTabCount:(NSUInteger)count;

#pragma mark - Window utilities (EG2)

// Returns screen position of the given `windowNumber`
+ (CGRect)screenPositionOfScreenWithNumber:(int)windowNumber;

// Returns the number of windows, including background and disconnected or
// archived windows.
+ (NSUInteger)windowCount [[nodiscard]];

// Returns the number of foreground (visible on screen) windows.
+ (NSUInteger)foregroundWindowCount [[nodiscard]];

// Closes all but one window, including all non-foreground windows.
+ (void)closeAllExtraWindows;

// Open a new window. Returns an error if multiwindow is not supported.
+ (NSError*)openNewWindow;

// Opens a new tab in window with number `windowNumber`.
+ (void)openNewTabInWindowWithNumber:(int)windowNumber;

// Closes window with number `windowNumber`.
+ (void)closeWindowWithNumber:(int)windowNumber;

// Changes window with number `windowNumber` to `newWindowNumber`.
+ (void)changeWindowWithNumber:(int)windowNumber
                   toNewNumber:(int)newWindowNumber;

// Loads the URL `spec` in the current WebState in window `windowNumber` with
// transition type ui::PAGE_TRANSITION_TYPED and returns without waiting for the
// page to load.
+ (void)startLoadingURL:(NSString*)spec inWindowWithNumber:(int)windowNumber;

// Returns YES if current WebState is loading in window `windowNumber`.
+ (BOOL)isLoadingInWindowWithNumber:(int)windowNumber [[nodiscard]];

// Returns YES if the current WebState in window `windowNumber` contains `text`.
+ (BOOL)webStateContainsText:(NSString*)text
          inWindowWithNumber:(int)windowNumber;

// Returns count of non-incognito tabs in window with number `windowNumber`.
+ (NSUInteger)mainTabCountInWindowWithNumber:(int)windowNumber;

// Returns count of incognito tabs in window with number `windowNumber`.
+ (NSUInteger)incognitoTabCountInWindowWithNumber:(int)windowNumber;

// Prepares an in-app wait for `windowCount` to equal `count`. Returns YES if a
// wait was needed and prepared for `startPendingWaitWithCompletion:`, or NO
// immediately if already equal.
+ (BOOL)prepareConditionForWindowCount:(NSUInteger)count;

// Prepares an in-app wait for `foregroundWindowCount` to equal `count`. Returns
// YES if a wait was needed and prepared for `startPendingWaitWithCompletion:`,
// or NO immediately if already equal.
+ (BOOL)prepareConditionForForegroundWindowCount:(NSUInteger)count;

// Prepares an in-app wait for page in window `windowNumber` to finish loading.
// Returns YES if a wait was needed, or NO immediately if already satisfied.
+ (BOOL)prepareConditionForPageToFinishLoadingInWindowWithNumber:
    (int)windowNumber;

// Prepares an in-app wait for main tab `count` in window `windowNumber`.
// Returns YES if a wait was needed, or NO immediately if already satisfied.
+ (BOOL)prepareConditionForMainTabCount:(NSUInteger)count
                     inWindowWithNumber:(int)windowNumber;

// Prepares an in-app wait for incognito tab `count` in window `windowNumber`.
// Returns YES if a wait was needed, or NO immediately if already satisfied.
+ (BOOL)prepareConditionForIncognitoTabCount:(NSUInteger)count
                          inWindowWithNumber:(int)windowNumber;

// Returns a key window from the connected scenes.
+ (UIWindow*)keyWindow;

// Opens settings in window with number `windowNumber`.
+ (void)openSettingsInWindowWithNumber:(int)windowNumber;

// Shows tab switcher in window with number `windowNumber`.
+ (void)showTabSwitcherInWindowWithNumber:(int)windowNumber;

// Returns the interface orientation of the scene.
+ (UIInterfaceOrientation)interfaceOrientation;

// Returns YES if running on an iPhone Duo simulator.
+ (BOOL)isDuoSimulator;

// Dispatches simulated Duo hinge angle `angleInDegrees`. Returns YES on
// success.
+ (BOOL)dispatchSimulatedDuoHingeAngle:(double)angleInDegrees;

// Returns YES if the window and hinge posture match `angleInDegrees`.
+ (BOOL)isSimulatedDuoHingePostureSettled:(double)angleInDegrees;

// Dispatches simulated Duo orientation `orientation`. Returns YES on success.
+ (BOOL)dispatchSimulatedDuoOrientation:(UIDeviceOrientation)orientation;

// Returns YES if the scene and window orientation match `orientation`.
+ (BOOL)isSimulatedDuoOrientationSettled:(UIDeviceOrientation)orientation;

#pragma mark - WebState Utilities (EG2)

// Taps element with `elementID` in iFrame. Returns nil on success.
+ (NSError*)tapWebStateElementInIFrameWithID:(NSString*)elementID;

// Taps html element with `elementID` in the current web state.
// If not succeed returns an NSError indicating why the
// operation failed, otherwise nil.
+ (NSError*)tapWebStateElementWithID:(NSString*)elementID;

// Returns YES if the current WebState's frames contain `text`.
+ (BOOL)webStateContainsTextInIFrame:(NSString*)text;

// Attempts to submit form with `formID` in the current WebState.
// Returns nil on success, or else an NSError indicating why the operation
// failed.
+ (NSError*)submitWebStateFormWithID:(NSString*)formID;

// Returns YES if the current WebState contains an element matching `selector`.
+ (BOOL)webStateContainsElement:(ElementSelector*)selector;

// Returns YES if the current WebState contains `text`.
+ (BOOL)webStateContainsText:(NSString*)text;

// Waits for the current WebState to contain loaded image with `imageID`.
// Returns YES if the current WebState contains a loaded image with `imageID`.
+ (BOOL)webStateContainsLoadedImage:(NSString*)imageID;

// Returns YES if the current WebState contains a blocked image with `imageID`.
+ (BOOL)webStateContainsBlockedImage:(NSString*)imageID;

// Waits for the web state's scroll view zoom scale to be suitably close (within
// 0.05) of the expected scale. Returns nil if the condition is met within a
// timeout, or else an NSError indicating why the operation failed.
// Returns YES if the current WebState's zoom scale is close to `scale`.
+ (BOOL)webStateZoomScaleCloseTo:(CGFloat)scale;

// Prepares an in-app wait for WebState zoom scale to equal `scale`.
// Returns YES if a wait was needed, or NO immediately if already satisfied.
+ (BOOL)prepareConditionForWebStateZoomScale:(CGFloat)scale;

// Prepares an in-app wait for the current WebState's visible URL to equal
// `spec`. Returns YES if a wait was needed and prepared for
// `startPendingWaitWithCompletion:`, or NO immediately if already equal.
+ (BOOL)prepareConditionForWebStateVisibleURL:(NSString*)spec;

// Signs out and clears identities, calling `completion` block when finished.
+ (void)signOutAndClearIdentitiesWithCompletion:(ProceduralBlock)completion;

// Returns YES if there is at at least identity in the ChromeIdentityService.
+ (BOOL)hasIdentities;

// Returns the current WebState's VisibleURL.
+ (NSString*)webStateVisibleURL;

// Returns the current WebState's last committed URL.
+ (NSString*)webStateLastCommittedURL;

// Purges cached web view pages. Returns nil on success, or NSError on failure.
+ (NSError*)purgeCachedWebViewPages;

// Stops any pending navigations in all WebStates which are loading.
+ (void)stopAllWebStatesLoading;

#pragma mark - URL Utilities (EG2)

// Returns the title string to be used for a page with `URL` if that page
// doesn't specify a title.
+ (NSString*)displayTitleForURL:(NSString*)URL;

#pragma mark - Sync Utilities (EG2)

// Waits up to `timeout` for sync transport state to become active.
// Returns nil on success, or NSError on timeout.
+ (NSError*)waitForSyncTransportStateActiveWithTimeout:(base::TimeDelta)timeout;

// Returns the current sync cache GUID. The sync server must be running when
// calling this.
+ (NSString*)syncCacheGUID;

// Waits for sync invalidation fields to be received. Returns nil on success.
+ (NSError*)waitForSyncInvalidationFields;

// Whether or not the fake sync server has been setup.
+ (BOOL)isFakeSyncServerSetUp;

// Sets up a fake sync server to be used by the SyncServiceImpl.
+ (void)setUpFakeSyncServer;

// Tears down the fake sync server used by the SyncServiceImpl and restores the
// real one.
+ (void)tearDownFakeSyncServer;

// Clears fake sync server data if the server is running.
+ (void)clearFakeSyncServerData;

// Ensures that all of the FakeServer's data is persisted to disk. This is
// useful before app restarts, where otherwise the FakeServer may not get to do
// its usual on-destruction flush.
+ (void)flushFakeSyncServerToDisk;

// Gets the number of entities of the given `type`.
+ (int)numberOfSyncEntitiesWithType:(syncer::DataType)type;

// Forces every request to fail in a way that simulates a network failure.
+ (void)disconnectFakeSyncServerNetwork;

// Undoes the effects of disconnectFakeSyncServerNetwork.
+ (void)connectFakeSyncServerNetwork;

// Injects a bookmark into the fake sync server with `URL` and `title`.
+ (void)addFakeSyncServerBookmarkWithURL:(NSString*)URL title:(NSString*)title;

// Injects a legacy bookmark to sync FakeServer with `URL`, `title`, and
// `originator_client_item_id`.
+ (void)addFakeSyncServerLegacyBookmarkWithURL:(NSString*)URL
                                         title:(NSString*)title
                     originator_client_item_id:
                         (NSString*)originator_client_item_id;

// Injects a HISTORY visit for `URL` to the sync FakeServer.
+ (void)addFakeSyncServerHistoryVisit:(NSURL*)URL;

// Injects device info with `deviceName` and `lastUpdatedTimestamp` to sync
// FakeServer.
+ (void)addFakeSyncServerDeviceInfo:(NSString*)deviceName
               lastUpdatedTimestamp:(base::Time)lastUpdatedTimestamp;

// Injects Send Tab To Self entry with `URL`, `title`, `deviceName`, and
// `targetDeviceGUID` to sync FakeServer.
+ (void)addFakeSyncServerSendTabToSelfEntryWithURL:(NSString*)URL
                                             title:(NSString*)title
                                        deviceName:(NSString*)deviceName
                                  targetDeviceGUID:(NSString*)targetDeviceGUID;

// Adds fake Send Tab To Self entry with `url`, `title`, and `formFieldData`
// to fake sync server. Returns its GUID.
+ (NSString*)addFakeSendTabToSelfEntryWithURL:(NSString*)url
                                        title:(NSString*)title
                                formFieldData:
                                    (NSDictionary<NSString*, NSString*>*)
                                        formFieldData;

// Adds fake Send Tab To Self entry with `url`, `title`, and `textFragment`
// to fake sync server. Returns its GUID.
+ (NSString*)addFakeSendTabToSelfEntryWithURL:(NSString*)url
                                        title:(NSString*)title
                                 textFragment:(NSString*)textFragment;

// Returns YES if local Send Tab To Self model contains entry with `guid`.
+ (BOOL)hasSendTabToSelfEntryWithGUID:(NSString*)guid;

// Returns YES if local Send Tab To Self model contains target `deviceName`.
+ (BOOL)hasSendTabToSelfTargetDevice:(NSString*)deviceName;

// Prepares an in-app wait for entry with `guid` to be present.
// Returns YES if a wait was needed, or NO immediately if already present.
+ (BOOL)prepareConditionForSendTabToSelfEntryWithGUID:(NSString*)guid;

// Prepares an in-app wait for target device `deviceName` to be present.
// Returns YES if a wait was needed, or NO immediately if already present.
+ (BOOL)prepareConditionForSendTabToSelfTargetDevice:(NSString*)deviceName;

// Returns generated text fragment for `URL`, or nil if no entry exists.
+ (NSString*)textFragmentForSendTabToSelfEntryWithURL:(NSString*)URL;

// Adds typed `URL` into HistoryService.
+ (void)addHistoryServiceTypedURL:(NSString*)URL;

// Adds typed `URL` into HistoryService at timestamp `visitTimestamp`.
+ (void)addHistoryServiceTypedURL:(NSString*)URL
                   visitTimestamp:(base::Time)visitTimestamp;

// Sets the page `title` for `URL` in the History Service.
+ (void)setHistoryServiceTitle:(NSString*)title forPage:(NSString*)URL;

// Deletes typed `URL` from HistoryService.
+ (void)deleteHistoryServiceTypedURL:(NSString*)URL;

// If the provided URL `spec` is either present or not present in HistoryService
// (depending on `expectPresent`), return YES. If the present status of `spec`
// is not what is expected, or there is an error, return NO.
+ (BOOL)isURL:(NSString*)spec presentOnClient:(BOOL)expectPresent;

// Injects user demographics into the fake sync server. `rawBirthYear` is the
// true birth year, pre-noise, and `gender` corresponds to the proto enum
// UserDemographicsProto::Gender.
+ (void)
    addUserDemographicsToSyncServerWithBirthYear:(int)rawBirthYear
                                          gender:
                                              (metrics::UserDemographicsProto::
                                                   Gender)gender;

// Clears the autofill profile for the given `GUID`.
+ (void)clearAutofillProfileWithGUID:(NSString*)GUID;

// Injects autofill profile into fake sync server with `GUID` and `fullName`.
+ (void)addAutofillProfileToFakeSyncServerWithGUID:(NSString*)GUID
                               autofillProfileName:(NSString*)fullName;

// Returns YES if there is an autofill profile with `GUID` and `fullName`.
+ (BOOL)isAutofillProfilePresentWithGUID:(NSString*)GUID
                     autofillProfileName:(NSString*)fullName;

// Verifies the sessions hierarchy on the Sync FakeServer. `specs` is
// the collection of URLs that are to be expected for a single window. On
// failure, returns a NSError describing the failure. See the
// SessionsHierarchy class for documentation regarding the verification.
+ (NSError*)verifySessionsOnSyncServerWithSpecs:(NSArray<NSString*>*)specs;

// Verifies expected `URLs` on Sync FakeServer. Returns nil on success.
+ (NSError*)verifyHistoryOnSyncServerWithURLs:(NSArray<NSURL*>*)URLs;

// Verifies that `count` entities of the given `type` and `name` exist on the
// sync FakeServer. Folders are not included in this count. Returns NSError
// if there is a failure or if the count does not match.
+ (NSError*)verifyNumberOfSyncEntitiesWithType:(NSUInteger)type
                                          name:(NSString*)name
                                         count:(NSUInteger)count;

// Prepares in-app wait for sync entities with `type`, `name`, and `count`.
// Returns YES if a wait was needed, or NO immediately if already satisfied.
+ (BOOL)prepareConditionForSyncServerEntitiesWithType:(NSUInteger)type
                                                 name:(NSString*)name
                                                count:(NSUInteger)count;

// Adds bookmark requiring `syncPassphrase` to sync.
+ (void)addBookmarkWithSyncPassphrase:(NSString*)syncPassphrase;

// Adds `syncPassphrase` requirement to start sync server.
+ (void)addSyncPassphrase:(NSString*)syncPassphrase;

// Returns whether UserSelectableType::kHistory is among the selected types.
+ (BOOL)isSyncHistoryDataTypeSelected;

#pragma mark - JavaScript Utilities (EG2)

// Executes `javaScript` through the WebState WebFrame and returns execution
// result.
+ (JavaScriptExecutionResult*)executeJavaScript:(NSString*)javaScript;

// Returns the user agent that should be used for the mobile version.
+ (NSString*)mobileUserAgentString;

#pragma mark - Accessibility Utilities (EG2)

// Verifies that interactive elements on screen are accessible. Returns nil
// on success or an NSError describing accessibility issues.
+ (NSError*)verifyAccessibilityForCurrentScreen [[nodiscard]];

#pragma mark - Check features (EG2)

// Helpers for checking feature state. These can't use FeatureList directly when
// invoked from test code, as the EG test code runs in a separate process and
// must query Chrome for the state.

// Returns YES if `variationID` is enabled.
+ (BOOL)isVariationEnabled:(int)variationID;

// Returns YES if `variationID` triggering server-side behavior is enabled.
+ (BOOL)isTriggerVariationEnabled:(int)variationID;

// Returns YES if UKM feature is enabled.
+ (BOOL)isUKMEnabled [[nodiscard]];

// Returns YES if kTestFeature is enabled.
+ (BOOL)isTestFeatureEnabled;

// Returns YES if kOverflowMenuHomeCustomizationEntrypoint is enabled.
+ (BOOL)isOverflowMenuHomeCustomizationEntrypointEnabled;

// Returns YES if Fullscreen smooth scrolling is supported.
+ (BOOL)isFullscreenSmoothScrollingSupported;

// Returns YES if DemographicMetricsReporting feature is enabled.
+ (BOOL)isDemographicMetricsReportingEnabled [[nodiscard]];

// Returns YES if ProactiveSuggestionsFramework is enabled.
+ (BOOL)isProactiveSuggestionsFrameworkEnabled [[nodiscard]];

// Returns YES if the `launchSwitch` is found in host app launch switches.
+ (BOOL)appHasLaunchSwitch:(NSString*)launchSwitch;

// Returns YES if custom WebKit frameworks were properly loaded, rather than
// system frameworks. Always returns YES if the app was not requested to run
// with custom WebKit frameworks.
+ (BOOL)isCustomWebKitLoadedIfRequested [[nodiscard]];

// Returns whether the mobile version of the websites are requested by default.
+ (BOOL)isMobileModeByDefault [[nodiscard]];

// Returns whether the app is configured to, and running in an environment which
// can, open multiple windows.
+ (BOOL)areMultipleWindowsSupported;

// Returns whether the UseLensToSearchForImage feature is enabled.
+ (BOOL)isUseLensToSearchForImageEnabled;

// Returns whether the YourSavedInfoSettingsPageIos feature is enabled.
+ (BOOL)isYourSavedInfoSettingsPageIosEnabled;

// Returns whether the current layout is showing the bottom omnibox.
+ (BOOL)isCurrentLayoutBottomOmnibox;

// Returns whether chrome next is enabled.
+ (BOOL)isChromeNextEnabled;

// Returns whether next old design is enabled.
+ (BOOL)isNextOldDesignEnabled;

// Returns whether overflow menu refactoring on the NTP is enabled.
+ (BOOL)isOverflowMenuNTPRefactorEnabled;

// Returns whether the Pinned Tabs feature is enabled.
+ (BOOL)isPinnedTabsEnabled;

// Returns whether the aim cobrowse web selection search is enabled.
+ (BOOL)isAimCobrowseWebSelectionSearchEnabled;

#pragma mark - ContentSettings

// Returns the current popup ContentSetting preference value for original
// profile.
+ (ContentSetting)popupPrefValue;

// Sets popup ContentSetting preference `value` for original profile.
+ (void)setPopupPrefValue:(ContentSetting)value;

// Resets the desktop content setting to its default value.
+ (void)resetDesktopContentSetting;

// Sets ContentSetting `setting` for ContentSettingsType `type` in original
// profile.
+ (void)setContentSetting:(ContentSetting)setting
    forContentSettingsType:(ContentSettingsType)type;

#pragma mark - Default Utilities (EG2)

// Stores `value` for the provided `defaultName` in `NSUserDefaults` and
// synchronizes it immediately.
+ (void)setUserDefaultsObject:(id)value forKey:(NSString*)defaultName;

// Removes the object for the provided `key` in `NSUserDefaults` and
// synchronizes changes immediately.
+ (void)removeUserDefaultsObjectForKey:(NSString*)key;

// Returns the value for `key` from NSUserDefaults.
+ (id)userDefaultsObjectForKey:(NSString*)key;

// Creates AppGroupCommand based on `text` and writes to shared NSUserDefaults.
+ (void)setAppGroupCommandToSearchText:(NSString*)text;

// Creates incognito AppGroupCommand with `text` and writes to shared
// NSUserDefaults.
+ (void)setAppGroupCommandToIncognitoSearchText:(NSString*)text;

#pragma mark - Pref Utilities (EG2)

// Returns the value of local state pref `prefName` as a JSON string.
+ (NSString*)localStatePrefValue:(NSString*)prefName;

// Sets the integer value for the local state pref with `prefName`. `value`
// can be either a casted enum or any other numerical value. Local State
// contains the preferences that are shared between all profiles.
+ (void)setIntegerValue:(int)value forLocalStatePref:(NSString*)prefName;

// Sets time `value` for local state pref `prefName`.
+ (void)setTimeValue:(base::Time)value forLocalStatePref:(NSString*)prefName;

// Sets time `value` for user pref `prefName` in original profile.
+ (void)setTimeValue:(base::Time)value forUserPref:(NSString*)prefName;

// Sets string `value` for the local state pref with `prefName`. Local State
// contains the preferences that are shared between all profiles.
+ (void)setStringValue:(NSString*)value forLocalStatePref:(NSString*)prefName;

// Sets string `value` for user pref `prefName` in original profile.
+ (void)setStringValue:(NSString*)value forUserPref:(NSString*)prefName;

// Sets bool `value` for local state pref `prefName`.
+ (void)setBoolValue:(BOOL)value forLocalStatePref:(NSString*)prefName;

// Returns value of user pref `prefName` in original profile as JSON string.
+ (NSString*)userPrefValue:(NSString*)prefName;

// Sets boolean `value` for user pref `prefName` in original profile.
+ (void)setBoolValue:(BOOL)value forUserPref:(NSString*)prefName;

// Sets integer `value` for user pref `prefName` in original profile.
+ (void)setIntegerValue:(int)value forUserPref:(NSString*)prefName;

// Sets double `value` for user pref `prefName` in original profile.
+ (void)setDoubleValue:(double)value forUserPref:(NSString*)prefName;

// Returns true if local state pref `prefName` uses default value.
+ (BOOL)prefWithNameIsDefaultValue:(NSString*)prefName;

// Clears user pref `prefName` in original profile.
+ (void)clearUserPrefWithName:(NSString*)prefName;

// Commit synchronously the pending user prefs write. Waits until the disk write
// operation is done.
+ (void)commitPendingUserPrefsWrite;

// Resets the BrowsingDataPrefs, which defines if its selected or not when
// clearing Browsing data.
+ (void)resetBrowsingDataPrefs;

// Resets data for the local state pref with `prefName`.
+ (void)resetDataForLocalStatePref:(NSString*)prefName;

#pragma mark - Unified Consent utilities

// Sets whether URL-keyed anonymized data collection is `enabled`.
+ (void)setURLKeyedAnonymizedDataCollectionEnabled:(BOOL)enabled;

#pragma mark - Keyboard Command utilities

// The count of key commands registered with the currently active BVC.
+ (NSInteger)registeredKeyCommandCount;

// Simulates a physical keyboard event.
// The input is similar to UIKeyCommand parameters, and is designed for testing
// keyboard shortcuts.
// Accepts any strings and also `UIKeyInputUpArrow`, `UIKeyInputDownArrow`,
// `UIKeyInputLeftArrow`, `UIKeyInputRightArrow`, and `UIKeyInputEscape`
// constants as `input`. `flags` must be set to
// UIKeyModifierShift for things like capital letters or characters like !@#$%
// etc.
+ (void)simulatePhysicalKeyboardEvent:(NSString*)input
                                flags:(UIKeyModifierFlags)flags;

#pragma mark - Pasteboard utilities

// Clears the URLs stored in the pasteboard, from the tested app's perspective.
+ (void)clearPasteboardURLs;

// Clears the pasteboard, from the tested app's perspective.
+ (void)clearPasteboard;

// Returns YES if general pasteboard images property contains a nonempty array.
+ (BOOL)pasteboardHasImages;

// Returns stored strings on the pasteboard from tested app perspective.
+ (NSArray<NSString*>*)pasteboardStrings;

// Returns stored URL string on the pasteboard from tested app perspective.
+ (NSString*)pasteboardURLSpec;

// Copies `text` into the clipboard from the app's perspective.
+ (void)copyTextToPasteboard:(NSString*)text;

// Copies `link` into pasteboard as a NSURL.
+ (void)copyLinkAsURLToPasteBoard:(NSString*)link;

// Copies image `imageData` into pasteboard.
+ (void)copyImageToPasteboard:(NSData*)imageData;

#pragma mark - Watcher utilities

// Clear the watcher list, stopping monitoring.
+ (void)stopWatcher;

#pragma mark - Default Browser Promo Utilities

// Clears default browser promo data to restart capping for the promos.
+ (void)clearDefaultBrowserPromoData;

#pragma mark - First Run Utilities

// Writes the First Run Sentinel file, used to record that First Run has
// completed.
+ (void)writeFirstRunSentinel;

// Removes the FirstRun sentinel file.
+ (void)removeFirstRunSentinel;

// Whether the first run sentinel exists.
+ (bool)hasFirstRunSentinel;

#pragma mark - Post Restore Utilities

// Writes the `BackedUpSentinel` file, used to simulate that an iOS device
// backup/restore state has occurred prior to app launch.
+ (void)writeDeviceRestoreSentinel;

// Removes the `BackedUpSentinel` and `NotBackedUpSentinel` files.
+ (void)removeDeviceRestoreSentinel;

#pragma mark - Notification Utilities

// Requests a tips notification of the specified `type`.
+ (void)requestTipsNotification:(TipsNotificationType)type;

#pragma mark - Shared Tab Groups Utilities

// Waits for MessagingBackendService to initialize. Returns nil on success.
+ (NSError*)waitForMessagingBackendServiceInitialized;

// Returns YES if the view with `accessibilityID` or any of its ancestors is
// animating.
+ (BOOL)isViewAnimatingWithAccessibilityID:(NSString*)accessibilityID;

// Prepares an in-app wait for view with `accessibilityID` to stop animating.
// Returns YES if a wait was needed, or NO immediately if already satisfied.
+ (BOOL)prepareConditionForViewToStopAnimatingWithAccessibilityID:
    (NSString*)accessibilityID;

@end

#endif  // IOS_CHROME_TEST_EARL_GREY_CHROME_EARL_GREY_APP_INTERFACE_H_
