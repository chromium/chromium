// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BOOKMARKS_BOOKMARKS_UTILS_H_
#define IOS_CHROME_BROWSER_BOOKMARKS_BOOKMARKS_UTILS_H_

#include <set>
#include <vector>

#include "base/compiler_specific.h"

class ChromeBrowserState;

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}

// Removes all user bookmarks and clears bookmark-related pref. Requires
// bookmark model to be loaded.
// Return true if the bookmarks were successfully removed and false otherwise.
bool RemoveAllUserBookmarksIOS(ChromeBrowserState* browser_state)
    WARN_UNUSED_RESULT;

// Returns the permanent nodes whose url children are considered uncategorized
// and whose folder children should be shown in the bookmark menu.
// |model| must be loaded.
std::vector<const bookmarks::BookmarkNode*> PrimaryPermanentNodes(
    bookmarks::BookmarkModel* model);

// Returns an unsorted vector of folders that are considered to be at the "root"
// level of the bookmark hierarchy. Functionally, this means all direct
// descendants of PrimaryPermanentNodes.
std::vector<const bookmarks::BookmarkNode*> RootLevelFolders(
    bookmarks::BookmarkModel* model);

// Returns whether |node| is a primary permanent node in the sense of
// |PrimaryPermanentNodes|.
bool IsPrimaryPermanentNode(const bookmarks::BookmarkNode* node,
                            bookmarks::BookmarkModel* model);

// Returns the root level folder in which this node is directly, or indirectly
// via subfolders, located.
const bookmarks::BookmarkNode* RootLevelFolderForNode(
    const bookmarks::BookmarkNode* node,
    bookmarks::BookmarkModel* model);

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_BOOKMARKS_UTILS_H_
