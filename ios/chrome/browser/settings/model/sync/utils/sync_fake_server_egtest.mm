// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/ios/ios_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/time/time.h"
#import "components/browser_sync/browser_sync_switches.h"
#import "components/sync/base/command_line_switches.h"
#import "components/sync/base/data_type.h"
#import "components/sync/base/features.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_storage_type.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/bookmark_earl_grey.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_navigation_controller_constants.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_app_interface.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_egtest_utils.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_constants.h"
#import "ios/chrome/browser/ui/settings/password/password_manager_egtest_utils.h"
#import "ios/chrome/browser/ui/settings/password/password_settings_app_interface.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/test_switches.h"
#import "ios/chrome/test/earl_grey/web_http_server_chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/public/test/http_server/http_server.h"
#import "ios/web/public/test/http_server/http_server_util.h"
#import "net/base/apple/url_conversions.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Constant for timeout while waiting for asynchronous sync operations.
constexpr base::TimeDelta kSyncOperationTimeout = base::Seconds(10);

constexpr NSString* kBookmarkUrl = @"https://www.goo.com/";
constexpr NSString* kBookmarkTitle = @"Goo";

constexpr NSString* kReadingListUrl = @"https://www.rl.com/";
constexpr NSString* kReadingListTitle = @"RL";

constexpr NSString* kPassphrase = @"passphrase";

// Waits for `entity_count` entities of type `entity_type` on the fake server,
// and fails with a GREYAssert if the condition is not met, within a short
// period of time.
void WaitForEntitiesOnFakeServer(int entity_count,
                                 syncer::DataType entity_type) {
  ConditionBlock condition = ^{
    return [ChromeEarlGrey numberOfSyncEntitiesWithType:entity_type] ==
           entity_count;
  };
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(kSyncOperationTimeout,
                                                          condition),
             @"Expected %d %s entities but found %d", entity_count,
             syncer::DataTypeToDebugString(entity_type),
             [ChromeEarlGrey numberOfSyncEntitiesWithType:entity_type]);
}

void ClearRelevantData() {
  [BookmarkEarlGrey clearBookmarks];
  GREYAssertNil([ReadingListAppInterface clearEntries],
                @"Unable to clear Reading List entries");
  [PasswordSettingsAppInterface clearPasswordStores];

  [ChromeEarlGrey clearFakeSyncServerData];
  WaitForEntitiesOnFakeServer(0, syncer::BOOKMARKS);
  WaitForEntitiesOnFakeServer(0, syncer::HISTORY);
  WaitForEntitiesOnFakeServer(0, syncer::PASSWORDS);
  WaitForEntitiesOnFakeServer(0, syncer::READING_LIST);

  // Ensure that all of the changes made are flushed to disk before the app is
  // terminated.
  [ChromeEarlGrey flushFakeSyncServerToDisk];
  [ChromeEarlGrey commitPendingUserPrefsWrite];
  [BookmarkEarlGrey commitPendingWrite];
  // Note that the ReadingListModel immediately writes pending changes to disk,
  // so no need for an explicit "flush" there.
}

}  // namespace

// Hermetic sync tests, which use the fake sync server.
@interface SyncFakeServerTestCase : WebHttpServerChromeTestCase
@end

@implementation SyncFakeServerTestCase

+ (void)setUpForTestCase {
  [super setUpForTestCase];

  [BookmarkEarlGrey waitForBookmarkModelLoaded];

  // Normally there shouldn't be any data (locally or on the fake server) at
  // this point, but just in case some other test case didn't clean up after
  // itself, clear everything here.
  ClearRelevantData();
}

- (void)setUp {
  [super setUp];

  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
}

- (void)tearDown {
  ClearRelevantData();

  [super tearDown];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  config.additional_args.push_back(std::string("--") +
                                   syncer::kSyncShortNudgeDelayForTest);

  if ([self isRunningTest:@selector(testMigrateSyncToSignin)] ||
      [self
          isRunningTest:@selector(testMigrateSyncToSignin_PasswordsDisabled)] ||
      [self
          isRunningTest:@selector(testMigrateSyncToSignin_BookmarksDisabled)] ||
      [self isRunningTest:@selector
            (testMigrateSyncToSignin_ReadingListDisabled)] ||
      [self isRunningTest:@selector(testMigrateSyncToSignin_SyncNotActive)] ||
      [self
          isRunningTest:@selector(testMigrateSyncToSignin_CustomPassphrase)] ||
      [self isRunningTest:@selector
            (testMigrateSyncToSignin_CustomPassphraseMissing)] ||
      [self isRunningTest:@selector(testMigrateSyncToSignin_ManagedAccount)] ||
      [self isRunningTest:@selector(testMigrateSyncToSignin_Undo)]) {
    // The testMigrateSyncToSignin* tests start with
    // kMigrateSyncingUserToSignedIn disabled, but later turn on the flag and
    // restart Chrome.
    config.features_disabled.push_back(switches::kMigrateSyncingUserToSignedIn);
  }
  if ([self isRunningTest:@selector
            (testManagedAccountClearsDataForSignedInPeriod)]) {
    config.features_enabled.push_back(kClearDeviceDataOnSignOutForManagedUsers);
    config.features_disabled.push_back(kIdentityDiscAccountMenu);
  }
  if ([self isRunningTest:@selector
            (testManagedAccountClearsDataAndTabsForSignedInPeriod)]) {
    config.features_enabled.push_back(kClearDeviceDataOnSignOutForManagedUsers);
    config.features_enabled.push_back(kIdentityDiscAccountMenu);
  }

  return config;
}

- (void)relaunchWithIdentity:(FakeSystemIdentity*)identity
             enabledFeatures:(const std::vector<base::test::FeatureRef>&)enabled
            disabledFeatures:
                (const std::vector<base::test::FeatureRef>&)disabled {
  // Before restarting, ensure that the FakeServer has written all its pending
  // state to disk.
  [ChromeEarlGrey flushFakeSyncServerToDisk];
  // Also make sure any pending prefs and bookmarks changes are written to disk.
  [ChromeEarlGrey commitPendingUserPrefsWrite];
  [BookmarkEarlGrey commitPendingWrite];

  AppLaunchConfiguration config = [self appConfigurationForTestCase];
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  config.features_enabled = enabled;
  config.features_disabled = disabled;
  config.additional_args.push_back(
      base::StrCat({"--", test_switches::kSignInAtStartup}));
  config.additional_args.push_back(base::StrCat({
    "-", test_switches::kAddFakeIdentitiesAtStartup, "=",
        [FakeSystemIdentity encodeIdentitiesToBase64:@[ identity ]]
  }));
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  // After the relaunch, wait for Sync-the-transport to become active again
  // (which should always happen if there's a signed-in account).
  [ChromeEarlGrey
      waitForSyncTransportStateActiveWithTimeout:kSyncOperationTimeout];
}

