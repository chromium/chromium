// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BOOKMARKS_TEST_BOOKMARK_EARL_GREY_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_BOOKMARKS_TEST_BOOKMARK_EARL_GREY_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

enum class BookmarkStorageType;

// BookmarkEarlGreyAppInterface contains the app-side implementation for
// helpers that primarily work via direct model access. These helpers are
// compiled into the app binary and can be called from either app or test code.
@interface BookmarkEarlGreyAppInterface : NSObject

// Clears bookmarks. If not successful, returns an NSError indicating why the
// operation failed, otherwise nil.
+ (NSError*)clearBookmarks;

// Clear Bookmarks top most row position cache.
+ (void)clearBookmarksPositionCache;

// Loads a set of default bookmarks in the model for the tests to use with
// `firstURL`, `secondURL`, `thirdURL`, and `fourthURL` in `storageType`.
// Returns an NSError if the operation failed, otherwise nil.
+ (NSError*)setupStandardBookmarksUsingFirstURL:(NSString*)firstURL
                                      secondURL:(NSString*)secondURL
                                       thirdURL:(NSString*)thirdURL
                                      fourthURL:(NSString*)fourthURL
                                      inStorage:
                                          (BookmarkStorageType)storageType;

// Loads a large set of bookmarks in the model which is longer than the screen
// height using `URL` in `storageType`. Returns an NSError if the operation
// failed, otherwise nil.
+ (NSError*)setupBookmarksWhichExceedsScreenHeightUsingURL:(NSString*)URL
                                                 inStorage:(BookmarkStorageType)
                                                               storageType;

// Waits for BookmarkModel to be loaded. Returns an NSError if the operation
// timed out or failed, otherwise nil.
+ (NSError*)waitForBookmarkModelLoaded;

// Flush any pending bookmarks writes to disk now. This is useful before
// terminating and restarting the app.
+ (void)commitPendingWrite;

// Sets the last used bookmark folder to Mobile Bookmarks in `storageType`.
+ (void)setLastUsedBookmarkFolderToMobileBookmarksInStorageType:
    (BookmarkStorageType)storageType;

// Asserts that `expectedCount` bookmarks exist with the corresponding `title`
// using the BookmarkModel in `storageType`. Returns an NSError if the
// verification failed, otherwise nil.
+ (NSError*)verifyBookmarksWithTitle:(NSString*)title
                       expectedCount:(NSUInteger)expectedCount
                           inStorage:(BookmarkStorageType)storageType;

// Programmatically adds a bookmark with `title` and `url` in `storageType`.
// Returns an NSError if the operation failed, otherwise nil.
+ (NSError*)addBookmarkWithTitle:(NSString*)title
                             URL:(NSString*)url
                       inStorage:(BookmarkStorageType)storageType;

// Removes programmatically the first bookmark with `title` in `storageType`.
// Returns an NSError if the operation failed, otherwise nil.
+ (NSError*)removeBookmarkWithTitle:(NSString*)title
                          inStorage:(BookmarkStorageType)storageType;

// Moves bookmark with title `bookmarkTitle` into a folder with title
// `newFolder` in `storageType`. Returns an NSError if the operation failed,
// otherwise nil.
+ (NSError*)moveBookmarkWithTitle:(NSString*)bookmarkTitle
                toFolderWithTitle:(NSString*)newFolder
                        inStorage:(BookmarkStorageType)storageType;

// Verifies that there is `count` children on the bookmark folder with `name`
// in `storageType`. Returns an NSError if the verification failed, otherwise
// nil.
+ (NSError*)verifyChildCount:(size_t)count
            inFolderWithName:(NSString*)name
                   inStorage:(BookmarkStorageType)storageType;

// Verifies the existence of a Bookmark with `URL` and `name` in `storageType`.
// Returns an NSError if the verification failed, otherwise nil.
+ (NSError*)verifyExistenceOfBookmarkWithURL:(NSString*)URL
                                        name:(NSString*)name
                                   inStorage:(BookmarkStorageType)storageType;

// Verifies the absence of a Bookmark with `URL` in `storageType`.
// Returns an NSError if the verification failed, otherwise nil.
+ (NSError*)verifyAbsenceOfBookmarkWithURL:(NSString*)URL
                                 inStorage:(BookmarkStorageType)storageType;

// Verifies that a folder called `title` exists in `storageType`.
// Returns an NSError if the verification failed, otherwise nil.
+ (NSError*)verifyExistenceOfFolderWithTitle:(NSString*)title
                                   inStorage:(BookmarkStorageType)storageType;

// Verifies the absence of a folder called `title` in `storageType`.
// Returns an NSError if the verification failed, otherwise nil.
+ (NSError*)verifyAbsenceOfFolderWithTitle:(NSString*)title
                                 inStorage:(BookmarkStorageType)storageType;

// Verifies whether the promo has been seen, matching `seen`.
// Returns an NSError if the verification failed, otherwise nil.
+ (NSError*)verifyPromoAlreadySeen:(BOOL)seen;

// Sets whether the promo has already been seen according to `seen`.
+ (void)setPromoAlreadySeen:(BOOL)seen;

// Sets that the promo has already been seen `times` number of times.
+ (void)setPromoAlreadySeenNumberOfTimes:(int)times;

// Returns the number of times a Promo has been seen.
+ (int)numberOfTimesPromoAlreadySeen;

// Clears `_pendingSelectionIndexPath` on any `UITableView` in the connected
// window scenes. This is needed after presenting or dismissing a context menu
// via an EarlGrey long press on a `UITableViewCell`, which detaches the cell
// before `UITouchPhaseEnded` is delivered and leaves the table view's pending
// selection set to the long-pressed row.
+ (void)cancelBookmarksTableViewTouches;

@end

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_TEST_BOOKMARK_EARL_GREY_APP_INTERFACE_H_
