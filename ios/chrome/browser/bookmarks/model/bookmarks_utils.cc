// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/bookmarks/model/bookmarks_utils.h"

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/metrics/histogram_functions.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/prefs/pref_service.h"
#include "ios/chrome/browser/bookmarks/model/bookmark_model_factory.h"
#include "ios/chrome/browser/bookmarks/model/bookmark_storage_type.h"
#include "ios/chrome/browser/shared/model/prefs/pref_names.h"

using bookmarks::BookmarkNode;

namespace {

void LogDefaultBookmarkFolderOutcome(
    DefaultBookmarkFolderOutcomeForMetrics value) {
  base::UmaHistogramEnumeration("IOS.Bookmarks.DefaultBookmarkFolderOutcome",
                                value);
}

}  // namespace

const int64_t kLastUsedBookmarkFolderNone = -1;

std::vector<const bookmarks::BookmarkNode*> PrimaryPermanentNodes(
    const bookmarks::BookmarkModel* model,
    BookmarkStorageType type) {
  CHECK(model);
  CHECK(model->loaded());

  switch (type) {
    case BookmarkStorageType::kLocalOrSyncable:
      return {model->mobile_node(), model->bookmark_bar_node(),
              model->other_node()};
    case BookmarkStorageType::kAccount: {
      std::vector<const bookmarks::BookmarkNode*> nodes;
      // Normally either all account permanent nodes exists or none does, but
      // during transitional states (i.e. during creation or removal of account
      // permanent nodes) it is also possible that a subset exists.
      for (const bookmarks::BookmarkNode* node :
           {model->account_mobile_node(), model->account_bookmark_bar_node(),
            model->account_other_node()}) {
        if (node) {
          nodes.push_back(node);
        }
      }
      return nodes;
    }
  }
  NOTREACHED();
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
                               BookmarkStorageType type) {
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
    const bookmarks::BookmarkModel* bookmark_model) {
  int64_t node_id =
      prefs->GetInt64(prefs::kIosBookmarkLastUsedFolderReceivingBookmarks);

  if (node_id == kLastUsedBookmarkFolderNone) {
    LogDefaultBookmarkFolderOutcome(
        DefaultBookmarkFolderOutcomeForMetrics::kUnset);
  } else {
    BookmarkStorageType type =
        static_cast<BookmarkStorageType>(prefs->GetInteger(
            prefs::kIosBookmarkLastUsedStorageReceivingBookmarks));

    const BookmarkNode* result =
        bookmarks::GetBookmarkNodeByID(bookmark_model, node_id);
    if (result && result->is_folder()) {
      // Make sure the bookmark node is a folder. See crbug.com/1450146.
      LogDefaultBookmarkFolderOutcome(
          bookmark_model->IsLocalOnlyNode(*result)
              ? DefaultBookmarkFolderOutcomeForMetrics::kExistingLocalFolderSet
              : DefaultBookmarkFolderOutcomeForMetrics::
                    kExistingAccountFolderSet);
      return result;
    } else {
      LogDefaultBookmarkFolderOutcome(
          (type == BookmarkStorageType::kLocalOrSyncable)
              ? DefaultBookmarkFolderOutcomeForMetrics::kMissingLocalFolderSet
              : DefaultBookmarkFolderOutcomeForMetrics::
                    kMissingAccountFolderSet);
    }
  }

  // Either preferences is not set, or refers to a non-existing folder.
  return (bookmark_model->account_mobile_node() != nullptr)
             ? bookmark_model->account_mobile_node()
             : bookmark_model->mobile_node();
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

  const BookmarkStorageType type = static_cast<BookmarkStorageType>(
      prefs->GetInteger(prefs::kIosBookmarkLastUsedStorageReceivingBookmarks));
  if (type != BookmarkStorageType::kLocalOrSyncable) {
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
