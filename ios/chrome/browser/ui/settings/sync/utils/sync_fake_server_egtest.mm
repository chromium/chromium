// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <XCTest/XCTest.h>

#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/titled_url_match.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/base/model_type.h"
#include "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#include "ios/chrome/browser/bookmarks/bookmarks_utils.h"
#include "ios/chrome/browser/signin/authentication_service.h"
#include "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui.h"
#import "ios/chrome/browser/ui/authentication/signin_earlgrey_utils.h"
#import "ios/chrome/browser/ui/settings/settings_table_view_controller.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service.h"
#import "ios/web/public/test/http_server/http_server.h"
#include "ios/web/public/test/http_server/http_server_util.h"
#import "net/base/mac/url_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::SettingsDoneButton;

namespace {

// Constant for timeout while waiting for asynchronous sync operations.
const NSTimeInterval kSyncOperationTimeout = 10.0;

// Waits for |entity_count| entities of type |entity_type|, and fails with
// a GREYAssert if the condition is not met, within a short period of time.
void AssertNumberOfEntities(int entity_count, syncer::ModelType entity_type) {
  ConditionBlock condition = ^{
    return [ChromeEarlGrey numberOfSyncEntitiesWithType:entity_type] ==
           entity_count;
  };
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(kSyncOperationTimeout,
                                                          condition),
             @"Expected %d entities of the specified type", entity_count);
}

}  // namespace

// Hermetic sync tests, which use the fake sync server.
@interface SyncFakeServerTestCase : ChromeTestCase
@end

@implementation SyncFakeServerTestCase

- (void)tearDown {
  [ChromeEarlGrey waitForBookmarksToFinishLoading];
  [ChromeEarlGrey clearBookmarks];

  [ChromeEarlGrey clearSyncServerData];
  AssertNumberOfEntities(0, syncer::AUTOFILL_PROFILE);
  [super tearDown];
}

- (void)setUp {
  [super setUp];
  GREYAssertEqual(
      [ChromeEarlGrey numberOfSyncEntitiesWithType:syncer::BOOKMARKS], 0,
      @"No bookmarks should exist before sync tests start.");
  GREYAssertEqual(
      [ChromeEarlGrey numberOfSyncEntitiesWithType:syncer::TYPED_URLS], 0,
      @"No bookmarks should exist before sync tests start.");
}

// Tests that a bookmark added on the client (before Sync is enabled) is
// uploaded to the Sync server once Sync is turned on.
- (void)testSyncUploadBookmarkOnFirstSync {
  [self addBookmark:GURL("https://www.foo.com") withTitle:@"foo"];

  // Sign in to sync, after a bookmark has been added.
  ChromeIdentity* identity = [SigninEarlGreyUtils fakeIdentity1];
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()->AddIdentity(
      identity);
  [SigninEarlGreyUI signinWithIdentity:identity];

  // Assert that the correct number of bookmarks have been synced.
  [ChromeEarlGrey waitForSyncInitialized:YES syncTimeout:kSyncOperationTimeout];
  AssertNumberOfEntities(1, syncer::BOOKMARKS);
}

// Tests that a bookmark added on the client is uploaded to the Sync server.
- (void)testSyncUploadBookmark {
  ChromeIdentity* identity = [SigninEarlGreyUtils fakeIdentity1];
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()->AddIdentity(
      identity);
  [SigninEarlGreyUI signinWithIdentity:identity];

  // Add a bookmark after sync is initialized.
  [ChromeEarlGrey waitForSyncInitialized:YES syncTimeout:kSyncOperationTimeout];
  [self addBookmark:GURL("https://www.goo.com") withTitle:@"goo"];
  AssertNumberOfEntities(1, syncer::BOOKMARKS);
}

// Tests that a bookmark injected in the FakeServer is synced down to the
// client.
- (void)testSyncDownloadBookmark {
  [[self class] assertBookmarksWithTitle:@"hoo" expectedCount:0];
  const GURL URL = web::test::HttpServer::MakeUrl("http://www.hoo.com");
  [ChromeEarlGrey addFakeSyncServerBookmarkWithURL:URL title:"hoo"];

  // Sign in to sync, after a bookmark has been injected in the sync server.
  ChromeIdentity* identity = [SigninEarlGreyUtils fakeIdentity1];
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()->AddIdentity(
      identity);
  [SigninEarlGreyUI signinWithIdentity:identity];
  [ChromeEarlGrey waitForSyncInitialized:YES syncTimeout:kSyncOperationTimeout];
  [[self class] assertBookmarksWithTitle:@"hoo" expectedCount:1];
}

