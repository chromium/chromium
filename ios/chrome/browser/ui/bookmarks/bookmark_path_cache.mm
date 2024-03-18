// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/bookmark_path_cache.h"

#import "components/bookmarks/browser/bookmark_node.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_model_type.h"
#import "ios/chrome/browser/bookmarks/model/legacy_bookmark_model.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"

using bookmarks::BookmarkNode;

namespace {
const int64_t kFolderNone = -1;
}  // namespace

@implementation BookmarkPathCache

+ (void)registerBrowserStatePrefs:(user_prefs::PrefRegistrySyncable*)registry {
  registry->RegisterInt64Pref(prefs::kIosBookmarkCachedFolderId, kFolderNone);
  registry->RegisterInt64Pref(
      prefs::kIosBookmarkCachedFolderModel,
      static_cast<int64_t>(BookmarkModelType::kLocalOrSyncable));
  registry->RegisterIntegerPref(prefs::kIosBookmarkCachedTopMostRow, 0);
}

+ (void)cacheBookmarkTopMostRowWithPrefService:(PrefService*)prefService
                                      folderId:(int64_t)folderId
                                   inModelType:(BookmarkModelType)modelType
                                    topMostRow:(int)topMostRow {
  prefService->SetInt64(prefs::kIosBookmarkCachedFolderId, folderId);
  prefService->SetInt64(prefs::kIosBookmarkCachedFolderModel,
                        static_cast<int64_t>(modelType));
  prefService->SetInteger(prefs::kIosBookmarkCachedTopMostRow, topMostRow);
}

+ (BOOL)getBookmarkTopMostRowCacheWithPrefService:(PrefService*)prefService
                             localOrSyncableModel:
                                 (LegacyBookmarkModel*)localOrSyncableModel
                                     accountModel:
                                         (LegacyBookmarkModel*)accountModel
                                         folderId:(int64_t*)folderId
                                        modelType:(BookmarkModelType*)modelType
                                       topMostRow:(int*)topMostRow {
  *folderId = prefService->GetInt64(prefs::kIosBookmarkCachedFolderId);
  *modelType = static_cast<BookmarkModelType>(
      prefService->GetInt64(prefs::kIosBookmarkCachedFolderModel));
  LegacyBookmarkModel* model;
  if (*modelType == BookmarkModelType::kLocalOrSyncable) {
    model = localOrSyncableModel;
  } else {
    model = accountModel;
  }

  // If the cache was at root node, consider it as nothing was cached.
  if (*folderId == kFolderNone ||
      *folderId == model->subtle_root_node_with_unspecified_children()->id()) {
    return NO;
  }

  // Create bookmark Path.
  const BookmarkNode* bookmark =
      bookmark_utils_ios::FindFolderById(model, *folderId);
  // The bookmark node is gone from model, maybe deleted remotely.
  if (!bookmark) {
    return NO;
  }

  *topMostRow = prefService->GetInteger(prefs::kIosBookmarkCachedTopMostRow);
  return YES;
}

+ (void)clearBookmarkTopMostRowCacheWithPrefService:(PrefService*)prefService {
  prefService->SetInt64(prefs::kIosBookmarkCachedFolderId, kFolderNone);
  prefService->SetInt64(
      prefs::kIosBookmarkCachedFolderModel,
      static_cast<int64_t>(BookmarkModelType::kLocalOrSyncable));
}

@end