// Opens the legacy Sync settings, and disables the data type whose toggle is
// identified by `typeIdentifier` (e.g. `kSyncPasswordsIdentifier`).
- (void)disableTypeForSyncTheFeature:(NSString*)typeIdentifier {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI
      tapSettingsMenuButton:chrome_test_util::ManageSyncSettingsButton()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kSyncEverythingItemAccessibilityIdentifier)]
      performAction:chrome_test_util::TurnTableViewSwitchOn(/*on=*/NO)];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                          typeIdentifier,
                                          /*is_toggled_on=*/YES,
                                          /*is_enabled=*/YES)]
      performAction:chrome_test_util::TurnTableViewSwitchOn(/*on=*/NO)];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsDoneButton()]
      performAction:grey_tap()];
}

- (void)enableTypeForSyncTheFeature:(NSString*)typeIdentifier {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI
      tapSettingsMenuButton:chrome_test_util::ManageSyncSettingsButton()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                          typeIdentifier,
                                          /*is_toggled_on=*/NO,
                                          /*is_enabled=*/YES)]
      performAction:chrome_test_util::TurnTableViewSwitchOn(/*on=*/YES)];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsDoneButton()]
      performAction:grey_tap()];
}

// Tests that a bookmark added on the client is uploaded to the Sync server.
- (void)testSyncUploadBookmark {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];

  // Add a bookmark after sync is initialized.
  [ChromeEarlGrey waitForSyncEngineInitialized:YES
                                   syncTimeout:kSyncOperationTimeout];
  [BookmarkEarlGrey addBookmarkWithTitle:@"goo"
                                     URL:@"https://www.goo.com"
                               inStorage:BookmarkStorageType::kAccount];
  WaitForEntitiesOnFakeServer(1, syncer::BOOKMARKS);
}

// Tests that a bookmark injected in the FakeServer is synced down to the
// client.
- (void)testSyncDownloadBookmark {
  [BookmarkEarlGrey verifyBookmarksWithTitle:@"hoo"
                               expectedCount:0
                                   inStorage:BookmarkStorageType::kAccount];
  const GURL URL = web::test::HttpServer::MakeUrl("http://www.hoo.com");
  [ChromeEarlGrey addFakeSyncServerBookmarkWithURL:URL title:"hoo"];

  // Sign in to sync, after a bookmark has been injected in the sync server.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];

  [ChromeEarlGrey
      waitForSyncTransportStateActiveWithTimeout:kSyncOperationTimeout];
  [BookmarkEarlGrey verifyBookmarksWithTitle:@"hoo"
                               expectedCount:1
                                   inStorage:BookmarkStorageType::kAccount];
}

// Tests that the local cache guid is reused when the user signs out and then
// signs back in with the same account.
- (void)testSyncCheckSameCacheGuid_SignOutAndSignIn {
  // Sign in a fake identity, and store the initial sync guid.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];
  [ChromeEarlGrey
      waitForSyncTransportStateActiveWithTimeout:kSyncOperationTimeout];
  std::string original_guid = [ChromeEarlGrey syncCacheGUID];

  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
  [SigninEarlGrey signOut];
  [ChromeEarlGrey waitForSyncEngineInitialized:NO
                                   syncTimeout:kSyncOperationTimeout];

  // Sign the user back in, and verify the guid has *not* changed.
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];
  [ChromeEarlGrey
      waitForSyncTransportStateActiveWithTimeout:kSyncOperationTimeout];
  GREYAssertTrue([ChromeEarlGrey syncCacheGUID] == original_guid,
                 @"guid changed after user signed out and signed back in");
}

// Tests that the local cache guid changes when the user signs out and then
// signs back in with a different account.
- (void)testSyncCheckDifferentCacheGuid_SignOutAndSignInWithDifferentAccount {
  // Sign in a fake identity, and store the initial sync guid.
  FakeSystemIdentity* fakeIdentity1 = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity1];
  [ChromeEarlGrey
      waitForSyncTransportStateActiveWithTimeout:kSyncOperationTimeout];
  std::string original_guid = [ChromeEarlGrey syncCacheGUID];

  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity1];
  [SigninEarlGrey signOut];
  [ChromeEarlGrey waitForSyncEngineInitialized:NO
                                   syncTimeout:kSyncOperationTimeout];

  // Sign a different user in, and verify the guid has changed.
  FakeSystemIdentity* fakeIdentity2 = [FakeSystemIdentity fakeIdentity2];
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity2];
  [ChromeEarlGrey
      waitForSyncTransportStateActiveWithTimeout:kSyncOperationTimeout];
  GREYAssertTrue(
      [ChromeEarlGrey syncCacheGUID] != original_guid,
      @"guid didn't change after user signed out and different user signed in");
}

// Tests that tabs opened on this client are committed to the Sync server and
// that the created sessions entities are correct.
- (void)testSyncUploadOpenTabs {
  // Create map of canned responses and set up the test HTML server.
  const GURL URL1 = web::test::HttpServer::MakeUrl("http://page1");
  const GURL URL2 = web::test::HttpServer::MakeUrl("http://page2");
  std::map<GURL, std::string> responses = {
      {URL1, std::string("page 1")},
      {URL2, std::string("page 2")},
  };
  web::test::SetUpSimpleHttpServer(responses);

  // Load both URLs in separate tabs.
  [ChromeEarlGrey loadURL:URL1];
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:URL2];

  // Sign in to sync, after opening two tabs.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity enableHistorySync:YES];

  // Verify the sessions on the sync server.
  [ChromeEarlGrey waitForSyncEngineInitialized:YES
                                   syncTimeout:kSyncOperationTimeout];
  WaitForEntitiesOnFakeServer(3, syncer::SESSIONS);

  NSArray<NSString*>* specs = @[
    base::SysUTF8ToNSString(URL1.spec()),
    base::SysUTF8ToNSString(URL2.spec()),
  ];
  [ChromeEarlGrey verifySyncServerSessionURLs:specs];
}