// Tests that the local cache guid does not change when sync is restarted.
- (void)testSyncCheckSameCacheGuid_SyncRestarted {
  // Sign in the fake identity.
  ChromeIdentity* identity = [SigninEarlGreyUtils fakeIdentity1];
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()->AddIdentity(
      identity);
  [SigninEarlGreyUI signinWithIdentity:identity];
  [ChromeEarlGrey waitForSyncInitialized:YES syncTimeout:kSyncOperationTimeout];

  // Store the original guid, then restart sync.
  std::string original_guid = [ChromeEarlGrey syncCacheGUID];
  [ChromeEarlGrey stopSync];
  [ChromeEarlGrey waitForSyncInitialized:NO syncTimeout:kSyncOperationTimeout];
  [ChromeEarlGrey startSync];

  // Verify the guid did not change.
  [ChromeEarlGrey waitForSyncInitialized:YES syncTimeout:kSyncOperationTimeout];
  GREYAssertEqual([ChromeEarlGrey syncCacheGUID], original_guid,
                  @"Stored guid doesn't match current value");
}

// Tests that the local cache guid changes when the user signs out and then
// signs back in with the same account.
- (void)testSyncCheckDifferentCacheGuid_SignOutAndSignIn {
  // Sign in a fake identity, and store the initial sync guid.
  ChromeIdentity* identity = [SigninEarlGreyUtils fakeIdentity1];
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()->AddIdentity(
      identity);
  [SigninEarlGreyUI signinWithIdentity:identity];
  [ChromeEarlGrey waitForSyncInitialized:YES syncTimeout:kSyncOperationTimeout];
  std::string original_guid = [ChromeEarlGrey syncCacheGUID];

  // Sign out the current user.
  ios::ChromeBrowserState* browser_state =
      chrome_test_util::GetOriginalBrowserState();
  AuthenticationService* authentication_service =
      AuthenticationServiceFactory::GetForBrowserState(browser_state);
  GREYAssert(authentication_service->IsAuthenticated(),
             @"User is not signed in.");
  authentication_service->SignOut(signin_metrics::SIGNOUT_TEST, nil);
  [ChromeEarlGrey waitForSyncInitialized:NO syncTimeout:kSyncOperationTimeout];

  // Sign the user back in, and verify the guid has changed.
  [SigninEarlGreyUI signinWithIdentity:identity];
  [ChromeEarlGrey waitForSyncInitialized:YES syncTimeout:kSyncOperationTimeout];
  GREYAssertTrue(
      [ChromeEarlGrey syncCacheGUID] != original_guid,
      @"guid didn't change after user signed out and signed back in");
}

// Tests that the local cache guid does not change when sync is restarted, if
// a user previously signed out and back in.
// Test for http://crbug.com/413611 .
- (void)testSyncCheckSameCacheGuid_SyncRestartedAfterSignOutAndSignIn {
  // Sign in a fake idenitty.
  ChromeIdentity* identity = [SigninEarlGreyUtils fakeIdentity1];
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()->AddIdentity(
      identity);
  [SigninEarlGreyUI signinWithIdentity:identity];
  [ChromeEarlGrey waitForSyncInitialized:YES syncTimeout:kSyncOperationTimeout];

  // Sign out the current user.
  ios::ChromeBrowserState* browser_state =
      chrome_test_util::GetOriginalBrowserState();
  AuthenticationService* authentication_service =
      AuthenticationServiceFactory::GetForBrowserState(browser_state);
  GREYAssert(authentication_service->IsAuthenticated(),
             @"User is not signed in.");
  authentication_service->SignOut(signin_metrics::SIGNOUT_TEST, nil);
  [ChromeEarlGrey waitForSyncInitialized:NO syncTimeout:kSyncOperationTimeout];

  // Sign the user back in.
  [SigninEarlGreyUI signinWithIdentity:identity];
  [ChromeEarlGrey waitForSyncInitialized:YES syncTimeout:kSyncOperationTimeout];

  // Record the initial guid, before restarting sync.
  std::string original_guid = [ChromeEarlGrey syncCacheGUID];
  [ChromeEarlGrey stopSync];
  [ChromeEarlGrey waitForSyncInitialized:NO syncTimeout:kSyncOperationTimeout];
  [ChromeEarlGrey startSync];

  // Verify the guid did not change after restarting sync.
  [ChromeEarlGrey waitForSyncInitialized:YES syncTimeout:kSyncOperationTimeout];
  GREYAssertEqual([ChromeEarlGrey syncCacheGUID], original_guid,
                  @"Stored guid doesn't match current value");
}

