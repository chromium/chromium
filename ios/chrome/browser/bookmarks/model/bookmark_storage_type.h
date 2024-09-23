// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BOOKMARKS_MODEL_BOOKMARK_STORAGE_TYPE_H_
#define IOS_CHROME_BROWSER_BOOKMARKS_MODEL_BOOKMARK_STORAGE_TYPE_H_

// Distinguishes whether or not a bookmark node is considered an account node,
// as opposed to a local-only node.
//
// Do not change the explicitly set values. This enum is saved in preference
// kIosBookmarkLastUsedStorageReceivingBookmarks.
enum class BookmarkStorageType {
  // Bookmarks that are stored on the local device only.
  kLocalOrSyncable = 0,
  // Account storage indicates all data can be attributed to an account, which
  // also means the data will be removed from the BookmarkModel when the user
  // signs out.
  kAccount = 1,
};

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_MODEL_BOOKMARK_STORAGE_TYPE_H_