// Tests that browsing history is uploaded to the Sync server.
- (void)testSyncHistoryUpload {
  const GURL preSyncURL = self.testServer->GetURL("/console.html");
  const GURL whileSyncURL = self.testServer->GetURL("/pony.html");
  const GURL postSyncURL = self.testServer->GetURL("/destination.html");

  [ChromeEarlGrey clearBrowsingHistory];
  [self setTearDownHandler:^{
    [ChromeEarlGrey clearBrowsingHistory];
  }];

  // Visit a URL before turning on Sync.
  [ChromeEarlGrey loadURL:preSyncURL];

  // Navigate away from that URL.
  [ChromeEarlGrey loadURL:whileSyncURL];

  // Sign in and wait for sync to become active.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity enableHistorySync:YES];

  [ChromeEarlGrey
      waitForSyncTransportStateActiveWithTimeout:kSyncOperationTimeout];

  // Navigate again. This URL, plus the URL that was currently open when Sync
  // was turned on, should arrive on the Sync server.
  [ChromeEarlGrey loadURL:postSyncURL];

  // This URL, plus the URL that was currently open when Sync was turned on,
  // should arrive on the Sync server.
  NSArray<NSURL*>* URLs = @[
    net::NSURLWithGURL(whileSyncURL),
    net::NSURLWithGURL(postSyncURL),
  ];
  [ChromeEarlGrey waitForSyncServerHistoryURLs:URLs
                                       timeout:kSyncOperationTimeout];
}

// Tests that history is downloaded from the sync server.
- (void)testSyncHistoryDownload {
  const GURL mockURL("http://not-a-real-site/");

  [ChromeEarlGrey clearBrowsingHistory];
  [self setTearDownHandler:^{
    [ChromeEarlGrey clearBrowsingHistory];
  }];

  // Inject a history visit on the server.
  [ChromeEarlGrey addFakeSyncServerHistoryVisit:mockURL];

  // Sign in and wait for sync to become active.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity enableHistorySync:YES];

  [ChromeEarlGrey
      waitForSyncTransportStateActiveWithTimeout:kSyncOperationTimeout];

  // Wait for the visit to appear on the client.
  [ChromeEarlGrey waitForHistoryURL:mockURL
                      expectPresent:YES
                            timeout:kSyncOperationTimeout];
}

// Tests download of two legacy bookmarks with the same item id.
- (void)testDownloadTwoPre2015BookmarksWithSameItemId {
  const GURL URL1 = web::test::HttpServer::MakeUrl("http://page1.com");
  const GURL URL2 = web::test::HttpServer::MakeUrl("http://page2.com");
  NSString* title1 = @"title1";
  NSString* title2 = @"title2";

  [BookmarkEarlGrey verifyBookmarksWithTitle:title1
                               expectedCount:0
                                   inStorage:BookmarkStorageType::kAccount];
  [BookmarkEarlGrey verifyBookmarksWithTitle:title2
                               expectedCount:0
                                   inStorage:BookmarkStorageType::kAccount];

  // Mimic the creation of two bookmarks from two different devices, with the
  // same client item ID.
  [ChromeEarlGrey
      addFakeSyncServerLegacyBookmarkWithURL:URL1
                                       title:base::SysNSStringToUTF8(title1)
                   originator_client_item_id:"1"];
  [ChromeEarlGrey
      addFakeSyncServerLegacyBookmarkWithURL:URL2
                                       title:base::SysNSStringToUTF8(title2)
                   originator_client_item_id:"1"];

  // Sign in to sync.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];

  [ChromeEarlGrey
      waitForSyncTransportStateActiveWithTimeout:kSyncOperationTimeout];

  [BookmarkEarlGrey verifyBookmarksWithTitle:title1
                               expectedCount:1
                                   inStorage:BookmarkStorageType::kAccount];
  [BookmarkEarlGrey verifyBookmarksWithTitle:title2
                               expectedCount:1
                                   inStorage:BookmarkStorageType::kAccount];
}

- (void)testSyncInvalidationsEnabled {
  // Sign in to sync.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];

  [ChromeEarlGrey waitForSyncEngineInitialized:YES
                                   syncTimeout:kSyncOperationTimeout];
  WaitForEntitiesOnFakeServer(1, syncer::DEVICE_INFO);
  [ChromeEarlGrey waitForSyncInvalidationFields];
}

- (void)testMigrateSyncToSignin {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  // Sign in and turn on Sync-the-feature.
  [SigninEarlGrey signinAndEnableLegacySyncFeature:fakeIdentity];
  [ChromeEarlGrey waitForSyncFeatureEnabled:YES
                                syncTimeout:kSyncOperationTimeout];
  [ChromeEarlGrey
      waitForSyncTransportStateActiveWithTimeout:kSyncOperationTimeout];

  // Create some data and wait for it to arrive on the server.
  [BookmarkEarlGrey addBookmarkWithTitle:kBookmarkTitle
                                     URL:kBookmarkUrl
                               inStorage:BookmarkStorageType::kLocalOrSyncable];
  GREYAssertNil([ReadingListAppInterface
                    addEntryWithURL:[NSURL URLWithString:kReadingListUrl]
                              title:kReadingListTitle
                               read:YES],
                @"Unable to add Reading List item");
  password_manager_test_utils::SavePasswordFormToProfileStore();

  WaitForEntitiesOnFakeServer(1, syncer::BOOKMARKS);
  WaitForEntitiesOnFakeServer(1, syncer::READING_LIST);
  WaitForEntitiesOnFakeServer(1, syncer::PASSWORDS);

  // Restart Chrome with UNO phase 3 (i.e. the migration) enabled.
  [self relaunchWithIdentity:fakeIdentity
             enabledFeatures:{switches::kMigrateSyncingUserToSignedIn}
            disabledFeatures:{}];
  // Sync-the-feature should *not* be enabled anymore.
  [ChromeEarlGrey waitForSyncFeatureEnabled:NO
                                syncTimeout:kSyncOperationTimeout];

  // The bookmark should still exist, but now be in the account store.
  [BookmarkEarlGrey
      verifyAbsenceOfBookmarkWithURL:kBookmarkUrl
                           inStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGrey
      verifyExistenceOfBookmarkWithURL:kBookmarkUrl
                                  name:kBookmarkTitle
                             inStorage:BookmarkStorageType::kAccount];
  // Similarly the password.
  GREYAssertEqual(
      0, [PasswordSettingsAppInterface passwordProfileStoreResultsCount],
      @"Password should NOT be in the profile store");
  GREYAssertEqual(
      1, [PasswordSettingsAppInterface passwordAccountStoreResultsCount],
      @"Password should be in the account store");

  // The reading list item should still exist, and *not* have a crossed-cloud
  // icon (no crossed-cloud icon means that it's in the account store).
  reading_list_test_utils::OpenReadingList();
  [[EarlGrey
      selectElementWithMatcher:reading_list_test_utils::VisibleReadingListItem(
                                   kReadingListTitle)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey
      selectElementWithMatcher:reading_list_test_utils::VisibleLocalItemIcon(
                                   kReadingListTitle)]
      assertWithMatcher:grey_nil()];
  // Close the Reading List.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kTableViewNavigationDismissButtonId)]
      performAction:grey_tap()];

  // The sync machinery should still be functional: Add an account bookmark
  // and ensure it arrives on the server.
  [BookmarkEarlGrey addBookmarkWithTitle:@"Second bookmark"
                                     URL:@"https://second.com/"
                               inStorage:BookmarkStorageType::kAccount];
  WaitForEntitiesOnFakeServer(2, syncer::BOOKMARKS);
}