// Tests that autofill profile injected in FakeServer gets synced to client.
- (void)testSyncDownloadAutofillProfile {
  const std::string kGuid = "2340E83B-5BEE-4560-8F95-5914EF7F539E";
  const std::string kFullName = "Peter Pan";
  GREYAssertFalse([ChromeEarlGrey isAutofillProfilePresentWithGUID:kGuid
                                               autofillProfileName:kFullName],
                  @"autofill profile should not exist");
  [ChromeEarlGrey injectAutofillProfileOnFakeSyncServerWithGUID:kGuid
                                            autofillProfileName:kFullName];
  [self setTearDownHandler:^{
    [ChromeEarlGrey clearAutofillProfileWithGUID:kGuid];
  }];

  // Sign in to sync.
  ChromeIdentity* identity = [SigninEarlGreyUtils fakeIdentity1];
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()->AddIdentity(
      identity);
  [SigninEarlGreyUI signinWithIdentity:identity];

  // Verify that the autofill profile has been downloaded.
  [ChromeEarlGrey waitForSyncInitialized:YES syncTimeout:kSyncOperationTimeout];
  GREYAssertTrue([ChromeEarlGrey isAutofillProfilePresentWithGUID:kGuid
                                              autofillProfileName:kFullName],
                 @"autofill profile should exist");
}

// Test that update to autofill profile injected in FakeServer gets synced to
// client.
- (void)testSyncUpdateAutofillProfile {
  const std::string kGuid = "2340E83B-5BEE-4560-8F95-5914EF7F539E";
  const std::string kFullName = "Peter Pan";
  const std::string kUpdatedFullName = "Roger Rabbit";
  GREYAssertFalse([ChromeEarlGrey isAutofillProfilePresentWithGUID:kGuid
                                               autofillProfileName:kFullName],
                  @"autofill profile should not exist");

  [ChromeEarlGrey injectAutofillProfileOnFakeSyncServerWithGUID:kGuid
                                            autofillProfileName:kFullName];
  [self setTearDownHandler:^{
    [ChromeEarlGrey clearAutofillProfileWithGUID:kGuid];
  }];

  // Sign in to sync.
  ChromeIdentity* identity = [SigninEarlGreyUtils fakeIdentity1];
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()->AddIdentity(
      identity);
  [SigninEarlGreyUI signinWithIdentity:identity];

  // Verify that the autofill profile has been downloaded.
  [ChromeEarlGrey waitForSyncInitialized:YES syncTimeout:kSyncOperationTimeout];
  GREYAssertTrue([ChromeEarlGrey isAutofillProfilePresentWithGUID:kGuid
                                              autofillProfileName:kFullName],
                 @"autofill profile should exist");

  // Update autofill profile.
  [ChromeEarlGrey
      injectAutofillProfileOnFakeSyncServerWithGUID:kGuid
                                autofillProfileName:kUpdatedFullName];

  // Trigger sync cycle and wait for update.
  [ChromeEarlGrey triggerSyncCycleForType:syncer::AUTOFILL_PROFILE];
  NSString* errorMessage =
      [NSString stringWithFormat:
                    @"Did not find autofill profile for guid: %@, and name: %@",
                    base::SysUTF8ToNSString(kGuid),
                    base::SysUTF8ToNSString(kUpdatedFullName)];
  ConditionBlock condition = ^{
    return [ChromeEarlGrey isAutofillProfilePresentWithGUID:kGuid
                                        autofillProfileName:kUpdatedFullName];
  };
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(kSyncOperationTimeout,
                                                          condition),
             errorMessage);
}

