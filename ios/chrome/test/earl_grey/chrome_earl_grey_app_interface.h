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
enum class TipsNotificationType;

@interface JavaScriptExecutionResult : NSObject
@property(readonly, nonatomic) BOOL success;
@property(readonly, nonatomic) NSString* result;

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

// Returns the number of entries in the history database. Returns -1 if there
// was an error.
+ (NSInteger)browsingHistoryEntryCountWithError:
    (NSError* __autoreleasing*)error;

// Gets the number of items in the back list. Returns -1 in case of error.
+ (NSInteger)navigationBackListItemsCount;

// Clears browsing cache. Returns nil on success, or else an NSError indicating
// the operation failed.
+ (NSError*)removeBrowsingCache;

// Persists the current list of tabs to disk immediately.
+ (void)saveSessionImmediately;

// Opens `URL` using some connected scene.
+ (void)sceneOpenURL:(NSString*)spec;

// Loads the URL `spec` in the current WebState with transition type
// ui::PAGE_TRANSITION_TYPED and returns without waiting for the page to load.
+ (void)startLoadingURL:(NSString*)spec;

// Returns YES if the current WebState is loading.
+ (BOOL)isLoading;

// Reloads the page without waiting for the page to load.
+ (void)startReloading;

// Loads `URL` as if it was opened from an external application.
+ (void)openURLFromExternalApp:(NSString*)URL;

// Programmatically dismisses settings screen.
+ (void)dismissSettings;

// Stops primes performance metrics logging by calling into the
// internal framework (should only be used by performance tests)
+ (void)primesStopLogging;

// Takes a snapshot of memory usage by calling into the internal
// framework (should only be used by performance tests)
+ (void)primesTakeMemorySnapshot:(NSString*)eventName;

#pragma mark - Tab Utilities (EG2)

// Selects tab with given index in current mode (incognito or main
// (non-incognito)).
+ (void)selectTabAtIndex:(NSUInteger)index;

// Closes tab with the given index in current mode (incognito or main
// (non-incognito)).
+ (void)closeTabAtIndex:(NSUInteger)index;

// Returns YES if the browser is in incognito mode, and NO otherwise.
+ (BOOL)isIncognitoMode [[nodiscard]];

// Returns the number of open non-incognito tabs.
+ (NSUInteger)mainTabCount [[nodiscard]];

// Returns the number of open inactive tabs.
+ (NSUInteger)inactiveTabCount [[nodiscard]];

// Returns the number of open incognito tabs.
+ (NSUInteger)incognitoTabCount [[nodiscard]];

// Returns the number of open browsers.
+ (NSUInteger)browserCount [[nodiscard]];

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

// Simulates opening a custom `URL` from another application.
+ (void)simulateExternalAppURLOpeningWithURL:(NSURL*)URL;

// Simulates opening the add account sign-in flow from the web.
+ (void)simulateAddAccountFromWeb;

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

// Opens a new tab in window with given number, and does not wait for animations
// to complete.
+ (void)openNewTabInWindowWithNumber:(int)windowNumber;

// Closes the window with given number.
+ (void)closeWindowWithNumber:(int)windowNumber;

// Renumbers given window with current number to new number.
+ (void)changeWindowWithNumber:(int)windowNumber
                   toNewNumber:(int)newWindowNumber;

// Loads the URL `spec` in the current WebState in window with given number with
// transition type ui::PAGE_TRANSITION_TYPED and returns without waiting for the
// page to load.
+ (void)startLoadingURL:(NSString*)spec inWindowWithNumber:(int)windowNumber;

// Returns YES if the current WebState in window with given number is loading.
+ (BOOL)isLoadingInWindowWithNumber:(int)windowNumber [[nodiscard]];

// Returns YES if the current WebState in window with given number contains
// `text`.
+ (BOOL)webStateContainsText:(NSString*)text
          inWindowWithNumber:(int)windowNumber;

// Returns the number of open non-incognito tabs, in window with given number.
+ (NSUInteger)mainTabCountInWindowWithNumber:(int)windowNumber;

// Returns the number of open incognito tabs, in window with given number.
+ (NSUInteger)incognitoTabCountInWindowWithNumber:(int)windowNumber;

// Returns a key window from the connected scenes.
+ (UIWindow*)keyWindow;

#pragma mark - WebState Utilities (EG2)

