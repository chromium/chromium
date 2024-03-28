// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/bookmarks/model/bookmarks_utils.h"

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/metrics/histogram_functions.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/browser/core_bookmark_model.h"
#include "components/prefs/pref_service.h"
#include "ios/chrome/browser/bookmarks/model/bookmark_model_factory.h"
#include "ios/chrome/browser/bookmarks/model/bookmark_model_type.h"
#include "ios/chrome/browser/bookmarks/model/legacy_bookmark_model.h"
#include "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/shared/model/prefs/pref_names.h"

using bookmarks::BookmarkNode;

namespace {

void LogDefaultBookmarkFolderOutcome(
    DefaultBookmarkFolderOutcomeForMetrics value) {
  base::UmaHistogramEnumeration("IOS.Bookmarks.DefaultBookmarkFolderOutcome",
                                value);
}

// Returns the bookmark model designed by `type`.
LegacyBookmarkModel* GetBookmarkModelForType(
    BookmarkModelType type,
    LegacyBookmarkModel* local_or_syncable_bookmark_model,
    LegacyBookmarkModel* account_bookmark_model) {
  switch (type) {
    case BookmarkModelType::kAccount:
      return account_bookmark_model;
    case BookmarkModelType::kLocalOrSyncable:
      return local_or_syncable_bookmark_model;
  }
  NOTREACHED_NORETURN();
}

}  // namespace

const int64_t kLastUsedBookmarkFolderNone = -1;

bool AreAllAvailableBookmarkModelsLoaded(ChromeBrowserState* browser_state) {
  bookmarks::CoreBookmarkModel* model =
      ios::BookmarkModelFactory::GetForBrowserState(browser_state);
  CHECK(model);
  return model->loaded();
}

bool RemoveAllUserBookmarksIOS(ChromeBrowserState* browser_state) {
  bookmarks::CoreBookmarkModel* bookmark_model =
      ios::BookmarkModelFactory::GetForBrowserState(browser_state);

  if (!bookmark_model->loaded()) {
    return false;
  }

  bookmark_model->RemoveAllUserBookmarks();

  ResetLastUsedBookmarkFolder(browser_state->GetPrefs());
  return true;
}

std::vector<const BookmarkNode*> PrimaryPermanentNodes(
    LegacyBookmarkModel* model) {
  DCHECK(model->loaded());
  std::vector<const BookmarkNode*> nodes;

  // When dealing with account bookmarks, it is possible that they don't
  // actually exist (e.g. the user is signed out). It is guaranteed that all
  // exist or none.
  if (!model->mobile_node()) {
    // Account bookmarks do not exist, no need to return them.
    DCHECK(!model->bookmark_bar_node());
    DCHECK(!model->other_node());
    return nodes;
  }

  // Account bookmarks do exist (all three). Return them.
  DCHECK(model->bookmark_bar_node());
  DCHECK(model->other_node());

  nodes.push_back(model->mobile_node());
  nodes.push_back(model->bookmark_bar_node());
  nodes.push_back(model->other_node());
  return nodes;
}

bool IsPrimaryPermanentNode(const BookmarkNode* node,
                            LegacyBookmarkModel* model) {
  std::vector<const BookmarkNode*> primary_nodes(PrimaryPermanentNodes(model));
  return base::Contains(primary_nodes, node);
}

bool IsLastUsedBookmarkFolderSet(PrefService* prefs) {
  return prefs->GetInt64(prefs::kIosBookmarkLastUsedFolderReceivingBookmarks) ==
         kLastUsedBookmarkFolderNone;
}

void ResetLastUsedBookmarkFolder(PrefService* prefs) {
  prefs->ClearPref(prefs::kIosBookmarkLastUsedFolderReceivingBookmarks);
  prefs->ClearPref(prefs::kIosBookmarkLastUsedStorageReceivingBookmarks);
}