// Test that autofill profile deleted from FakeServer gets deleted from client
// as well.
- (void)testSyncDeleteAutofillProfile {
  const std::string kGuid = "2340E83B-5BEE-4560-8F95-5914EF7F539E";
  const std::string kFullName = "Peter Pan";
  GREYAssertFalse([ChromeEarlGrey isAutofillProfilePresentWithGUID:kGuid
                                               autofillProfileName:kFullName],
                  @"autofill profile should not exist");
  [ChromeEarlGrey injectAutofillProfileOnFakeSyncServerWithGUID:kGuid
                                            autofillProfileName:kFullName];
  [self setTearDownHandler:^{
    [ChromeEarlGrey clearAutofillProfileWithGUID:kGuid];
  }];

  // Sign in to sync.
  ChromeIdentity* identity = [SigninEarlGreyUtils fakeIdentity1];
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()->AddIdentity(
      identity);
  [SigninEarlGreyUI signinWithIdentity:identity];

  // Verify that the autofill profile has been downloaded
  [ChromeEarlGrey waitForSyncInitialized:YES syncTimeout:kSyncOperationTimeout];
  GREYAssertTrue([ChromeEarlGrey isAutofillProfilePresentWithGUID:kGuid
                                              autofillProfileName:kFullName],
                 @"autofill profile should exist");

  // Delete autofill profile from server, and verify it is removed.
  [ChromeEarlGrey deleteAutofillProfileOnFakeSyncServerWithGUID:kGuid];
  [ChromeEarlGrey triggerSyncCycleForType:syncer::AUTOFILL_PROFILE];
  ConditionBlock condition = ^{
    return ![ChromeEarlGrey isAutofillProfilePresentWithGUID:kGuid
                                         autofillProfileName:kFullName];
  };
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(kSyncOperationTimeout,
                                                          condition),
             @"Autofill profile was not deleted.");
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
  ChromeIdentity* identity = [SigninEarlGreyUtils fakeIdentity1];
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()->AddIdentity(
      identity);
  [SigninEarlGreyUI signinWithIdentity:identity];

  // Verify the sessions on the sync server.
  [ChromeEarlGrey waitForSyncInitialized:YES syncTimeout:kSyncOperationTimeout];
  AssertNumberOfEntities(3, syncer::SESSIONS);

  NSArray<NSString*>* specs = @[
    base::SysUTF8ToNSString(URL1.spec()),
    base::SysUTF8ToNSString(URL2.spec()),
  ];
  [ChromeEarlGrey verifySyncServerURLs:specs];
}

// Tests that a typed URL (after Sync is enabled) is uploaded to the Sync
// server.
- (void)testSyncTypedURLUpload {
  const GURL mockURL("http://not-a-real-site/");

  [ChromeEarlGrey clearBrowsingHistory];
  [self setTearDownHandler:^{
    [ChromeEarlGrey clearBrowsingHistory];
  }];
  [ChromeEarlGrey addHistoryServiceTypedURL:mockURL];

  // Sign in to sync.
  ChromeIdentity* identity = [SigninEarlGreyUtils fakeIdentity1];
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()->AddIdentity(
      identity);
  [SigninEarlGreyUI signinWithIdentity:identity];

  [ChromeEarlGrey waitForSyncInitialized:YES syncTimeout:kSyncOperationTimeout];

  // Trigger sync and verify the typed URL is on the fake sync server.
  [ChromeEarlGrey triggerSyncCycleForType:syncer::TYPED_URLS];
  [ChromeEarlGrey waitForSyncServerEntitiesWithType:syncer::TYPED_URLS
                                               name:mockURL.spec()
                                              count:1
                                            timeout:kSyncOperationTimeout];
}

// Tests that typed url is downloaded from sync server.
- (void)testSyncTypedUrlDownload {
  const GURL mockURL("http://not-a-real-site/");

  [ChromeEarlGrey clearBrowsingHistory];
  [self setTearDownHandler:^{
    [ChromeEarlGrey clearBrowsingHistory];
  }];

  // Inject typed url on server.
  [ChromeEarlGrey addFakeSyncServerTypedURL:mockURL];

  // Sign in to sync.
  ChromeIdentity* identity = [SigninEarlGreyUtils fakeIdentity1];
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()->AddIdentity(
      identity);
  [SigninEarlGreyUI signinWithIdentity:identity];

  [ChromeEarlGrey waitForSyncInitialized:YES syncTimeout:kSyncOperationTimeout];

  // Wait for typed url to appear on client.
  [ChromeEarlGrey waitForTypedURL:mockURL
                    expectPresent:YES
                          timeout:kSyncOperationTimeout];
}

