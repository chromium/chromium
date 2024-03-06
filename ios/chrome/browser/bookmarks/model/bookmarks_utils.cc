// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/bookmarks/model/bookmarks_utils.h"

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/metrics/histogram_macros.h"
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

  if (node_id != kLastUsedBookmarkFolderNone) {
    BookmarkModelType type = static_cast<BookmarkModelType>(prefs->GetInteger(
        prefs::kIosBookmarkLastUsedStorageReceivingBookmarks));
    LegacyBookmarkModel* bookmark_model = GetBookmarkModelForType(
        type, local_or_syncable_bookmark_model, account_bookmark_model);
    const BookmarkNode* result = bookmark_model->GetNodeById(node_id);
    if (result && result->is_folder()) {
      // Make sure the bookmark node is a folder. See crbug.com/1450146.
      return result;
    }
  }

  // Either preferences is not set, or refers to a non-existing folder.
  BookmarkModelType type = (is_account_bookmark_model_available)
                               ? BookmarkModelType::kAccount
                               : BookmarkModelType::kLocalOrSyncable;
  LegacyBookmarkModel* bookmark_model = GetBookmarkModelForType(
      type, local_or_syncable_bookmark_model, account_bookmark_model);
  return bookmark_model->mobile_node();
}