void SetLastUsedBookmarkFolder(PrefService* prefs,
                               const bookmarks::BookmarkNode* folder,
                               BookmarkModelType type) {
  CHECK(folder);
  CHECK(folder->is_folder()) << "node type: " << folder->type()
                             << ", storage type: " << static_cast<int>(type);
  prefs->SetInt64(prefs::kIosBookmarkLastUsedFolderReceivingBookmarks,
                  folder->id());
  prefs->SetInteger(prefs::kIosBookmarkLastUsedStorageReceivingBookmarks,
                    static_cast<int>(type));
}

const bookmarks::BookmarkNode* GetDefaultBookmarkFolder(
    PrefService* prefs,
    bool is_account_bookmark_model_available,
    LegacyBookmarkModel* local_or_syncable_bookmark_model,
    LegacyBookmarkModel* account_bookmark_model) {
  int64_t node_id =
      prefs->GetInt64(prefs::kIosBookmarkLastUsedFolderReceivingBookmarks);

  if (node_id == kLastUsedBookmarkFolderNone) {
    LogDefaultBookmarkFolderOutcome(
        DefaultBookmarkFolderOutcomeForMetrics::kUnset);
  } else {
    BookmarkModelType type = static_cast<BookmarkModelType>(prefs->GetInteger(
        prefs::kIosBookmarkLastUsedStorageReceivingBookmarks));
    LegacyBookmarkModel* bookmark_model = GetBookmarkModelForType(
        type, local_or_syncable_bookmark_model, account_bookmark_model);
    const BookmarkNode* result = bookmark_model->GetNodeById(node_id);
    if (result && result->is_folder()) {
      // Make sure the bookmark node is a folder. See crbug.com/1450146.
      LogDefaultBookmarkFolderOutcome(
          (bookmark_model == local_or_syncable_bookmark_model)
              ? DefaultBookmarkFolderOutcomeForMetrics::kExistingLocalFolderSet
              : DefaultBookmarkFolderOutcomeForMetrics::
                    kExistingAccountFolderSet);
      return result;
    } else {
      LogDefaultBookmarkFolderOutcome(
          (bookmark_model == local_or_syncable_bookmark_model)
              ? DefaultBookmarkFolderOutcomeForMetrics::kMissingLocalFolderSet
              : DefaultBookmarkFolderOutcomeForMetrics::
                    kMissingAccountFolderSet);
    }
  }

  // Either preferences is not set, or refers to a non-existing folder.
  BookmarkModelType type = (is_account_bookmark_model_available &&
                            account_bookmark_model->mobile_node() != nullptr)
                               ? BookmarkModelType::kAccount
                               : BookmarkModelType::kLocalOrSyncable;
  LegacyBookmarkModel* bookmark_model = GetBookmarkModelForType(
      type, local_or_syncable_bookmark_model, account_bookmark_model);
  return bookmark_model->mobile_node();
}

void MigrateLastUsedBookmarkFolderUponLocalIdsReassigned(
    PrefService* prefs,
    const std::multimap<int64_t, int64_t>&
        local_or_syncable_reassigned_ids_per_old_id) {
  const int64_t node_id_in_prefs =
      prefs->GetInt64(prefs::kIosBookmarkLastUsedFolderReceivingBookmarks);

  if (node_id_in_prefs == kLastUsedBookmarkFolderNone) {
    return;
  }

  const BookmarkModelType type = static_cast<BookmarkModelType>(
      prefs->GetInteger(prefs::kIosBookmarkLastUsedStorageReceivingBookmarks));
  if (type != BookmarkModelType::kLocalOrSyncable) {
    // Account bookmarks don't get their IDs reassigned as a result of the
    // migration covered here (the adoption of a single BookmarkModel on iOS,
    // whereas previously this client may have used two of them).
    return;
  }

  const size_t match_count =
      local_or_syncable_reassigned_ids_per_old_id.count(node_id_in_prefs);
  if (match_count == 0) {
    // ID not reassigned; nothing to do.
    return;
  }

  if (match_count != 1) {
    // ID reassignment ambiguous: this should be very rare and hence not
    // supported.
    return;
  }

  const int64_t new_node_id =
      local_or_syncable_reassigned_ids_per_old_id.find(node_id_in_prefs)
          ->second;
  prefs->SetInt64(prefs::kIosBookmarkLastUsedFolderReceivingBookmarks,
                  new_node_id);
}
