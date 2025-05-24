// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "base/files/file_util.h"
#import "base/ios/ios_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/time/time.h"
#import "components/signin/public/base/signin_switches.h"
#import "components/sync/base/command_line_switches.h"
#import "components/sync/base/features.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin_earl_grey.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/test_switches.h"
#import "ios/chrome/test/earl_grey/web_http_server_chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"

namespace {

const char* kTestSyncablePref = prefs::kNetworkPredictionSetting;
const int kTestPrefValue1 = 1001;  // Some random value.
const int kTestPrefValue2 = 1002;  // Some random value.

// Constant for timeout while waiting for asynchronous sync operations.
constexpr base::TimeDelta kSyncOperationTimeout = base::Seconds(20);

// Waits for `entity_count` entities of PREFERENCE type on the fake server,
// and fails with a GREYAssert if the condition is not met, within a short
// period of time.
void WaitForTestPreferenceOnFakeServer(bool present) {
  [ChromeEarlGrey waitForSyncServerEntitiesWithType:syncer::PREFERENCES
                                               name:kTestSyncablePref
                                              count:static_cast<int>(present)
                                            timeout:kSyncOperationTimeout];
}

// Waits for the active pref value to become `pref_value` and fails with a
// GREYAssert if the condition is not met, within a short period of time.
void WaitForPreferenceValue(int pref_value) {
  GREYAssertTrue(base::test::ios::WaitUntilConditionOrTimeout(
                     kSyncOperationTimeout,
                     ^{
                       return
                           [ChromeEarlGrey userIntegerPref:kTestSyncablePref] ==
                           pref_value;
                     }),
                 @"Expected preference to be present.");
}

}  // namespace

@interface SyncPreferencesTestCase : WebHttpServerChromeTestCase
@end

@implementation SyncPreferencesTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  config.additional_args.push_back(std::string("--") +
                                   syncer::kSyncShortNudgeDelayForTest);
  config.features_enabled.push_back(switches::kEnablePreferencesAccountStorage);
  return config;
}

- (void)setUp {
  [super setUp];

  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");

  [ChromeEarlGrey clearFakeSyncServerData];
}

- (void)tearDownHelper {
  [ChromeEarlGrey clearUserPrefWithName:kTestSyncablePref];
  [ChromeEarlGrey clearFakeSyncServerData];
  [super tearDownHelper];
}

#pragma mark - SyncPreferencesTestCase Tests

// Tests that the local pref value is not uploaded to the account.
- (void)testLocalPrefValueNotUploadedToAccountOnSignIn {
  [ChromeEarlGrey setIntegerValue:kTestPrefValue1
                      forUserPref:kTestSyncablePref];

  // Sign in and sign out.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];
  // Pref is not committed to the server.
  WaitForTestPreferenceOnFakeServer(false);
  [SigninEarlGrey signOut];

  GREYAssertEqual([ChromeEarlGrey userIntegerPref:kTestSyncablePref],
                  kTestPrefValue1, @"Incorrect local pref value.");

  // Remove from local store.
  [ChromeEarlGrey clearUserPrefWithName:kTestSyncablePref];

  // Sign in again to validate the value is not set from the server.
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];
  GREYAssertNotEqual([ChromeEarlGrey userIntegerPref:kTestSyncablePref],
                     kTestPrefValue1, @"Incorrect account pref value.");
}

// Tests that the value is written to local and account when signed in.
- (void)testPrefWrittenToLocalAndAccountIfSignedIn {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];
  [ChromeEarlGrey
      waitForSyncTransportStateActiveWithTimeout:kSyncOperationTimeout];

  // Pref does not exist on the server.
  WaitForTestPreferenceOnFakeServer(false);

  [ChromeEarlGrey setIntegerValue:kTestPrefValue1
                      forUserPref:kTestSyncablePref];

  // Preference is committed to the server.
  WaitForTestPreferenceOnFakeServer(true);

  [SigninEarlGrey signOut];

  // Pref value is set locally.
  GREYAssertEqual([ChromeEarlGrey userIntegerPref:kTestSyncablePref],
                  kTestPrefValue1, @"Incorrect local pref value.");

  // Remove from local store.
  [ChromeEarlGrey clearUserPrefWithName:kTestSyncablePref];

  // Sign in again to validate the value was set in the server.
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];
  WaitForPreferenceValue(kTestPrefValue1);
}