// Tests that when typed url is deleted on the client, sync the change gets
// propagated to server.
- (void)testSyncTypedURLDeleteFromClient {
  const GURL mockURL("http://not-a-real-site/");

  [ChromeEarlGrey clearBrowsingHistory];
  [self setTearDownHandler:^{
    [ChromeEarlGrey clearBrowsingHistory];
  }];

  // Inject typed url on server.
  [ChromeEarlGrey addFakeSyncServerTypedURL:mockURL];

  // Sign in to sync.
  ChromeIdentity* identity = [SigninEarlGreyUtils fakeIdentity1];
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()->AddIdentity(
      identity);
  [SigninEarlGreyUI signinWithIdentity:identity];

  [ChromeEarlGrey waitForSyncInitialized:YES syncTimeout:kSyncOperationTimeout];

  // Wait for typed url to appear on client.
  [ChromeEarlGrey waitForTypedURL:mockURL
                    expectPresent:YES
                          timeout:kSyncOperationTimeout];
  GREYAssert(
      [ChromeEarlGrey numberOfSyncEntitiesWithType:syncer::TYPED_URLS] == 1,
      @"There should be 1 typed URL entity");

  // Delete typed URL from client.
  [ChromeEarlGrey deleteHistoryServiceTypedURL:mockURL];

  // Trigger sync and wait for typed URL to be deleted.
  [ChromeEarlGrey triggerSyncCycleForType:syncer::TYPED_URLS];
  AssertNumberOfEntities(0, syncer::TYPED_URLS);
}

// Test that typed url is deleted from client after server sends tombstone for
// that typed url.
- (void)testSyncTypedURLDeleteFromServer {
  const GURL mockURL("http://not-a-real-site/");

  [ChromeEarlGrey clearBrowsingHistory];
  [self setTearDownHandler:^{
    [ChromeEarlGrey clearBrowsingHistory];
  }];
  [ChromeEarlGrey addHistoryServiceTypedURL:mockURL];

  // Sign in to sync.
  ChromeIdentity* identity = [SigninEarlGreyUtils fakeIdentity1];
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()->AddIdentity(
      identity);
  [SigninEarlGreyUI signinWithIdentity:identity];

  [ChromeEarlGrey waitForSyncInitialized:YES syncTimeout:kSyncOperationTimeout];
  [ChromeEarlGrey triggerSyncCycleForType:syncer::TYPED_URLS];

  [ChromeEarlGrey waitForSyncServerEntitiesWithType:syncer::TYPED_URLS
                                               name:mockURL.spec()
                                              count:1
                                            timeout:kSyncOperationTimeout];
  [ChromeEarlGrey deleteHistoryServiceTypedURL:mockURL];

  // Trigger sync and wait for fake server to be updated.
  [ChromeEarlGrey triggerSyncCycleForType:syncer::TYPED_URLS];
  [ChromeEarlGrey waitForTypedURL:mockURL
                    expectPresent:NO
                          timeout:kSyncOperationTimeout];
}

#pragma mark - Test Utilities

// Adds a bookmark with the given |url| and |title| into the Mobile Bookmarks
// folder.
// TODO(crbug.com/646164): This is copied from bookmarks_egtest.mm and should
// move to common location.
- (void)addBookmark:(const GURL)url withTitle:(NSString*)title {
  [ChromeEarlGrey waitForBookmarksToFinishLoading];
  bookmarks::BookmarkModel* bookmark_model =
      ios::BookmarkModelFactory::GetForBrowserState(
          chrome_test_util::GetOriginalBrowserState());
  bookmark_model->AddURL(bookmark_model->mobile_node(), 0,
                         base::SysNSStringToUTF16(title), url);
}

// Asserts that |expectedCount| bookmarks exist with the corresponding |title|
// using the BookmarkModel.
// TODO(crbug.com/646164): This is copied from bookmarks_egtest.mm and should
// move to common location.
+ (void)assertBookmarksWithTitle:(NSString*)title
                   expectedCount:(NSUInteger)expectedCount {
  // Get BookmarkModel and wait for it to be loaded.
  bookmarks::BookmarkModel* bookmarkModel =
      ios::BookmarkModelFactory::GetForBrowserState(
          chrome_test_util::GetOriginalBrowserState());

  // Verify the correct number of bookmarks exist.
  base::string16 matchString = base::SysNSStringToUTF16(title);
  std::vector<bookmarks::TitledUrlMatch> matches;
  bookmarkModel->GetBookmarksMatching(matchString, 50, &matches);
  const size_t count = matches.size();
  GREYAssertEqual(expectedCount, count, @"Unexpected number of bookmarks");
}

@end
