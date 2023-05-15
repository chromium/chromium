// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BOOKMARKS_BOOKMARKS_UTILS_H_
#define IOS_CHROME_BROWSER_BOOKMARKS_BOOKMARKS_UTILS_H_

#include <set>
#include <vector>

class ChromeBrowserState;
class PrefService;

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
enum class StorageType;
}

// Used in the preference kIosBookmarkLastUsedFolderReceivingBookmarks.
// It means that the user has not set a folder for bookmarks explicitly.
extern const int64_t kLastUsedBookmarkFolderNone;

// Removes all user bookmarks and clears bookmark-related pref. Requires
// bookmark model to be loaded.
// Return true if the bookmarks were successfully removed and false otherwise.
[[nodiscard]] bool RemoveAllUserBookmarksIOS(ChromeBrowserState* browser_state);

// Returns the permanent nodes whose url children are considered uncategorized
// and whose folder children should be shown in the bookmark menu.
// `model` must be loaded.
std::vector<const bookmarks::BookmarkNode*> PrimaryPermanentNodes(
    bookmarks::BookmarkModel* model);

// Returns whether `node` is a primary permanent node in the sense of
// `PrimaryPermanentNodes`.
bool IsPrimaryPermanentNode(const bookmarks::BookmarkNode* node,
                            bookmarks::BookmarkModel* model);

// Whether a bookmark was manually moved by the user to a different folder since
// last signin/signout.
bool IsLastUsedBookmarkFolderSet(PrefService* prefs);

// Resets the preferences related to the last used folder for bookmarks.
void ResetLastUsedBookmarkFolder(PrefService* prefs);

// Records `folder` of model `type` as the last folder the user selected to save
// or move bookmarks.
void SetLastUsedBookmarkFolder(PrefService* prefs,
                               const bookmarks::BookmarkNode* folder,
                               bookmarks::StorageType type);

// It returns the first bookmark folder that exists, with the following
// priority:
//- Last used folder
//- Account mobile folder
//- Profile mobile folder
const bookmarks::BookmarkNode* GetDefaultBookmarkFolder(
    PrefService* prefs,
    bool is_account_bookmark_model_available,
    bookmarks::BookmarkModel* profile_bookmark_model,
    bookmarks::BookmarkModel* account_bookmark_model);

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_BOOKMARKS_UTILS_H_
