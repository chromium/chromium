// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/bookmarks/bookmarks_utils.h"

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/metrics/histogram_macros.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/prefs/pref_service.h"
#include "ios/chrome/browser/bookmarks/local_or_syncable_bookmark_model_factory.h"
#include "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/shared/model/prefs/pref_names.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

namespace {

// Returns the bookmark model designed by `type`.
bookmarks::BookmarkModel* GetBookmarkModelForType(
    bookmarks::StorageType type,
    bookmarks::BookmarkModel* profile_bookmark_model,
    bookmarks::BookmarkModel* account_bookmark_model) {
  switch (type) {
    case bookmarks::StorageType::kAccount:
      return account_bookmark_model;
    case bookmarks::StorageType::kLocalOrSyncable:
      return profile_bookmark_model;
  }
  NOTREACHED_NORETURN();
}

}  // namespace

const int64_t kLastUsedBookmarkFolderNone = -1;

bool RemoveAllUserBookmarksIOS(ChromeBrowserState* browser_state) {
  BookmarkModel* bookmark_model =
      ios::LocalOrSyncableBookmarkModelFactory::GetForBrowserState(
          browser_state);

  if (!bookmark_model->loaded())
    return false;

  bookmark_model->RemoveAllUserBookmarks();

  for (const auto& child : bookmark_model->root_node()->children()) {
    if (!bookmark_model->client()->CanBeEditedByUser(child.get()))
      continue;
    if (!child->children().empty())
      return false;
  }

  ResetLastUsedBookmarkFolder(browser_state->GetPrefs());
  return true;
}

std::vector<const BookmarkNode*> PrimaryPermanentNodes(BookmarkModel* model) {
  DCHECK(model->loaded());
  std::vector<const BookmarkNode*> nodes;
  nodes.push_back(model->mobile_node());
  nodes.push_back(model->bookmark_bar_node());
  nodes.push_back(model->other_node());
  return nodes;
}

bool IsPrimaryPermanentNode(const BookmarkNode* node, BookmarkModel* model) {
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
                               bookmarks::StorageType type) {
  DCHECK(folder);
  DCHECK(folder->is_folder()) << "node type: " << folder->type()
                              << ", storage type: " << static_cast<int>(type);
  prefs->SetInt64(prefs::kIosBookmarkLastUsedFolderReceivingBookmarks,
                  folder->id());
  prefs->SetInteger(prefs::kIosBookmarkLastUsedStorageReceivingBookmarks,
                    static_cast<int>(type));
}

const bookmarks::BookmarkNode* GetDefaultBookmarkFolder(
    PrefService* prefs,
    bool is_account_bookmark_model_available,
    bookmarks::BookmarkModel* profile_bookmark_model,
    bookmarks::BookmarkModel* account_bookmark_model) {
  int64_t node_id =
      prefs->GetInt64(prefs::kIosBookmarkLastUsedFolderReceivingBookmarks);

  if (node_id != kLastUsedBookmarkFolderNone) {
    bookmarks::StorageType type =
        static_cast<bookmarks::StorageType>(prefs->GetInteger(
            prefs::kIosBookmarkLastUsedStorageReceivingBookmarks));
    bookmarks::BookmarkModel* bookmark_model = GetBookmarkModelForType(
        type, profile_bookmark_model, account_bookmark_model);
    const BookmarkNode* result =
        bookmarks::GetBookmarkNodeByID(bookmark_model, node_id);
    if (result && result->is_folder()) {
      return result;
    }
  }

  // Either preferences is not set, or refers to a non-existing folder.
  bookmarks::StorageType type = (is_account_bookmark_model_available)
                                    ? bookmarks::StorageType::kAccount
                                    : bookmarks::StorageType::kLocalOrSyncable;
  bookmarks::BookmarkModel* bookmark_model = GetBookmarkModelForType(
      type, profile_bookmark_model, account_bookmark_model);
  return bookmark_model->mobile_node();
}
