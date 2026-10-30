// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BOOKMARKS_TEST_BOOKMARK_EARL_GREY_H_
#define IOS_CHROME_BROWSER_BOOKMARKS_TEST_BOOKMARK_EARL_GREY_H_

#import <UIKit/UIKit.h>

#import "ios/testing/earl_grey/base_eg_test_helper_impl.h"
#import "url/gurl.h"

namespace bookmarks {
class BookmarkNode;
}  // namespace bookmarks

enum class BookmarkStorageType;

#define BookmarkEarlGrey \
  [BookmarkEarlGreyImpl invokedFromFile:@"" __FILE__ lineNumber:__LINE__]

// GURL for the testing bookmark "First URL".
const GURL GetFirstUrl();

// GURL for the testing bookmark "Second URL".
const GURL GetSecondUrl();

// GURL for the testing bookmark "French URL".
const GURL GetFrenchUrl();

// Test methods that perform actions on Bookmarks. These methods may read or
// alter Chrome's internal state programmatically or via the UI, but in both
// cases will properly synchronize the UI for Earl Grey tests.
@interface BookmarkEarlGreyImpl : BaseEGTestHelperImpl

#pragma mark - Setup and Teardown

// Clears bookmarks if any bookmark still presents. A GREYAssert is induced if
// bookmarks can not be cleared.
- (void)clearBookmarks;

// Clear Bookmarks top most row position cache.
- (void)clearBookmarksPositionCache;

// Loads a set of default bookmarks in `storageType` in the model for the tests
// to use. GREYAssert is induced if test bookmarks can not be loaded.
- (void)setupStandardBookmarksInStorage:(BookmarkStorageType)storageType;

// Loads a large set of bookmarks in `storageType` in the model which is longer
// than the screen height. GREYAssert is induced if test bookmarks can not be
// loaded.
- (void)setupBookmarksWhichExceedsScreenHeightInStorage:
    (BookmarkStorageType)storageType;

// Waits for BookmarkModel to be loaded.
- (void)waitForBookmarkModelLoaded;

// Flush any pending bookmarks writes to disk now. This is useful before
// terminating and restarting the app.
- (void)commitPendingWrite;

// Sets the last used bookmark folder to Mobile bookmarks in `storageType`.
- (void)setLastUsedBookmarkFolderToMobileBookmarksInStorageType:
    (BookmarkStorageType)storageType;

#pragma mark - Common Helpers

// Verifies that `expectedCount` bookmarks exist with the corresponding `title`
// in `storageType` using the BookmarkModel. GREYAssert is induced if the count
// doesn't match.
- (void)verifyBookmarksWithTitle:(NSString*)title
                   expectedCount:(NSUInteger)expectedCount
                       inStorage:(BookmarkStorageType)storageType;

// Verifies that there are `count` children in the bookmark folder with `name`
// in `storageType`. GREYAssert is induced if the folder doesn't exist or the
// count doesn't match.
- (void)verifyChildCount:(int)count
        inFolderWithName:(NSString*)name
               inStorage:(BookmarkStorageType)storageType;

// Programmatically adds a bookmark with the given `title` and `url` in
// `storageType`. GREYAssert is induced if the bookmark cannot be added.
- (void)addBookmarkWithTitle:(NSString*)title
                         URL:(NSString*)url
                   inStorage:(BookmarkStorageType)storageType;

// Removes programmatically the first bookmark with the given `title` in
// `storageType`. GREYAssert is induced if the bookmark can't be removed.
- (void)removeBookmarkWithTitle:(NSString*)title
                      inStorage:(BookmarkStorageType)storageType;

// Moves bookmark with title `bookmarkTitle` into a folder with title
// `newFolder` in `storageType`. GREYAssert is induced if the bookmark can't be
// moved.
- (void)moveBookmarkWithTitle:(NSString*)bookmarkTitle
            toFolderWithTitle:(NSString*)newFolder
                    inStorage:(BookmarkStorageType)storageType;

// Verifies the existence of a Bookmark with `URL` and `name` in `storageType`.
// GREYAssert is induced if the bookmark doesn't exist.
- (void)verifyExistenceOfBookmarkWithURL:(NSString*)URL
                                    name:(NSString*)name
                               inStorage:(BookmarkStorageType)storageType;

// Verifies the absence of a Bookmark with `URL` in `storageType`. GREYAssert is
// induced if the bookmark does exist.
- (void)verifyAbsenceOfBookmarkWithURL:(NSString*)URL
                             inStorage:(BookmarkStorageType)storageType;

// Verifies that a folder called `title` exists in `storageType`. GREYAssert is
// induced if the folder doesn't exist.
- (void)verifyExistenceOfFolderWithTitle:(NSString*)title
                               inStorage:(BookmarkStorageType)storageType;

// Verifies the absence of a folder with `title` in `storageType`. GREYAssert is
// induced if the folder does exist.
- (void)verifyAbsenceOfFolderWithTitle:(NSString*)title
                             inStorage:(BookmarkStorageType)storageType;

#pragma mark - Promo

// Checks that the promo seen state matches `seen`. GREYAssert is induced if
// the opposite is true.
- (void)verifyPromoAlreadySeen:(BOOL)seen;

// Sets whether the promo has already been `seen`.
- (void)setPromoAlreadySeen:(BOOL)seen;

// Sets that the promo has already been seen `times` number of times.
- (void)setPromoAlreadySeenNumberOfTimes:(int)times;

// Returns the number of times a Promo has been seen.
- (int)numberOfTimesPromoAlreadySeen;

@end

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_TEST_BOOKMARK_EARL_GREY_H_