- (void)testMigrateSyncToSignin_PasswordsDisabled {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  // Sign in and turn on Sync-the-feature.
  [SigninEarlGrey signinAndEnableLegacySyncFeature:fakeIdentity];
  [ChromeEarlGrey waitForSyncFeatureEnabled:YES
                                syncTimeout:kSyncOperationTimeout];
  [ChromeEarlGrey
      waitForSyncTransportStateActiveWithTimeout:kSyncOperationTimeout];

  // Save a password and wait for it to be uploaded to the server.
  password_manager_test_utils::SavePasswordFormToProfileStore();
  WaitForEntitiesOnFakeServer(1, syncer::PASSWORDS);

  // Also create a bookmark.
  [BookmarkEarlGrey addBookmarkWithTitle:kBookmarkTitle
                                     URL:kBookmarkUrl
                               inStorage:BookmarkStorageType::kLocalOrSyncable];
  WaitForEntitiesOnFakeServer(1, syncer::BOOKMARKS);

  // Disable the Passwords data type.
  [self disableTypeForSyncTheFeature:kSyncPasswordsIdentifier];

  // Restart Chrome with UNO phase 3 (i.e. the migration) enabled.
  [self relaunchWithIdentity:fakeIdentity
             enabledFeatures:{switches::kMigrateSyncingUserToSignedIn}
            disabledFeatures:{}];
  // Sync-the-feature should *not* be enabled anymore.
  [ChromeEarlGrey waitForSyncFeatureEnabled:NO
                                syncTimeout:kSyncOperationTimeout];

  // The password should still be in the profile store, since the Passwords data
  // type was disabled at the time of migration.
  GREYAssertEqual(
      1, [PasswordSettingsAppInterface passwordProfileStoreResultsCount],
      @"Password should (still) be in the profile store");
  GREYAssertEqual(
      0, [PasswordSettingsAppInterface passwordAccountStoreResultsCount],
      @"Password should NOT be in the account store");

  // The bookmark should have been moved to the account store.
  [BookmarkEarlGrey
      verifyAbsenceOfBookmarkWithURL:kBookmarkUrl
                           inStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGrey
      verifyExistenceOfBookmarkWithURL:kBookmarkUrl
                                  name:kBookmarkTitle
                             inStorage:BookmarkStorageType::kAccount];
}

- (void)testMigrateSyncToSignin_BookmarksDisabled {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  // Sign in and turn on Sync-the-feature.
  [SigninEarlGrey signinAndEnableLegacySyncFeature:fakeIdentity];
  [ChromeEarlGrey waitForSyncFeatureEnabled:YES
                                syncTimeout:kSyncOperationTimeout];
  [ChromeEarlGrey
      waitForSyncTransportStateActiveWithTimeout:kSyncOperationTimeout];

  // Create a bookmark and wait for it to be uploaded to the server.
  [BookmarkEarlGrey addBookmarkWithTitle:kBookmarkTitle
                                     URL:kBookmarkUrl
                               inStorage:BookmarkStorageType::kLocalOrSyncable];
  WaitForEntitiesOnFakeServer(1, syncer::BOOKMARKS);

  // Also save a password.
  password_manager_test_utils::SavePasswordFormToProfileStore();
  WaitForEntitiesOnFakeServer(1, syncer::PASSWORDS);

  // Disable the Bookmarks data type.
  [self disableTypeForSyncTheFeature:kSyncBookmarksIdentifier];

  // Restart Chrome with UNO phase 3 (i.e. the migration) enabled.
  [self relaunchWithIdentity:fakeIdentity
             enabledFeatures:{switches::kMigrateSyncingUserToSignedIn}
            disabledFeatures:{}];
  // Sync-the-feature should *not* be enabled anymore.
  [ChromeEarlGrey waitForSyncFeatureEnabled:NO
                                syncTimeout:kSyncOperationTimeout];

  // The bookmark should still be in the local-or-syncable store, since the
  // Bookmarks data type was disabled at the time of migration.
  [BookmarkEarlGrey
      verifyExistenceOfBookmarkWithURL:kBookmarkUrl
                                  name:kBookmarkTitle
                             inStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGrey
      verifyAbsenceOfBookmarkWithURL:kBookmarkUrl
                           inStorage:BookmarkStorageType::kAccount];

  // The password should have been moved to the account store.
  GREYAssertEqual(
      0, [PasswordSettingsAppInterface passwordProfileStoreResultsCount],
      @"Password should NOT be in the profile store");
  GREYAssertEqual(
      1, [PasswordSettingsAppInterface passwordAccountStoreResultsCount],
      @"Password should be in the account store");
}

