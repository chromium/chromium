// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_APP_BOOKMARKS_TEST_UTIL_H_
#define IOS_CHROME_TEST_APP_BOOKMARKS_TEST_UTIL_H_

namespace chrome_test_util {

// Returns if the internal Bookmarks state is loaded.
bool BookmarksLoaded();

// Clears all bookmarks. Returns true if successful, false otherwise.
bool ClearBookmarks();

}  // namespace chrome_test_util

#endif  // IOS_CHROME_TEST_APP_BOOKMARKS_TEST_UTIL_H_