// Attempts to tap the element with `element_id` within window.frames[0] of the
// current WebState using a JavaScript click() event. This only works on
// same-origin iframes. If not succeed returns an NSError indicating why the
// operation failed, otherwise nil.
+ (NSError*)tapWebStateElementInIFrameWithID:(NSString*)elementID;

// Taps html element with `elementID` in the current web state.
// If not succeed returns an NSError indicating why the
// operation failed, otherwise nil.
+ (NSError*)tapWebStateElementWithID:(NSString*)elementID;

// Waits for the current web state to contain an element matching `selector`.
// If not succeed returns an NSError indicating  why the operation failed,
// otherwise nil.
+ (NSError*)waitForWebStateContainingElement:(ElementSelector*)selector;

// Waits for the current web state to no longer contain an element matching
// `selector`. On failure, returns an NSError, otherwise nil.
+ (NSError*)waitForWebStateNotContainingElement:(ElementSelector*)selector;

// Waits for the current web state's frames to contain `text`.
// If not succeed returns an NSError indicating  why the operation failed,
// otherwise nil.
+ (NSError*)waitForWebStateContainingTextInIFrame:(NSString*)text;

// Attempts to submit form with `formID` in the current WebState.
// Returns nil on success, or else an NSError indicating why the operation
// failed.
+ (NSError*)submitWebStateFormWithID:(NSString*)formID;

// Returns YES if the current WebState contains an element matching `selector`.
+ (BOOL)webStateContainsElement:(ElementSelector*)selector;

// Returns YES if the current WebState contains `text`.
+ (BOOL)webStateContainsText:(NSString*)text;

// Waits for the current WebState to contain loaded image with `imageID`.
// When loaded, the image element will have the same size as actual image.
// Returns nil if the condition is met within a timeout, or else an NSError
// indicating why the operation failed.
+ (NSError*)waitForWebStateContainingLoadedImage:(NSString*)imageID;

// Waits for the current WebState to contain a blocked image with `imageID`.
// When blocked, the image element will be smaller than the actual image size.
// Returns nil if the condition is met within a timeout, or else an NSError
// indicating why the operation failed.
+ (NSError*)waitForWebStateContainingBlockedImage:(NSString*)imageID;

// Waits for the web state's scroll view zoom scale to be suitably close (within
// 0.05) of the expected scale. Returns nil if the condition is met within a
// timeout, or else an NSError indicating why the operation failed.
+ (NSError*)waitForWebStateZoomScale:(CGFloat)scale;

// Signs the user out from Chrome and then starts clearing the identities.
//
// Note: The idendities & browsing data cleanings are executed asynchronously.
// The completion block should be used if there's a need to wait the end of
// those operations.
+ (void)signOutAndClearIdentitiesWithCompletion:(ProceduralBlock)completion;

// Returns YES if there is at at least identity in the ChromeIdentityService.
+ (BOOL)hasIdentities;

// Returns the current WebState's VisibleURL.
+ (NSString*)webStateVisibleURL;

// Returns the current WebState's last committed URL.
+ (NSString*)webStateLastCommittedURL;

// Purges cached web view pages in the current web state, so the next time back
// navigation will not use a cached page. Browsers don't have to use a fresh
// version for back/forward navigation for HTTP pages and may serve a version
// from the cache even if the Cache-Control response header says otherwise.
+ (NSError*)purgeCachedWebViewPages;

// Returns YES if the current WebState's web view uses the content inset to
// correctly align the top of the content with the bottom of the top bar.
+ (BOOL)webStateWebViewUsesContentInset;

// Returns the size of the current WebState's web view.
+ (CGSize)webStateWebViewSize;

// Stops any pending navigations in all WebStates which are loading.
+ (void)stopAllWebStatesLoading;

#pragma mark - URL Utilities (EG2)

// Returns the title string to be used for a page with `URL` if that page
// doesn't specify a title.
+ (NSString*)displayTitleForURL:(NSString*)URL;

#pragma mark - Sync Utilities (EG2)

// Waits for sync engine to be initialized or not. It doesn't necessarily mean
// that data types are configured and ready to use. See
// SyncService::IsEngineInitialized() for details. If not succeeded a GREYAssert
// is induced.
+ (NSError*)waitForSyncEngineInitialized:(BOOL)isInitialized
                             syncTimeout:(base::TimeDelta)timeout;