- (void)testMigrateSyncToSignin_ReadingListDisabled {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  // Sign in and turn on Sync-the-feature.
  [SigninEarlGrey signinAndEnableLegacySyncFeature:fakeIdentity];
  [ChromeEarlGrey waitForSyncFeatureEnabled:YES
                                syncTimeout:kSyncOperationTimeout];
  [ChromeEarlGrey
      waitForSyncTransportStateActiveWithTimeout:kSyncOperationTimeout];

  // Disable the ReadingList data type.
  [self disableTypeForSyncTheFeature:kSyncReadingListIdentifier];

  // Create a reading list entry.
  GREYAssertNil([ReadingListAppInterface
                    addEntryWithURL:[NSURL URLWithString:kReadingListUrl]
                              title:kReadingListTitle
                               read:YES],
                @"Unable to add Reading List item");

  // Also save a password.
  password_manager_test_utils::SavePasswordFormToProfileStore();
  WaitForEntitiesOnFakeServer(1, syncer::PASSWORDS);

  // Restart Chrome with UNO phase 3 (i.e. the migration) enabled.
  [self relaunchWithIdentity:fakeIdentity
             enabledFeatures:{switches::kMigrateSyncingUserToSignedIn}
            disabledFeatures:{}];
  // Sync-the-feature should *not* be enabled anymore.
  [ChromeEarlGrey waitForSyncFeatureEnabled:NO
                                syncTimeout:kSyncOperationTimeout];

  // Enable the ReadingList data type again. This isn't really required, but
  // without doing this there's no easy way to verify that the entry is still in
  // the local store (the crossed-out cloud icon only shows up if the type is
  // enabled).
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI
      tapSettingsMenuButton:chrome_test_util::SettingsAccountButton()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                          kSyncReadingListIdentifier,
                                          /*is_toggled_on=*/NO,
                                          /*is_enabled=*/YES)]
      performAction:chrome_test_util::TurnTableViewSwitchOn(/*on=*/YES)];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsDoneButton()]
      performAction:grey_tap()];

  // The reading list entry should still be in the local-or-syncable store
  // (indicated by the crossed-out cloud icon), since the ReadingList data type
  // was disabled at the time of migration.
  reading_list_test_utils::OpenReadingList();
  [[EarlGrey
      selectElementWithMatcher:reading_list_test_utils::VisibleReadingListItem(
                                   kReadingListTitle)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey
      selectElementWithMatcher:reading_list_test_utils::VisibleLocalItemIcon(
                                   kReadingListTitle)]
      assertWithMatcher:grey_notNil()];

  // The password should have been moved to the account store.
  GREYAssertEqual(
      0, [PasswordSettingsAppInterface passwordProfileStoreResultsCount],
      @"Password should NOT be in the profile store");
  GREYAssertEqual(
      1, [PasswordSettingsAppInterface passwordAccountStoreResultsCount],
      @"Password should be in the account store");
}

- (void)testMigrateSyncToSignin_SyncNotActive {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  // Sign in and turn on Sync-the-feature.
  [SigninEarlGrey signinAndEnableLegacySyncFeature:fakeIdentity];
  [ChromeEarlGrey waitForSyncFeatureEnabled:YES
                                syncTimeout:kSyncOperationTimeout];
  [ChromeEarlGrey
      waitForSyncTransportStateActiveWithTimeout:kSyncOperationTimeout];

  // Disable a data type (so that we can later re-enable it to trigger a
  // reconfiguration).
  [self disableTypeForSyncTheFeature:kSyncReadingListIdentifier];

  // Disconnect the fake server, simulating a network/connection issue.
  [ChromeEarlGrey disconnectFakeSyncServerNetwork];
  [self setTearDownHandler:^{
    [ChromeEarlGrey connectFakeSyncServerNetwork];
  }];

  // Re-enable the data type that was previously disabled. This causes a
  // reconfiguration, which will not complete due to the network issue.
  [self enableTypeForSyncTheFeature:kSyncReadingListIdentifier];

  // Now, while Sync is not active (it's reconfiguring), restart Chrome with UNO
  // phase 3 (i.e. the migration) enabled.
  [self relaunchWithIdentity:fakeIdentity
             enabledFeatures:{switches::kMigrateSyncingUserToSignedIn}
            disabledFeatures:{}];

  // Because Sync wasn't active at the time of the migration attempt, the
  // migration should NOT have happened, and Sync-the-feature should still be
  // enabled.
  [ChromeEarlGrey waitForSyncFeatureEnabled:YES
                                syncTimeout:kSyncOperationTimeout];

  // Resolve the network error and wait for Sync to become active.
  [ChromeEarlGrey connectFakeSyncServerNetwork];
  [ChromeEarlGrey
      waitForSyncTransportStateActiveWithTimeout:kSyncOperationTimeout];

  // Relaunch again - this time the migration should trigger.
  [self relaunchWithIdentity:fakeIdentity
             enabledFeatures:{switches::kMigrateSyncingUserToSignedIn}
            disabledFeatures:{}];
  // ...and Sync-the-feature should NOT be enabled anymore.
  [ChromeEarlGrey waitForSyncFeatureEnabled:NO
                                syncTimeout:kSyncOperationTimeout];
}

- (void)testMigrateSyncToSignin_CustomPassphrase {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  // Set up a custom passphrase.
  [ChromeEarlGrey addBookmarkWithSyncPassphrase:kPassphrase];

  // Sign in and turn on Sync-the-feature.
  [SigninEarlGrey signinAndEnableLegacySyncFeature:fakeIdentity];
  [ChromeEarlGrey waitForSyncFeatureEnabled:YES
                                syncTimeout:kSyncOperationTimeout];
  [ChromeEarlGrey
      waitForSyncTransportStateActiveWithTimeout:kSyncOperationTimeout];

  // Now Sync is in the "passphrase required" state. Resolve the passphrase
  // error from Sync settings.
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI
      tapSettingsMenuButton:chrome_test_util::ManageSyncSettingsButton()];
  // Tap "Enter Passphrase" button.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_SYNC_ERROR_TITLE)]
      performAction:grey_tap()];
  // Enter the passphrase.
  [SigninEarlGreyUI submitSyncPassphrase:kPassphrase];
  // Close settings.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsDoneButton()]
      performAction:grey_tap()];

  // Save a bookmark and a password and wait for them to be uploaded.
  [BookmarkEarlGrey addBookmarkWithTitle:kBookmarkTitle
                                     URL:kBookmarkUrl
                               inStorage:BookmarkStorageType::kLocalOrSyncable];
  password_manager_test_utils::SavePasswordFormToProfileStore();
  WaitForEntitiesOnFakeServer(2, syncer::BOOKMARKS);
  WaitForEntitiesOnFakeServer(1, syncer::PASSWORDS);

  // Restart Chrome with UNO phase 3 (i.e. the migration) enabled.
  [self relaunchWithIdentity:fakeIdentity
             enabledFeatures:{switches::kMigrateSyncingUserToSignedIn}
            disabledFeatures:{}];
  // Sync-the-feature should *not* be enabled anymore.
  [ChromeEarlGrey waitForSyncFeatureEnabled:NO
                                syncTimeout:kSyncOperationTimeout];

  // The password should have been migrated to the account store.
  GREYAssertEqual(
      0, [PasswordSettingsAppInterface passwordProfileStoreResultsCount],
      @"Password should NOT be in the profile store anymore");
  GREYAssertEqual(
      1, [PasswordSettingsAppInterface passwordAccountStoreResultsCount],
      @"Password should be in the account store");

  // The bookmark should have been migrated to the account store.
  [BookmarkEarlGrey
      verifyAbsenceOfBookmarkWithURL:kBookmarkUrl
                           inStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGrey
      verifyExistenceOfBookmarkWithURL:kBookmarkUrl
                                  name:kBookmarkTitle
                             inStorage:BookmarkStorageType::kAccount];
}

