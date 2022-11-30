// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/bookmark_earl_grey.h"

#import <Foundation/Foundation.h>

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_earl_grey_app_interface.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/public/test/http_server/http_server.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

const GURL GetFirstUrl() {
  return web::test::HttpServer::MakeUrl(
      "http://ios/testing/data/http_server_files/pony.html");
}

const GURL GetSecondUrl() {
  return web::test::HttpServer::MakeUrl(
      "http://ios/testing/data/http_server_files/destination.html");
}

const GURL GetFrenchUrl() {
  return web::test::HttpServer::MakeUrl("http://www.a.fr/");
}

@implementation BookmarkEarlGreyImpl

#pragma mark - Setup and Teardown

- (void)clearBookmarksPositionCache {
  [BookmarkEarlGreyAppInterface clearBookmarksPositionCache];
}

- (void)setupStandardBookmarks {
  const GURL fourthURL = web::test::HttpServer::MakeUrl(
      "http://ios/testing/data/http_server_files/chromium_logo_page.html");

  NSString* spec1 = base::SysUTF8ToNSString(GetFirstUrl().spec());
  NSString* spec2 = base::SysUTF8ToNSString(GetSecondUrl().spec());
  NSString* spec3 = base::SysUTF8ToNSString(GetFrenchUrl().spec());
  NSString* spec4 = base::SysUTF8ToNSString(fourthURL.spec());
  EG_TEST_HELPER_ASSERT_NO_ERROR([BookmarkEarlGreyAppInterface
      setupStandardBookmarksUsingFirstURL:spec1
                                secondURL:spec2
                                 thirdURL:spec3
                                fourthURL:spec4]);
}

- (void)setupBookmarksWhichExceedsScreenHeight {
  EG_TEST_HELPER_ASSERT_NO_ERROR([BookmarkEarlGreyAppInterface
      setupBookmarksWhichExceedsScreenHeightUsingURL:@"http://google.com"]);
}

- (void)waitForBookmarkModelLoaded:(BOOL)loaded {
  EG_TEST_HELPER_ASSERT_TRUE(
      [BookmarkEarlGreyAppInterface waitForBookmarkModelLoaded:loaded],
      @"Bookmark model was not loaded");
}

#pragma mark - Common Helpers

- (void)verifyBookmarksWithTitle:(NSString*)title
                   expectedCount:(NSUInteger)expectedCount {
  EG_TEST_HELPER_ASSERT_NO_ERROR([BookmarkEarlGreyAppInterface
      verifyBookmarksWithTitle:title
                 expectedCount:expectedCount]);
}

- (void)verifyChildCount:(int)count inFolderWithName:(NSString*)name {
  EG_TEST_HELPER_ASSERT_NO_ERROR([BookmarkEarlGreyAppInterface
      verifyChildCount:count
      inFolderWithName:name]);
}

- (void)addBookmarkWithTitle:(NSString*)title URL:(NSString*)url {
  EG_TEST_HELPER_ASSERT_NO_ERROR(
      [BookmarkEarlGreyAppInterface addBookmarkWithTitle:title URL:url]);
}

- (void)removeBookmarkWithTitle:(NSString*)title {
  EG_TEST_HELPER_ASSERT_NO_ERROR(
      [BookmarkEarlGreyAppInterface removeBookmarkWithTitle:title]);
}

- (void)moveBookmarkWithTitle:(NSString*)bookmarkTitle
            toFolderWithTitle:(NSString*)newFolder {
  EG_TEST_HELPER_ASSERT_NO_ERROR([BookmarkEarlGreyAppInterface
      moveBookmarkWithTitle:bookmarkTitle
          toFolderWithTitle:newFolder]);
}

- (void)verifyExistenceOfBookmarkWithURL:(NSString*)URL name:(NSString*)name {
  EG_TEST_HELPER_ASSERT_NO_ERROR([BookmarkEarlGreyAppInterface
      verifyExistenceOfBookmarkWithURL:URL
                                  name:name]);
}

- (void)verifyAbsenceOfBookmarkWithURL:(NSString*)URL {
  EG_TEST_HELPER_ASSERT_NO_ERROR(
      [BookmarkEarlGreyAppInterface verifyAbsenceOfBookmarkWithURL:URL]);
}

- (void)verifyExistenceOfFolderWithTitle:(NSString*)title {
  EG_TEST_HELPER_ASSERT_NO_ERROR(
      [BookmarkEarlGreyAppInterface verifyExistenceOfFolderWithTitle:title]);
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
