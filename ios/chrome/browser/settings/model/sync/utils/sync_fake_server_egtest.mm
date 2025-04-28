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
#import "ios/chrome/browser/authentication/ui_bundled/signin_earl_grey.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_storage_type.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/bookmark_earl_grey.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/reading_list/ui_bundled/reading_list_app_interface.h"
#import "ios/chrome/browser/reading_list/ui_bundled/reading_list_egtest_utils.h"
#import "ios/chrome/browser/settings/ui_bundled/google_services/manage_sync_settings_constants.h"
#import "ios/chrome/browser/settings/ui_bundled/password/password_manager_egtest_utils.h"
#import "ios/chrome/browser/settings/ui_bundled/password/password_settings_app_interface.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_navigation_controller_constants.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
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

- (void)tearDownHelper {
  ClearRelevantData();

  [super tearDownHelper];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  config.additional_args.push_back(std::string("--") +
                                   syncer::kSyncShortNudgeDelayForTest);

  if ([self isRunningTest:@selector
            (testManagedAccountClearsDataForSignedInPeriod)]) {
    config.features_disabled.push_back(kIdentityDiscAccountMenu);
    // When kSeparateProfilesForManagedAccounts is enabled, there will be no
    // need to show the data-delete dialog.
    config.features_disabled.push_back(kSeparateProfilesForManagedAccounts);
  }
  if ([self isRunningTest:@selector
            (testManagedAccountClearsDataAndTabsForSignedInPeriod)]) {
    config.features_enabled.push_back(kIdentityDiscAccountMenu);
    // When kSeparateProfilesForManagedAccounts is enabled, there will be no
    // need to show the data-delete dialog.
    config.features_disabled.push_back(kSeparateProfilesForManagedAccounts);
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

// Tests that a bookmark added on the client is uploaded to the Sync server.
- (void)testSyncUploadBookmark {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];

  // Add a bookmark after sync is active.
  [ChromeEarlGrey
      waitForSyncTransportStateActiveWithTimeout:kSyncOperationTimeout];
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
  std::string original_guid = [ChromeEarlGrey syncCacheGUID];

  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
  [SigninEarlGrey signOut];

  // Sign the user back in, and verify the guid has *not* changed.
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];
  GREYAssertTrue([ChromeEarlGrey syncCacheGUID] == original_guid,
                 @"guid changed after user signed out and signed back in");
}

// Tests that the local cache guid changes when the user signs out and then
// signs back in with a different account.
- (void)testSyncCheckDifferentCacheGuid_SignOutAndSignInWithDifferentAccount {
  // Sign in a fake identity, and store the initial sync guid.
  FakeSystemIdentity* fakeIdentity1 = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity1];
  std::string original_guid = [ChromeEarlGrey syncCacheGUID];

  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity1];
  [SigninEarlGrey signOut];

  // Sign a different user in, and verify the guid has changed.
  FakeSystemIdentity* fakeIdentity2 = [FakeSystemIdentity fakeIdentity2];
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity2];
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
  [ChromeEarlGrey
      waitForSyncTransportStateActiveWithTimeout:kSyncOperationTimeout];
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

  if (![ChromeTestCase forceRestartAndWipe]) {
    [ChromeEarlGrey clearBrowsingHistory];
    [self setTearDownHandler:^{
      [ChromeEarlGrey clearBrowsingHistory];
    }];
  }

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

  if (![ChromeTestCase forceRestartAndWipe]) {
    [ChromeEarlGrey clearBrowsingHistory];
    [self setTearDownHandler:^{
      [ChromeEarlGrey clearBrowsingHistory];
    }];
  }

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

  [ChromeEarlGrey
      waitForSyncTransportStateActiveWithTimeout:kSyncOperationTimeout];
  WaitForEntitiesOnFakeServer(1, syncer::DEVICE_INFO);
  [ChromeEarlGrey waitForSyncInvalidationFields];
}

- (void)testManagedAccountClearsDataForSignedInPeriod {
  const GURL preSigninURL = self.testServer->GetURL("/console.html");
  const GURL firstSigninURL = self.testServer->GetURL("/pony.html");
  const GURL secondSigninURL = self.testServer->GetURL("/destination.html");
  const GURL thirdSigninURL = self.testServer->GetURL("/links.html");

  // Clear browsing history before and after the test to avoid conflicting with
  // other tests.
  if (![ChromeTestCase forceRestartAndWipe]) {
    [ChromeEarlGrey clearBrowsingHistory];
    [self setTearDownHandler:^{
      [ChromeEarlGrey clearBrowsingHistory];
    }];
  }

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
  GREYAssertEqual([ChromeEarlGrey mainTabCount], 2UL,
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
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ButtonWithAccessibilityLabelId(
                     IDS_IOS_SIGNOUT_AND_DELETE_DIALOG_SIGN_OUT_BUTTON)]
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
  GREYAssertEqual([ChromeEarlGrey mainTabCount], 2UL,
                  @"Tab was unexpectedly closed");
}

- (void)testManagedAccountClearsDataAndTabsForSignedInPeriod {
  const GURL preSigninURL = self.testServer->GetURL("/console.html");
  const GURL firstSigninURL = self.testServer->GetURL("/pony.html");
  const GURL secondSigninURL = self.testServer->GetURL("/destination.html");
  const GURL thirdSigninURL = self.testServer->GetURL("/links.html");

  // Clear browsing history before and after the test to avoid conflicting with
  // other tests.
  if (![ChromeTestCase forceRestartAndWipe]) {
    [ChromeEarlGrey clearBrowsingHistory];
    [self setTearDownHandler:^{
      [ChromeEarlGrey clearBrowsingHistory];
    }];
  }

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
  GREYAssertEqual([ChromeEarlGrey mainTabCount], 2UL,
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
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ButtonWithAccessibilityLabelId(
                     IDS_IOS_SIGNOUT_AND_DELETE_DIALOG_SIGN_OUT_BUTTON)]
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
  GREYAssertEqual([ChromeEarlGrey mainTabCount], 1UL,
                  @"Tab wasn't closed as expected");
}

@end