- (void)testMigrateSyncToSignin_CustomPassphraseMissing {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  // Set up a custom passphrase.
  [ChromeEarlGrey addSyncPassphrase:kPassphrase];

  // Sign in and turn on Sync-the-feature.
  [SigninEarlGrey signinAndEnableLegacySyncFeature:fakeIdentity];
  [ChromeEarlGrey waitForSyncFeatureEnabled:YES
                                syncTimeout:kSyncOperationTimeout];
  [ChromeEarlGrey
      waitForSyncTransportStateActiveWithTimeout:kSyncOperationTimeout];

  // Now Sync is in the "passphrase required" state. Verify this in settings.
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI
      tapSettingsMenuButton:chrome_test_util::ManageSyncSettingsButton()];
  // Check that the "Enter Passphrase" button is there.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_SYNC_ERROR_TITLE)]
      assertWithMatcher:grey_sufficientlyVisible()];
  // Close settings.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsDoneButton()]
      performAction:grey_tap()];

  // Save a bookmark and a password. Note that they will not be uploaded to the
  // server, due to the missing passphrase.
  [BookmarkEarlGrey addBookmarkWithTitle:kBookmarkTitle
                                     URL:kBookmarkUrl
                               inStorage:BookmarkStorageType::kLocalOrSyncable];
  password_manager_test_utils::SavePasswordFormToProfileStore();

  // Restart Chrome with UNO phase 3 (i.e. the migration) enabled.
  [self relaunchWithIdentity:fakeIdentity
             enabledFeatures:{switches::kMigrateSyncingUserToSignedIn}
            disabledFeatures:{}];
  // Sync-the-feature should *not* be enabled anymore.
  [ChromeEarlGrey waitForSyncFeatureEnabled:NO
                                syncTimeout:kSyncOperationTimeout];

  // The password should NOT have been migrated to the account store.
  GREYAssertEqual(
      1, [PasswordSettingsAppInterface passwordProfileStoreResultsCount],
      @"Password should still be in the profile store");
  GREYAssertEqual(
      0, [PasswordSettingsAppInterface passwordAccountStoreResultsCount],
      @"Password should NOT be in the account store");

  // The bookmark should NOT have been migrated to the account store.
  [BookmarkEarlGrey
      verifyExistenceOfBookmarkWithURL:kBookmarkUrl
                                  name:kBookmarkTitle
                             inStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGrey
      verifyAbsenceOfBookmarkWithURL:kBookmarkUrl
                           inStorage:BookmarkStorageType::kAccount];
}

- (void)testMigrateSyncToSignin_ManagedAccount {
  // Use a managed (aka enterprise) account.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeManagedIdentity];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  // Sign in and turn on Sync-the-feature.
  [SigninEarlGrey signinAndEnableLegacySyncFeature:fakeIdentity];
  [ChromeEarlGrey waitForSyncFeatureEnabled:YES
                                syncTimeout:kSyncOperationTimeout];
  [ChromeEarlGrey
      waitForSyncTransportStateActiveWithTimeout:kSyncOperationTimeout];

  // Disable the Passwords data type.
  [self disableTypeForSyncTheFeature:kSyncPasswordsIdentifier];

  // Save a password. Since the type is disabled, this is local and won't be
  // migrated to the account store.
  password_manager_test_utils::SavePasswordFormToProfileStore();

  // Also create a bookmark and wait for it to arrive on the server.
  [BookmarkEarlGrey addBookmarkWithTitle:kBookmarkTitle
                                     URL:kBookmarkUrl
                               inStorage:BookmarkStorageType::kLocalOrSyncable];
  WaitForEntitiesOnFakeServer(1, syncer::BOOKMARKS);

  // Restart Chrome with UNO phase 3 (i.e. the migration) enabled.
  [self relaunchWithIdentity:fakeIdentity
             enabledFeatures:{switches::kMigrateSyncingUserToSignedIn}
            disabledFeatures:{}];
  // Sync-the-feature should *not* be enabled anymore.
  [ChromeEarlGrey waitForSyncFeatureEnabled:NO
                                syncTimeout:kSyncOperationTimeout];

  // The password should NOT have been migrated to the account store.
  GREYAssertEqual(
      1, [PasswordSettingsAppInterface passwordProfileStoreResultsCount],
      @"Password should still be in the profile store");
  GREYAssertEqual(
      0, [PasswordSettingsAppInterface passwordAccountStoreResultsCount],
      @"Password should NOT be in the account store");

  // The bookmark should have been migrated to the account store.
  [BookmarkEarlGrey
      verifyAbsenceOfBookmarkWithURL:kBookmarkUrl
                           inStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGrey
      verifyExistenceOfBookmarkWithURL:kBookmarkUrl
                                  name:kBookmarkTitle
                             inStorage:BookmarkStorageType::kAccount];

  // Open settings and tap "Sign Out".
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI
      tapSettingsMenuButton:chrome_test_util::SettingsAccountButton()];
  [[[EarlGrey selectElementWithMatcher:
                  grey_accessibilityLabel(l10n_util::GetNSString(
                      IDS_IOS_GOOGLE_ACCOUNT_SETTINGS_SIGN_OUT_ITEM))]
         usingSearchAction:grey_swipeSlowInDirection(kGREYDirectionUp)
      onElementWithMatcher:grey_accessibilityID(
                               kManageSyncTableViewAccessibilityIdentifier)]
      performAction:grey_tap()];

  // Ensure the confirmation dialog is shown (this happens only for migrated
  // managed users!), and does *not* have the "Keep Data" option.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_SIGNOUT_DIALOG_KEEP_DATA_BUTTON)]
      assertWithMatcher:grey_nil()];
  // Confirm "Clear Data".
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_SIGNOUT_DIALOG_CLEAR_DATA_BUTTON)]
      performAction:grey_tap()];

  // Wait until the user is signed out. Use a longer timeout for cases where
  // sign out also triggers a clear browsing data.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:chrome_test_util::SettingsDoneButton()
                                  timeout:base::test::ios::
                                              kWaitForClearBrowsingDataTimeout];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsDoneButton()]
      performAction:grey_tap()];
  [SigninEarlGrey verifySignedOut];

  [ChromeEarlGrey waitForSyncEngineInitialized:NO
                                   syncTimeout:kSyncOperationTimeout];

  // Both the bookmark and the password should be gone.
  GREYAssertEqual(
      0, [PasswordSettingsAppInterface passwordProfileStoreResultsCount],
      @"Password should NOT be in the profile store anymore");
  GREYAssertEqual(
      0, [PasswordSettingsAppInterface passwordAccountStoreResultsCount],
      @"Password should NOT be in the account store");

  [BookmarkEarlGrey
      verifyAbsenceOfBookmarkWithURL:kBookmarkUrl
                           inStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGrey
      verifyAbsenceOfBookmarkWithURL:kBookmarkUrl
                           inStorage:BookmarkStorageType::kAccount];
}

