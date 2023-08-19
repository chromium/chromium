// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/bookmark_path_cache.h"

#import "components/bookmarks/browser/bookmark_model.h"
#import "components/bookmarks/browser/bookmark_node.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

namespace {
const int64_t kFolderNone = -1;
}  // namespace

@implementation BookmarkPathCache

+ (void)registerBrowserStatePrefs:(user_prefs::PrefRegistrySyncable*)registry {
  registry->RegisterInt64Pref(prefs::kIosBookmarkCachedFolderId, kFolderNone);
  registry->RegisterIntegerPref(prefs::kIosBookmarkCachedTopMostRow, 0);
}

+ (void)cacheBookmarkTopMostRowWithPrefService:(PrefService*)prefService
                                      folderId:(int64_t)folderId
                                    topMostRow:(int)topMostRow {
  prefService->SetInt64(prefs::kIosBookmarkCachedFolderId, folderId);
  prefService->SetInteger(prefs::kIosBookmarkCachedTopMostRow, topMostRow);
}

+ (BOOL)getBookmarkTopMostRowCacheWithPrefService:(PrefService*)prefService
                                            model:
                                                (bookmarks::BookmarkModel*)model
                                         folderId:(int64_t*)folderId
                                       topMostRow:(int*)topMostRow {
  *folderId = prefService->GetInt64(prefs::kIosBookmarkCachedFolderId);

  // If the cache was at root node, consider it as nothing was cached.
  if (*folderId == kFolderNone || *folderId == model->root_node()->id()) {
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
}

@end