// Waits for the sync feature to be enabled/disabled. See SyncService::
// IsSyncFeatureEnabled() for details. If not succeeded a GREYAssert is induced.
+ (NSError*)waitForSyncFeatureEnabled:(BOOL)isEnabled
                          syncTimeout:(base::TimeDelta)timeout;

// Waits for sync to become fully active; see
// SyncService::TransportState::ACTIVE for details. If not succeeded a
// GREYAssert is induced.
+ (NSError*)waitForSyncTransportStateActiveWithTimeout:(base::TimeDelta)timeout;

// Returns the current sync cache GUID. The sync server must be running when
// calling this.
+ (NSString*)syncCacheGUID;

// Waits for sync invalidation field presence in the DeviceInfo data type on the
// server.
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

// Injects a legacy bookmark into the fake sync server. The legacy bookmark
// means 2015 and earlier, prior to the adoption of GUIDs for originator client
// item ID.
+ (void)addFakeSyncServerLegacyBookmarkWithURL:(NSString*)URL
                                         title:(NSString*)title
                     originator_client_item_id:
                         (NSString*)originator_client_item_id;

// Injects a HISTORY visit to the sync FakeServer.
+ (void)addFakeSyncServerHistoryVisit:(NSURL*)URL;

// Injects device info to sync FakeServer.
+ (void)addFakeSyncServerDeviceInfo:(NSString*)deviceName
               lastUpdatedTimestamp:(base::Time)lastUpdatedTimestamp;

// Adds typed URL into HistoryService.
+ (void)addHistoryServiceTypedURL:(NSString*)URL;

// Adds typed URL into HistoryService at timestamp `visitTimestamp`.
+ (void)addHistoryServiceTypedURL:(NSString*)URL
                   visitTimestamp:(base::Time)visitTimestamp;

// Deletes typed URL from HistoryService.
+ (void)deleteHistoryServiceTypedURL:(NSString*)URL;

// If the provided URL `spec` is either present or not present in HistoryService
// (depending on `expectPresent`), return YES. If the present status of `spec`
// is not what is expected, or there is an error, return NO.
+ (BOOL)isURL:(NSString*)spec presentOnClient:(BOOL)expectPresent;

// Triggers a sync cycle for a `type`.
+ (void)triggerSyncCycleForType:(syncer::DataType)type;

// Injects user demographics into the fake sync server. `rawBirthYear` is the
// true birth year, pre-noise, and the gender corresponds to the proto enum
// UserDemographicsProto::Gender.
+ (void)
    addUserDemographicsToSyncServerWithBirthYear:(int)rawBirthYear
                                          gender:
                                              (metrics::UserDemographicsProto::
                                                   Gender)gender;

// Clears the autofill profile for the given `GUID`.
+ (void)clearAutofillProfileWithGUID:(NSString*)GUID;

// Injects an autofill profile into the fake sync server with `GUID` and
// `full_name`.
+ (void)addAutofillProfileToFakeSyncServerWithGUID:(NSString*)GUID
                               autofillProfileName:(NSString*)fullName;

// Returns YES if there is an autofilll profile with the corresponding `GUID`
// and `full_name`.
+ (BOOL)isAutofillProfilePresentWithGUID:(NSString*)GUID
                     autofillProfileName:(NSString*)fullName;

// Deletes an autofill profile from the fake sync server with `GUID`, if it
// exists. If it doesn't exist, nothing is done.
+ (void)deleteAutofillProfileFromFakeSyncServerWithGUID:(NSString*)GUID;

// Verifies the sessions hierarchy on the Sync FakeServer. `specs` is
// the collection of URLs that are to be expected for a single window. On
// failure, returns a NSError describing the failure. See the
// SessionsHierarchy class for documentation regarding the verification.
+ (NSError*)verifySessionsOnSyncServerWithSpecs:(NSArray<NSString*>*)specs;

// Verifies the URLs (in the HISTORY data type) on the Sync FakeServer.
// `specs` is the collection of expected URLs. On failure, returns a NSError
// describing the failure.
+ (NSError*)verifyHistoryOnSyncServerWithURLs:(NSArray<NSURL*>*)URLs;