- (void)testManagedAccountClearsDataForSignedInPeriod {
  const GURL preSigninURL = self.testServer->GetURL("/console.html");
  const GURL firstSigninURL = self.testServer->GetURL("/pony.html");
  const GURL secondSigninURL = self.testServer->GetURL("/destination.html");
  const GURL thirdSigninURL = self.testServer->GetURL("/links.html");

  // Clear browsing history before and after the test to avoid conflicting with
  // other tests.
  [ChromeEarlGrey clearBrowsingHistory];
  [self setTearDownHandler:^{
    [ChromeEarlGrey clearBrowsingHistory];
  }];

  GREYAssertEqual([ChromeEarlGrey browsingHistoryEntryCount], 0,
                  @"History was unexpectedly not empty");

  // Save a password to the local store and visit a URL before sign-in.
  password_manager_test_utils::SavePasswordFormToProfileStore(
      @"password1", @"user1", @"https://example.com");
  [ChromeEarlGrey loadURL:preSigninURL];
  GREYAssertEqual([ChromeEarlGrey browsingHistoryEntryCount], 1,
                  @"History was unexpectedly empty");

  // Still before signing in, open a second tab.
  [ChromeEarlGrey openNewTab];
  GREYAssertEqual([ChromeEarlGrey mainTabCount], 2,
                  @"Tabs left behind from previous test?!");

  // Sign in a managed (aka enterprise) account.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeManagedIdentity];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];

  // Save another password to the local store after sign-in.
  password_manager_test_utils::SavePasswordFormToProfileStore(
      @"password2", @"user2", @"https://example.com");

  // Navigate to a few URLs.
  [ChromeEarlGrey loadURL:firstSigninURL];
  [ChromeEarlGrey loadURL:secondSigninURL];
  [ChromeEarlGrey loadURL:thirdSigninURL];
  GREYAssertEqual([ChromeEarlGrey browsingHistoryEntryCount], 4,
                  @"History did not contain the expected entries");

  // Open settings and tap "Sign Out".
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI
      tapSettingsMenuButton:chrome_test_util::SettingsAccountButton()];
  [[[EarlGrey selectElementWithMatcher:
                  grey_accessibilityLabel(l10n_util::GetNSString(
                      IDS_IOS_GOOGLE_ACCOUNT_SETTINGS_SIGN_OUT_ITEM))]
         usingSearchAction:grey_swipeSlowInDirection(kGREYDirectionUp)
      onElementWithMatcher:grey_accessibilityID(
                               kManageSyncTableViewAccessibilityIdentifier)]
      performAction:grey_tap()];

  // Confirm "Sign Out" when alert dialog that data will be cleared is shown.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_SIGNOUT_DIALOG_SIGN_OUT_BUTTON)]
      performAction:grey_tap()];

  // Wait until the user is signed out. Use a longer timeout to give time for
  // data to be cleared.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:chrome_test_util::SettingsDoneButton()
                                  timeout:base::test::ios::
                                              kWaitForClearBrowsingDataTimeout];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsDoneButton()]
      performAction:grey_tap()];
  [SigninEarlGrey verifySignedOut];

  // Only the password saved before sign-in should be remaining.
  GREYAssertEqual(
      1, [PasswordSettingsAppInterface passwordProfileStoreResultsCount],
      @"Only the password saved BEFORE sign-in should be in the profile store");
  GREYAssertEqual(
      0, [PasswordSettingsAppInterface passwordAccountStoreResultsCount],
      @"Password should NOT be in the account store");

  // Only two history entries remain after browsing history is cleared: the one
  // from before sign-in and the active URL.
  // Do one more navigation to ensure everything's in a settled state, bringing
  // the total count to 3.
  [ChromeEarlGrey loadURL:secondSigninURL];
  GREYAssertEqual([ChromeEarlGrey browsingHistoryEntryCount], 3,
                  @"History did not contain the expected entries");

  // Both tabs should still be there.
  GREYAssertEqual([ChromeEarlGrey mainTabCount], 2,
                  @"Tab was unexpectedly closed");
}