// Tests that the account pref value is removed on signout and the local pref
// value takes effect.
- (void)testAccountPrefValueRemovedOnSignout {
  // Set a pref value of `kTestPrefValue2` in account.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];

  [ChromeEarlGrey setIntegerValue:kTestPrefValue2
                      forUserPref:kTestSyncablePref];
  WaitForTestPreferenceOnFakeServer(true);
  [SigninEarlGrey signOut];

  // Reset local value to `kTestPrefValue1`.
  [ChromeEarlGrey setIntegerValue:kTestPrefValue1
                      forUserPref:kTestSyncablePref];

  // Local pref value is now `kTestPrefValue1` and the account value is
  // `kTestPrefValue2`.

  // Sign in and sync.
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];
  WaitForPreferenceValue(kTestPrefValue2);

  // Sign out and validate that the active pref value is the local value.
  [SigninEarlGrey signOut];
  WaitForPreferenceValue(kTestPrefValue1);
}

@end

@interface SyncPreferencesWithMigrateAccountPrefsBaseTestCase
    : WebHttpServerChromeTestCase
@end

@implementation SyncPreferencesWithMigrateAccountPrefsBaseTestCase

- (void)setUp {
  [super setUp];
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
  [ChromeEarlGrey clearFakeSyncServerData];
}

- (void)tearDownHelper {
  [ChromeEarlGrey clearUserPrefWithName:kTestSyncablePref];
  [ChromeEarlGrey clearFakeSyncServerData];
  [super tearDownHelper];
}

- (void)restartWithMigrateAccountPrefsEnabled:(FakeSystemIdentity*)identity {
  // Before restarting, ensure that the FakeServer has written all its pending
  // state to disk.
  [ChromeEarlGrey flushFakeSyncServerToDisk];
  // Also make sure any pending prefs changes are written to disk.
  [ChromeEarlGrey commitPendingUserPrefsWrite];

  AppLaunchConfiguration config = [self appConfigurationForTestCase];
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  config.features_enabled.push_back(syncer::kMigrateAccountPrefs);
  config.additional_args.push_back(base::StrCat({
    "-", test_switches::kAddFakeIdentitiesAtStartup, "=",
        [FakeSystemIdentity encodeIdentitiesToBase64:@[ identity ]]
  }));
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];
}

- (void)restartWithMigrateAccountPrefsDisabled:(FakeSystemIdentity*)identity {
  // Before restarting, ensure that the FakeServer has written all its pending
  // state to disk.
  [ChromeEarlGrey flushFakeSyncServerToDisk];
  // Also make sure any pending prefs changes are written to disk.
  [ChromeEarlGrey commitPendingUserPrefsWrite];

  AppLaunchConfiguration config = [self appConfigurationForTestCase];
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  config.features_disabled.push_back(syncer::kMigrateAccountPrefs);
  config.additional_args.push_back(base::StrCat({
    "-", test_switches::kAddFakeIdentitiesAtStartup, "=",
        [FakeSystemIdentity encodeIdentitiesToBase64:@[ identity ]]
  }));
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];
}

- (void)setTestSyncablePrefValueTo:(int)pref_value
                   forFakeIdentity:(FakeSystemIdentity*)fakeIdentity {
  // Sign in and set the pref value.
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];
  [ChromeEarlGrey setIntegerValue:pref_value forUserPref:kTestSyncablePref];
  WaitForTestPreferenceOnFakeServer(true);
  [SigninEarlGrey signOut];

  // Remove from local store.
  [ChromeEarlGrey clearUserPrefWithName:kTestSyncablePref];
}

@end

@interface SyncPreferencesWithMigrateAccountPrefsEnabledTestCase
    : SyncPreferencesWithMigrateAccountPrefsBaseTestCase
@end

@implementation SyncPreferencesWithMigrateAccountPrefsEnabledTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  config.features_enabled.push_back(syncer::kMigrateAccountPrefs);
  return config;
}

#pragma mark - SyncPreferencesWithMigrateAccountPrefsEnabledTestCase Tests

