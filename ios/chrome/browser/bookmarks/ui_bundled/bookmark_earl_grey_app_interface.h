// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_BOOKMARK_EARL_GREY_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_BOOKMARK_EARL_GREY_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

enum class BookmarkStorageType;

// BookmarkEarlGreyAppInterface contains the app-side implementation for
// helpers that primarily work via direct model access. These helpers are
// compiled into the app binary and can be called from either app or test code.
@interface BookmarkEarlGreyAppInterface : NSObject

// Clears bookmarks. If not succeed returns an NSError indicating  why the
// operation failed, otherwise nil.
+ (NSError*)clearBookmarks;

// Clear Bookmarks top most row position cache.
+ (void)clearBookmarksPositionCache;

// Loads a set of default bookmarks in the model for the tests to use.
+ (NSError*)setupStandardBookmarksUsingFirstURL:(NSString*)firstURL
                                      secondURL:(NSString*)secondURL
                                       thirdURL:(NSString*)thirdURL
                                      fourthURL:(NSString*)fourthURL
                                      inStorage:
                                          (BookmarkStorageType)storageType;

// Loads a large set of bookmarks in the model which is longer than the screen
// height.
+ (NSError*)setupBookmarksWhichExceedsScreenHeightUsingURL:(NSString*)URL
                                                 inStorage:(BookmarkStorageType)
                                                               storageType;

// Waits for BookmarkModel to be loaded.
+ (NSError*)waitForBookmarkModelLoaded;

// Flush any pending bookmarks writes to disk now. This is useful before
// terminating and restarting the app.
+ (void)commitPendingWrite;

// Set the last used bookmark folder.
+ (void)setLastUsedBookmarkFolderToMobileBookmarksInStorageType:
    (BookmarkStorageType)storageType;

// Asserts that `expectedCount` bookmarks exist with the corresponding `title`
// using the BookmarkModel.
+ (NSError*)verifyBookmarksWithTitle:(NSString*)title
                       expectedCount:(NSUInteger)expectedCount
                           inStorage:(BookmarkStorageType)storageType;

// Programmatically adds a bookmark with the given title and URL.
+ (NSError*)addBookmarkWithTitle:(NSString*)title
                             URL:(NSString*)url
                       inStorage:(BookmarkStorageType)storageType;

// Removes programmatically the first bookmark with the given title.
+ (NSError*)removeBookmarkWithTitle:(NSString*)title
                          inStorage:(BookmarkStorageType)storageType;

// Moves bookmark with title `bookmarkTitle` into a folder with title
// `newFolder`.
+ (NSError*)moveBookmarkWithTitle:(NSString*)bookmarkTitle
                toFolderWithTitle:(NSString*)newFolder
                        inStorage:(BookmarkStorageType)storageType;

// Verifies that there is `count` children on the bookmark folder with `name`.
+ (NSError*)verifyChildCount:(size_t)count
            inFolderWithName:(NSString*)name
                   inStorage:(BookmarkStorageType)storageType;

// Verifies the existence of a Bookmark with `URL` and `name`.
+ (NSError*)verifyExistenceOfBookmarkWithURL:(NSString*)URL
                                        name:(NSString*)name
                                   inStorage:(BookmarkStorageType)storageType;

// Verifies the absence of a Bookmark with `URL`.
+ (NSError*)verifyAbsenceOfBookmarkWithURL:(NSString*)URL
                                 inStorage:(BookmarkStorageType)storageType;

// Verifies that a folder called `title` exists.
+ (NSError*)verifyExistenceOfFolderWithTitle:(NSString*)title
                                   inStorage:(BookmarkStorageType)storageType;

// Checks that the promo has already been seen or not.
+ (NSError*)verifyPromoAlreadySeen:(BOOL)seen;

// Checks that the promo has already been seen or not.
+ (void)setPromoAlreadySeen:(BOOL)seen;

// Sets that the promo has already been seen `times` number of times.
+ (void)setPromoAlreadySeenNumberOfTimes:(int)times;

// Returns the number of times a Promo has been seen.
+ (int)numberOfTimesPromoAlreadySeen;

@end

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_BOOKMARK_EARL_GREY_APP_INTERFACE_H_
