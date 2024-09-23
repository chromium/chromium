// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BOOKMARKS_MODEL_BOOKMARKS_UTILS_H_
#define IOS_CHROME_BROWSER_BOOKMARKS_MODEL_BOOKMARKS_UTILS_H_

#include <map>
#include <set>
#include <vector>

enum class BookmarkStorageType;
class PrefService;

namespace bookmarks {
class BookmarkModel;
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

// Returns the permanent bookmark folders that match `type`.
// `model` must not be null and must be loaded. The returned list follows the
// ordering used to display the folders in the management UI. Note that the
// managed bookmarks folder is never included.
//
// Additional caveats if `BookmarkStorageType::kAccount` is used:
// 1. The function may return an empty result if account bookmarks don't
//    actually exist (e.g. the user is signed out).
// 2. In rare cases, it may also return a non-empty but partial list, if this
//    function is exercised *during* the creation of account permanent folders,
//    which report BookmarkModelObserver::BookmarkNodeAdded() individually. The
//    same is true during their destruction (during signout).
std::vector<const bookmarks::BookmarkNode*> PrimaryPermanentNodes(
    const bookmarks::BookmarkModel* model,
    BookmarkStorageType type);

// Whether a bookmark was manually moved by the user to a different folder since
// last signin/signout.
bool IsLastUsedBookmarkFolderSet(PrefService* prefs);

// Resets the preferences related to the last used folder for bookmarks.
void ResetLastUsedBookmarkFolder(PrefService* prefs);

// Records `folder` of model `type` as the last folder the user selected to save
// or move bookmarks.
void SetLastUsedBookmarkFolder(PrefService* prefs,
                               const bookmarks::BookmarkNode* folder,
                               BookmarkStorageType type);

// It returns the first bookmark folder that exists, with the following
// priority:
//- Last used folder
//- Account mobile folder
//- Local mobile folder
const bookmarks::BookmarkNode* GetDefaultBookmarkFolder(
    PrefService* prefs,
    const bookmarks::BookmarkModel* bookmark_model);

// Used when on-disk bookmark IDs have been reassigned and therefore the prefs
// need to be migrated accordingly.
void MigrateLastUsedBookmarkFolderUponLocalIdsReassigned(
    PrefService* prefs,
    const std::multimap<int64_t, int64_t>&
        local_or_syncable_reassigned_ids_per_old_id);

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_MODEL_BOOKMARKS_UTILS_H_