- (void)testAccountPrefsDownloadedWithInitialSync {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  // Set a pref value of `kTestPrefValue1` in account.
  [self setTestSyncablePrefValueTo:kTestPrefValue1
                   forFakeIdentity:fakeIdentity];

  [self restartWithMigrateAccountPrefsEnabled:fakeIdentity];

  // Sign in and sync.
  WaitForTestPreferenceOnFakeServer(true);
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];
  WaitForPreferenceValue(kTestPrefValue1);

  // Sign out and validate that the pref is not set locally.
  [SigninEarlGrey signOut];
  GREYAssertNotEqual([ChromeEarlGrey userIntegerPref:kTestSyncablePref],
                     kTestPrefValue1, @"Incorrect local pref value.");
}

- (void)testAccountPrefsPersisted {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  // Set a pref value of `kTestPrefValue1` in account.
  [self setTestSyncablePrefValueTo:kTestPrefValue1
                   forFakeIdentity:fakeIdentity];

  // Sign in and sync.
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];
  WaitForPreferenceValue(kTestPrefValue1);

  // Restart.
  [self restartWithMigrateAccountPrefsEnabled:fakeIdentity];
  GREYAssertEqual([ChromeEarlGrey userIntegerPref:kTestSyncablePref],
                  kTestPrefValue1, @"Incorrect local pref value.");
}

- (void)testDisablingFlag {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];

  // Set a pref value of `kTestPrefValue1` in account.
  [self setTestSyncablePrefValueTo:kTestPrefValue1
                   forFakeIdentity:fakeIdentity];

  // Sign in and sync.
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];
  WaitForPreferenceValue(kTestPrefValue1);

  // Restart with MigrateAccountPrefs flag disabled.
  [self restartWithMigrateAccountPrefsDisabled:fakeIdentity];
  GREYAssertEqual([ChromeEarlGrey userIntegerPref:kTestSyncablePref],
                  kTestPrefValue1, @"Incorrect local pref value.");

  // Sign out and validate that the pref is not set locally.
  [SigninEarlGrey signOut];
  GREYAssertNotEqual([ChromeEarlGrey userIntegerPref:kTestSyncablePref],
                     kTestPrefValue1, @"Incorrect local pref value.");
}

@end

@interface SyncPreferencesWithMigrateAccountPrefsDisabledTestCase
    : SyncPreferencesWithMigrateAccountPrefsBaseTestCase
@end

@implementation SyncPreferencesWithMigrateAccountPrefsDisabledTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  config.features_disabled.push_back(syncer::kMigrateAccountPrefs);
  return config;
}

#pragma mark - SyncPreferencesWithMigrateAccountPrefsDisabledTestCase Tests

- (void)testEnablingFlag {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  // Set a pref value of `kTestPrefValue1` in account.
  [self setTestSyncablePrefValueTo:kTestPrefValue1
                   forFakeIdentity:fakeIdentity];

  // Sign in and sync.
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];
  WaitForPreferenceValue(kTestPrefValue1);

  // Restart with MigrateAccountPrefs flag enabled.
  [self restartWithMigrateAccountPrefsEnabled:fakeIdentity];
  GREYAssertEqual([ChromeEarlGrey userIntegerPref:kTestSyncablePref],
                  kTestPrefValue1, @"Incorrect local pref value.");

  // Sign out and validate that the pref is not set locally.
  [SigninEarlGrey signOut];
  GREYAssertNotEqual([ChromeEarlGrey userIntegerPref:kTestSyncablePref],
                     kTestPrefValue1, @"Incorrect local pref value.");
}

- (void)testAccountPrefsDownloadedFromSyncMetadataIfFlagEnabled {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  // Set a pref value of `kTestPrefValue1` in account.
  [self setTestSyncablePrefValueTo:kTestPrefValue1
                   forFakeIdentity:fakeIdentity];

  // Sign in and sync.
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];
  WaitForPreferenceValue(kTestPrefValue1);

  // Restart with MigrateAccountPrefs flag enabled.
  [self restartWithMigrateAccountPrefsEnabled:fakeIdentity];
  // The account values are loaded upon sync initialization, thus wait for the
  // pref value to be set.
  WaitForPreferenceValue(kTestPrefValue1);

  // Sign out and validate that the pref is not set locally.
  [SigninEarlGrey signOut];
  GREYAssertNotEqual([ChromeEarlGrey userIntegerPref:kTestSyncablePref],
                     kTestPrefValue1, @"Incorrect local pref value.");
}

@end
