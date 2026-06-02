// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bookmarks/test/bookmark_earl_grey.h"

#import <Foundation/Foundation.h>

#import <memory>

#import "base/base_paths.h"
#import "base/no_destructor.h"
#import "base/path_service.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_storage_type.h"
#import "ios/chrome/browser/bookmarks/test/bookmark_earl_grey_app_interface.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"

namespace {

std::unique_ptr<net::EmbeddedTestServer> CreateAndStartSharedTestServer() {
  auto server = std::make_unique<net::EmbeddedTestServer>();
  server->ServeFilesFromDirectory(
      base::PathService::CheckedGet(base::DIR_ASSETS));
  bool started = server->Start();
  GREYAssertTrue(started, @"Failed to start the shared test server.");
  return server;
}

net::EmbeddedTestServer* GetSharedTestServer() {
  static base::NoDestructor<std::unique_ptr<net::EmbeddedTestServer>> server(
      CreateAndStartSharedTestServer());
  return server->get();
}

}  // namespace

const GURL GetFirstUrl() {
  return GetSharedTestServer()->GetURL(
      "/ios/testing/data/http_server_files/pony.html");
}

const GURL GetSecondUrl() {
  return GetSharedTestServer()->GetURL(
      "/ios/testing/data/http_server_files/destination.html");
}

const GURL GetFrenchUrl() {
  return GetSharedTestServer()->GetURL("/www.a.fr/");
}

@implementation BookmarkEarlGreyImpl

#pragma mark - Setup and Teardown

- (void)clearBookmarks {
  EG_TEST_HELPER_ASSERT_NO_ERROR([BookmarkEarlGreyAppInterface clearBookmarks]);
}

- (void)clearBookmarksPositionCache {
  [BookmarkEarlGreyAppInterface clearBookmarksPositionCache];
}

- (void)setupStandardBookmarksInStorage:(BookmarkStorageType)storageType {
  const GURL fourthURL = GetSharedTestServer()->GetURL(
      "/ios/testing/data/http_server_files/chromium_logo_page.html");

  NSString* spec1 = base::SysUTF8ToNSString(GetFirstUrl().spec());
  NSString* spec2 = base::SysUTF8ToNSString(GetSecondUrl().spec());
  NSString* spec3 = base::SysUTF8ToNSString(GetFrenchUrl().spec());
  NSString* spec4 = base::SysUTF8ToNSString(fourthURL.spec());
  EG_TEST_HELPER_ASSERT_NO_ERROR([BookmarkEarlGreyAppInterface
      setupStandardBookmarksUsingFirstURL:spec1
                                secondURL:spec2
                                 thirdURL:spec3
                                fourthURL:spec4
                                inStorage:storageType]);
}

- (void)setupBookmarksWhichExceedsScreenHeightInStorage:
    (BookmarkStorageType)storageType {
  EG_TEST_HELPER_ASSERT_NO_ERROR([BookmarkEarlGreyAppInterface
      setupBookmarksWhichExceedsScreenHeightUsingURL:@"http://google.com"
                                           inStorage:storageType]);
}

- (void)waitForBookmarkModelLoaded {
  EG_TEST_HELPER_ASSERT_NO_ERROR(
      [BookmarkEarlGreyAppInterface waitForBookmarkModelLoaded]);
}

- (void)commitPendingWrite {
  [BookmarkEarlGreyAppInterface commitPendingWrite];
}

- (void)setLastUsedBookmarkFolderToMobileBookmarksInStorageType:
    (BookmarkStorageType)storageType {
  [BookmarkEarlGreyAppInterface
      setLastUsedBookmarkFolderToMobileBookmarksInStorageType:storageType];
}

#pragma mark - Common Helpers

