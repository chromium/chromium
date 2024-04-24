// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BOOKMARKS_MODEL_BOOKMARKS_UTILS_H_
#define IOS_CHROME_BROWSER_BOOKMARKS_MODEL_BOOKMARKS_UTILS_H_

#include <map>
#include <set>
#include <vector>

#include "base/location.h"

enum class BookmarkModelType;
class ChromeBrowserState;
class LegacyBookmarkModel;
class PrefService;

namespace bookmarks {
class BookmarkNode;
}  // namespace bookmarks

// Enum representing the internal behavior's outcome for
// `GetDefaultBookmarkFolder()`, distinguishing the various cases depending on
// the values in PrefService. These values are persisted to logs. Entries should
// not be renumbered and numeric values should never be reused.
enum class DefaultBookmarkFolderOutcomeForMetrics {
  kUnset = 0,
  kExistingLocalFolderSet = 1,
  kExistingAccountFolderSet = 2,
  kMissingLocalFolderSet = 3,
  kMissingAccountFolderSet = 4,
  kMaxValue = kMissingAccountFolderSet
};

// Used in the preference kIosBookmarkLastUsedFolderReceivingBookmarks.
// It means that the user has not set a folder for bookmarks explicitly.
extern const int64_t kLastUsedBookmarkFolderNone;

// Checks whether all available bookmark models are loaded.
// Return true if the bookmarks model are loaded, false otherwise.
// TODO(crbug.com/326185948): Inline this trivial helper function.
[[nodiscard]] bool AreAllAvailableBookmarkModelsLoaded(
    ChromeBrowserState* browser_state);

// Removes all user bookmarks and clears bookmark-related pref. Requires
// bookmark model to be loaded.
// Return true if the bookmarks were successfully removed and false otherwise.
// TODO(crbug.com/326185948): Inline this trivial helper function.
[[nodiscard]] bool RemoveAllUserBookmarksIOS(ChromeBrowserState* browser_state,
                                             const base::Location& location);

// Returns the permanent nodes whose url children are considered uncategorized
// and whose folder children should be shown in the bookmark menu.
// `model` must be loaded.
std::vector<const bookmarks::BookmarkNode*> PrimaryPermanentNodes(
    LegacyBookmarkModel* model);

// Returns whether `node` is a primary permanent node in the sense of
// `PrimaryPermanentNodes`.
bool IsPrimaryPermanentNode(const bookmarks::BookmarkNode* node,
                            LegacyBookmarkModel* model);

// Whether a bookmark was manually moved by the user to a different folder since
// last signin/signout.
bool IsLastUsedBookmarkFolderSet(PrefService* prefs);

// Resets the preferences related to the last used folder for bookmarks.
void ResetLastUsedBookmarkFolder(PrefService* prefs);

// Records `folder` of model `type` as the last folder the user selected to save
// or move bookmarks.
void SetLastUsedBookmarkFolder(PrefService* prefs,
                               const bookmarks::BookmarkNode* folder,
                               BookmarkModelType type);

// It returns the first bookmark folder that exists, with the following
// priority:
//- Last used folder
//- Account mobile folder
//- Local mobile folder
const bookmarks::BookmarkNode* GetDefaultBookmarkFolder(
    PrefService* prefs,
    bool is_account_bookmark_model_available,
    LegacyBookmarkModel* profile_bookmark_model,
    LegacyBookmarkModel* account_bookmark_model);

// Used when on-disk bookmark IDs have been reassigned and therefore the prefs
// need to be migrated accordingly.
void MigrateLastUsedBookmarkFolderUponLocalIdsReassigned(
    PrefService* prefs,
    const std::multimap<int64_t, int64_t>&
        local_or_syncable_reassigned_ids_per_old_id);

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_MODEL_BOOKMARKS_UTILS_H_
