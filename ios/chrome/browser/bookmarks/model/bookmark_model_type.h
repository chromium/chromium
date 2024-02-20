// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BOOKMARKS_MODEL_BOOKMARK_MODEL_TYPE_H_
#define IOS_CHROME_BROWSER_BOOKMARKS_MODEL_BOOKMARK_MODEL_TYPE_H_

// On iOS, there are two BookmarkModel instances, with one factory each. This
// enum allows representing which of the two is relevant in a context.
//
// Do not change the explicitly set values. This enum is saved in preference
// kIosBookmarkLastUsedStorageReceivingBookmarks.
enum class BookmarkModelType {
  // Bookmarks that are stored on the local device only. Corresponds to
  // LocalOrSyncableBookmarkModelFactory.
  kLocalOrSyncable = 0,
  // Account storage indicates all data can be attributed to an account, which
  // also means the data will be removed from the BookmarkModel when the user
  // signs out. Corresponds to AccountBookmarkModelFactory.
  kAccount = 1,
};

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_MODEL_BOOKMARK_MODEL_TYPE_H_