// Verifies that `count` entities of the given `type` and `name` exist on the
// sync FakeServer. Folders are not included in this count. Returns NSError
// if there is a failure or if the count does not match.
+ (NSError*)verifyNumberOfSyncEntitiesWithType:(NSUInteger)type
                                          name:(NSString*)name
                                         count:(NSUInteger)count;

// Adds a bookmark with a sync passphrase. The sync server will need the sync
// passphrase to start.
+ (void)addBookmarkWithSyncPassphrase:(NSString*)syncPassphrase;

// Add a sync passphrase requirement to start the sync server.
+ (void)addSyncPassphrase:(NSString*)syncPassphrase;

// Returns whether UserSelectableType::kHistory is among the selected types.
+ (BOOL)isSyncHistoryDataTypeSelected;

#pragma mark - JavaScript Utilities (EG2)

// Executes JavaScript through the WebState's WebFrame and waits for either the
// completion or timeout. If execution does not complete within a timeout or
// JavaScript exception is thrown, `success` is NO.
// otherwise returns object representing execution result.
+ (JavaScriptExecutionResult*)executeJavaScript:(NSString*)javaScript;

// Returns the user agent that should be used for the mobile version.
+ (NSString*)mobileUserAgentString;

#pragma mark - Accessibility Utilities (EG2)

// Verifies that all interactive elements on screen (or at least one of their
// descendants) are accessible.
+ (NSError*)verifyAccessibilityForCurrentScreen [[nodiscard]];

#pragma mark - Check features (EG2)

// Helpers for checking feature state. These can't use FeatureList directly when
// invoked from test code, as the EG test code runs in a separate process and
// must query Chrome for the state.

// Returns YES if `variationID` is enabled.
+ (BOOL)isVariationEnabled:(int)variationID;

// Returns YES if a variation triggering server-side behavior is enabled.
+ (BOOL)isTriggerVariationEnabled:(int)variationID;

// Returns YES if UKM feature is enabled.
+ (BOOL)isUKMEnabled [[nodiscard]];

// Returns YES if kTestFeature is enabled.
+ (BOOL)isTestFeatureEnabled;

// Returns YES if DemographicMetricsReporting feature is enabled.
+ (BOOL)isDemographicMetricsReportingEnabled [[nodiscard]];

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

// Returns whether the NewOverflowMenu feature is enabled.
+ (BOOL)isNewOverflowMenuEnabled;

// Returns whether the UseLensToSearchForImage feature is enabled.
+ (BOOL)isUseLensToSearchForImageEnabled;

// Returns whether the Web Channels feature is enabled.
+ (BOOL)isWebChannelsEnabled;

// Returns whether Tab Group Sync is enabled.
+ (BOOL)isTabGroupSyncEnabled;

// Returns whether the current layout is showing the bottom omnibox.
+ (BOOL)isCurrentLayoutBottomOmnibox;

// Returns whether the Enhanced Safe Browsing Infobar Promo feature is enabled.
+ (BOOL)isEnhancedSafeBrowsingInfobarEnabled;

#pragma mark - ContentSettings

// Gets the current value of the popup content setting preference for the
// original profile.
+ (ContentSetting)popupPrefValue;

// Sets the popup content setting preference to the given value for the original
// profile.
+ (void)setPopupPrefValue:(ContentSetting)value;

// Resets the desktop content setting to its default value.
+ (void)resetDesktopContentSetting;

// Sets the preference value of a content settings type for the original browser
// state.
+ (void)setContentSetting:(ContentSetting)setting
    forContentSettingsType:(ContentSettingsType)type;

#pragma mark - Default Utilities (EG2)

// Stores a value for the provided key in NSUserDefaults.
+ (void)setUserDefaultsObject:(id)value forKey:(NSString*)defaultName;

// Removes the object for the provided `key` in NSUserDefaults.
+ (void)removeUserDefaultsObjectForKey:(NSString*)key;

// Returns the value for provided key from NSUserDefaults.
+ (id)userDefaultsObjectForKey:(NSString*)key;

#pragma mark - Pref Utilities (EG2)

// Gets the value of a local state pref. Returns a
// base::Value encoded as a JSON string. If the pref was not registered,
// returns a Value of type NONE.
+ (NSString*)localStatePrefValue:(NSString*)prefName;

// Sets the integer value for the local state pref with `prefName`. `value`
// can be either a casted enum or any other numerical value. Local State
// contains the preferences that are shared between all profiles.
+ (void)setIntegerValue:(int)value forLocalStatePref:(NSString*)prefName;