- (void)verifyBookmarksWithTitle:(NSString*)title
                   expectedCount:(NSUInteger)expectedCount
                       inStorage:(BookmarkStorageType)storageType {
  EG_TEST_HELPER_ASSERT_NO_ERROR([BookmarkEarlGreyAppInterface
      verifyBookmarksWithTitle:title
                 expectedCount:expectedCount
                     inStorage:storageType]);
}

- (void)verifyChildCount:(int)count
        inFolderWithName:(NSString*)name
               inStorage:(BookmarkStorageType)storageType {
  EG_TEST_HELPER_ASSERT_NO_ERROR([BookmarkEarlGreyAppInterface
      verifyChildCount:count
      inFolderWithName:name
             inStorage:storageType]);
}

- (void)addBookmarkWithTitle:(NSString*)title
                         URL:(NSString*)url
                   inStorage:(BookmarkStorageType)storageType {
  EG_TEST_HELPER_ASSERT_NO_ERROR([BookmarkEarlGreyAppInterface
      addBookmarkWithTitle:title
                       URL:url
                 inStorage:storageType]);
}

- (void)removeBookmarkWithTitle:(NSString*)title
                      inStorage:(BookmarkStorageType)storageType {
  EG_TEST_HELPER_ASSERT_NO_ERROR([BookmarkEarlGreyAppInterface
      removeBookmarkWithTitle:title
                    inStorage:storageType]);
}

- (void)moveBookmarkWithTitle:(NSString*)bookmarkTitle
            toFolderWithTitle:(NSString*)newFolder
                    inStorage:(BookmarkStorageType)storageType {
  EG_TEST_HELPER_ASSERT_NO_ERROR([BookmarkEarlGreyAppInterface
      moveBookmarkWithTitle:bookmarkTitle
          toFolderWithTitle:newFolder
                  inStorage:storageType]);
}

- (void)verifyExistenceOfBookmarkWithURL:(NSString*)URL
                                    name:(NSString*)name
                               inStorage:(BookmarkStorageType)storageType {
  EG_TEST_HELPER_ASSERT_NO_ERROR([BookmarkEarlGreyAppInterface
      verifyExistenceOfBookmarkWithURL:URL
                                  name:name
                             inStorage:storageType]);
}

- (void)verifyAbsenceOfBookmarkWithURL:(NSString*)URL
                             inStorage:(BookmarkStorageType)storageType {
  EG_TEST_HELPER_ASSERT_NO_ERROR([BookmarkEarlGreyAppInterface
      verifyAbsenceOfBookmarkWithURL:URL
                           inStorage:storageType]);
}

- (void)verifyExistenceOfFolderWithTitle:(NSString*)title
                               inStorage:(BookmarkStorageType)storageType {
  EG_TEST_HELPER_ASSERT_NO_ERROR([BookmarkEarlGreyAppInterface
      verifyExistenceOfFolderWithTitle:title
                             inStorage:storageType]);
}

- (void)verifyAbsenceOfFolderWithTitle:(NSString*)title
                             inStorage:(BookmarkStorageType)storageType {
  EG_TEST_HELPER_ASSERT_NO_ERROR([BookmarkEarlGreyAppInterface
      verifyAbsenceOfFolderWithTitle:title
                           inStorage:storageType]);
}

#pragma mark - Promo

- (void)verifyPromoAlreadySeen:(BOOL)seen {
  EG_TEST_HELPER_ASSERT_NO_ERROR(
      [BookmarkEarlGreyAppInterface verifyPromoAlreadySeen:seen]);
}

- (void)setPromoAlreadySeen:(BOOL)seen {
  [BookmarkEarlGreyAppInterface setPromoAlreadySeen:seen];
}

- (void)setPromoAlreadySeenNumberOfTimes:(int)times {
  [BookmarkEarlGreyAppInterface setPromoAlreadySeenNumberOfTimes:times];
}

- (int)numberOfTimesPromoAlreadySeen {
  return [BookmarkEarlGreyAppInterface numberOfTimesPromoAlreadySeen];
}

@end