- (void)testManagedAccountClearsDataAndTabsForSignedInPeriod {
  const GURL preSigninURL = self.testServer->GetURL("/console.html");
  const GURL firstSigninURL = self.testServer->GetURL("/pony.html");
  const GURL secondSigninURL = self.testServer->GetURL("/destination.html");
  const GURL thirdSigninURL = self.testServer->GetURL("/links.html");

  // Clear browsing history before and after the test to avoid conflicting with
  // other tests.
  [ChromeEarlGrey clearBrowsingHistory];
  [self setTearDownHandler:^{
    [ChromeEarlGrey clearBrowsingHistory];
  }];

  GREYAssertEqual([ChromeEarlGrey browsingHistoryEntryCount], 0,
                  @"History was unexpectedly not empty");

  // Save a password to the local store and visit a URL before sign-in.
  password_manager_test_utils::SavePasswordFormToProfileStore(
      @"password1", @"user1", @"https://example.com");
  [ChromeEarlGrey loadURL:preSigninURL];
  GREYAssertEqual([ChromeEarlGrey browsingHistoryEntryCount], 1,
                  @"History was unexpectedly empty");

  // Still before signing in, open a second tab.
  [ChromeEarlGrey openNewTab];
  GREYAssertEqual([ChromeEarlGrey mainTabCount], 2,
                  @"Tabs left behind from previous test?!");

  // Sign in a managed (aka enterprise) account.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeManagedIdentity];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];

  // Save another password to the local store after sign-in.
  password_manager_test_utils::SavePasswordFormToProfileStore(
      @"password2", @"user2", @"https://example.com");

  // Navigate to a few URLs (in the second tab). This also marks the tab as
  // "used since signin".
  [ChromeEarlGrey loadURL:firstSigninURL];
  [ChromeEarlGrey loadURL:secondSigninURL];
  [ChromeEarlGrey loadURL:thirdSigninURL];
  GREYAssertEqual([ChromeEarlGrey browsingHistoryEntryCount], 4,
                  @"History did not contain the expected entries");

  // Signout doesn't close the current tab. Switch to the first tab, so the
  // second tab can close.
  [ChromeEarlGrey selectTabAtIndex:0];

  // Open settings and tap "Sign Out".
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI
      tapSettingsMenuButton:chrome_test_util::SettingsAccountButton()];
  [[[EarlGrey selectElementWithMatcher:
                  grey_accessibilityLabel(l10n_util::GetNSString(
                      IDS_IOS_GOOGLE_ACCOUNT_SETTINGS_SIGN_OUT_ITEM))]
         usingSearchAction:grey_swipeSlowInDirection(kGREYDirectionUp)
      onElementWithMatcher:grey_accessibilityID(
                               kManageSyncTableViewAccessibilityIdentifier)]
      performAction:grey_tap()];

  // Confirm "Sign Out" when alert dialog that data will be cleared is shown.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_SIGNOUT_DIALOG_SIGN_OUT_BUTTON)]
      performAction:grey_tap()];

  // Wait until the user is signed out. Use a longer timeout to give time for
  // data to be cleared.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:chrome_test_util::SettingsDoneButton()
                                  timeout:base::test::ios::
                                              kWaitForClearBrowsingDataTimeout];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsDoneButton()]
      performAction:grey_tap()];
  [SigninEarlGrey verifySignedOut];

  // Only the password saved before sign-in should be remaining.
  GREYAssertEqual(
      1, [PasswordSettingsAppInterface passwordProfileStoreResultsCount],
      @"Only the password saved BEFORE sign-in should be in the profile store");
  GREYAssertEqual(
      0, [PasswordSettingsAppInterface passwordAccountStoreResultsCount],
      @"Password should NOT be in the account store");

  // Only one history entry should remain after browsing history is cleared: the
  // one from before sign-in.
  GREYAssertEqual([ChromeEarlGrey browsingHistoryEntryCount], 1,
                  @"History did not contain the expected entries");

  // The original tab (not used since signing in) should still be there. The
  // second tab, where we navigated while signed in, should have been closed.
  GREYAssertEqual([ChromeEarlGrey mainTabCount], 1,
                  @"Tab wasn't closed as expected");
}

- (void)testMigrateSyncToSignin_Undo {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  // Sign in and turn on Sync-the-feature.
  [SigninEarlGrey signinAndEnableLegacySyncFeature:fakeIdentity];
  [ChromeEarlGrey waitForSyncFeatureEnabled:YES
                                syncTimeout:kSyncOperationTimeout];
  [ChromeEarlGrey
      waitForSyncTransportStateActiveWithTimeout:kSyncOperationTimeout];

  // Create some data and wait for it to arrive on the server.
  [BookmarkEarlGrey addBookmarkWithTitle:kBookmarkTitle
                                     URL:kBookmarkUrl
                               inStorage:BookmarkStorageType::kLocalOrSyncable];
  password_manager_test_utils::SavePasswordFormToProfileStore();

  WaitForEntitiesOnFakeServer(1, syncer::BOOKMARKS);
  WaitForEntitiesOnFakeServer(1, syncer::PASSWORDS);

  // Restart Chrome with UNO phase 3 (i.e. the migration) enabled.
  [self relaunchWithIdentity:fakeIdentity
             enabledFeatures:{switches::kMigrateSyncingUserToSignedIn}
            disabledFeatures:{}];
  // Sync-the-feature should *not* be enabled anymore.
  [ChromeEarlGrey waitForSyncFeatureEnabled:NO
                                syncTimeout:kSyncOperationTimeout];

  // The bookmark should still exist, but now be in the account store.
  [BookmarkEarlGrey
      verifyAbsenceOfBookmarkWithURL:kBookmarkUrl
                           inStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGrey
      verifyExistenceOfBookmarkWithURL:kBookmarkUrl
                                  name:kBookmarkTitle
                             inStorage:BookmarkStorageType::kAccount];
  // Similarly the password.
  GREYAssertEqual(
      0, [PasswordSettingsAppInterface passwordProfileStoreResultsCount],
      @"Password should NOT be in the profile store");
  GREYAssertEqual(
      1, [PasswordSettingsAppInterface passwordAccountStoreResultsCount],
      @"Password should be in the account store");

  // Restart Chrome with the reverse migration (undo) enabled.
  [self relaunchWithIdentity:fakeIdentity
             enabledFeatures:{switches::kUndoMigrationOfSyncingUserToSignedIn}
            disabledFeatures:{}];
  // Sync-the-feature should be enabled again.
  [ChromeEarlGrey waitForSyncFeatureEnabled:YES
                                syncTimeout:kSyncOperationTimeout];

  // The bookmark should be back in the local store.
  [BookmarkEarlGrey
      verifyExistenceOfBookmarkWithURL:kBookmarkUrl
                                  name:kBookmarkTitle
                             inStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGrey
      verifyAbsenceOfBookmarkWithURL:kBookmarkUrl
                           inStorage:BookmarkStorageType::kAccount];
  // Similarly the password.
  GREYAssertEqual(
      1, [PasswordSettingsAppInterface passwordProfileStoreResultsCount],
      @"Password should be back in the profile store");
  GREYAssertEqual(
      0, [PasswordSettingsAppInterface passwordAccountStoreResultsCount],
      @"Password should NOT be in the account store anymore");

  // Verify that the local-or-syncable store is the one being synced again: Add
  // another bookmark (to the local store) and ensure it arrives on the server.
  [BookmarkEarlGrey addBookmarkWithTitle:@"Other title"
                                     URL:@"https://other.url.com"
                               inStorage:BookmarkStorageType::kLocalOrSyncable];
  WaitForEntitiesOnFakeServer(2, syncer::BOOKMARKS);
}

@end