// Sets the time value for the local state pref with `prefName`. Local State
// contains the preferences that are shared between all profiles.
+ (void)setTimeValue:(base::Time)value forLocalStatePref:(NSString*)prefName;

// Sets the time value for the user pref with `prefName` in the original
// profile.
+ (void)setTimeValue:(base::Time)value forUserPref:(NSString*)prefName;

// Sets the string value for the local state pref with `prefName`. Local State
// contains the preferences that are shared between all profiles.
+ (void)setStringValue:(NSString*)value forLocalStatePref:(NSString*)prefName;

// Sets the value of a string user pref in the original profile.
+ (void)setStringValue:(NSString*)value forUserPref:(NSString*)prefName;

// Sets the bool value for the local state pref with `prefName`. Local State
// contains the preferences that are shared between all profiles.
+ (void)setBoolValue:(BOOL)value forLocalStatePref:(NSString*)prefName;

// Gets the value of a user pref in the original profile. Returns a
// base::Value encoded as a JSON string. If the pref was not registered,
// returns a Value of type NONE.
+ (NSString*)userPrefValue:(NSString*)prefName;

// Sets the value of a boolean user pref in the original profile.
+ (void)setBoolValue:(BOOL)value forUserPref:(NSString*)prefName;

// Sets the value of a integer user pref in the original profile.
+ (void)setIntegerValue:(int)value forUserPref:(NSString*)prefName;

// Returns true if the LocalState Preference is currently using its default
// value, and has not been set by any higher-priority source (even with the same
// value).
+ (BOOL)prefWithNameIsDefaultValue:(NSString*)prefName;

// Clears the user pref of |prefName|.
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

// Enables or disables URL-keyed anonymized data collection.
+ (void)setURLKeyedAnonymizedDataCollectionEnabled:(BOOL)enabled;

#pragma mark - Keyboard Command utilities

// The count of key commands registered with the currently active BVC.
+ (NSInteger)registeredKeyCommandCount;

// Simulates a physical keyboard event.
// The input is similar to UIKeyCommand parameters, and is designed for testing
// keyboard shortcuts.
// Accepts any strings and also UIKeyInput{Up|Down|Left|Right}Arrow and
// UIKeyInputEscape constants as `input`. `flags` must be set to
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

// Retrieves the currently stored strings on the pasteboard from the tested
// app's perspective.
+ (NSArray<NSString*>*)pasteboardStrings;

// Retrieves the currently stored URL on the pasteboard from the tested app's
// perspective.
+ (NSString*)pasteboardURLSpec;

// Copies `text` into the clipboard from the app's perspective.
+ (void)copyTextToPasteboard:(NSString*)text;

#pragma mark - Watcher utilities

// Starts monitoring for buttons (based on traits) with the given
// (accessibility) `labels`. Monitoring will stop once all are found, or if
// timeout expires. If a previous set is currently being watched for it gets
// replaced with this set. Note that timeout is best effort and can be a bit
// longer than specified. This method returns immediately.
+ (void)watchForButtonsWithLabels:(NSArray<NSString*>*)labels
                          timeout:(base::TimeDelta)timeout;

// Returns YES if the button with given (accessibility) `label` was observed at
// some point since `watchForButtonsWithLabels:timeout:` was called.
+ (BOOL)watcherDetectedButtonWithLabel:(NSString*)label;

// Clear the watcher list, stopping monitoring.
+ (void)stopWatcher;

#pragma mark - Default Browser Promo Utilities

// Clears default browser promo data to restart capping for the promos.
+ (void)clearDefaultBrowserPromoData;

// Copies a chrome:// URL that doesn't require internet connection.
+ (void)copyURLToPasteBoard;

#pragma mark - First Run Utilities

// Writes the First Run Sentinel file, used to record that First Run has
// completed.
+ (void)writeFirstRunSentinel;

// Removes the FirstRun sentinel file.
+ (void)removeFirstRunSentinel;

// Whether the first run sentinel exists.
+ (bool)hasFirstRunSentinel;

#pragma mark - Notification Utilities

+ (void)requestTipsNotification:(TipsNotificationType)type;

@end

#endif  // IOS_CHROME_TEST_EARL_GREY_CHROME_EARL_GREY_APP_INTERFACE_H_
