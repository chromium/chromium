// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BOOKMARKS_MODEL_BOOKMARK_CLIENT_IMPL_H_
#define IOS_CHROME_BROWSER_BOOKMARKS_MODEL_BOOKMARK_CLIENT_IMPL_H_

#import <set>
#import <string>
#import <vector>

#import "base/memory/raw_ptr.h"
#import "base/task/deferred_sequenced_task_runner.h"
#import "components/power_bookmarks/core/bookmark_client_base.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

class BookmarkUndoService;
class GURL;

namespace bookmarks {
class BookmarkModel;
class ManagedBookmarkService;
}  // namespace bookmarks

namespace sync_bookmarks {
class BookmarkSyncService;
}  // namespace sync_bookmarks

class BookmarkClientImpl : public power_bookmarks::BookmarkClientBase {
 public:
  BookmarkClientImpl(
      ProfileIOS* profile,
      bookmarks::ManagedBookmarkService* managed_bookmark_service,
      sync_bookmarks::BookmarkSyncService*
          local_or_syncable_bookmark_sync_service,
      sync_bookmarks::BookmarkSyncService* account_bookmark_sync_service,
      BookmarkUndoService* bookmark_undo_service);

  BookmarkClientImpl(const BookmarkClientImpl&) = delete;
  BookmarkClientImpl& operator=(const BookmarkClientImpl&) = delete;

  ~BookmarkClientImpl() override;

  void SetIsSyncFeatureEnabledIncludingBookmarksForTest();

  // bookmarks::BookmarkClient:
  void Init(bookmarks::BookmarkModel* model) override;
  void RequiredRecoveryToLoad(
      const std::multimap<int64_t, int64_t>&
          local_or_syncable_reassigned_ids_per_old_id) override;
  base::CancelableTaskTracker::TaskId GetFaviconImageForPageURL(
      const GURL& page_url,
      favicon_base::FaviconImageCallback callback,
      base::CancelableTaskTracker* tracker) override;
  bool SupportsTypedCountForUrls() override;
  void GetTypedCountForUrls(UrlTypedCountMap* url_typed_count_map) override;
  bookmarks::LoadManagedNodeCallback GetLoadManagedNodeCallback() override;
  bool IsSyncFeatureEnabledIncludingBookmarks() override;
  bool CanSetPermanentNodeTitle(
      const bookmarks::BookmarkNode* permanent_node) override;
  bool IsNodeManaged(const bookmarks::BookmarkNode* node) override;
  std::string EncodeLocalOrSyncableBookmarkSyncMetadata() override;
  std::string EncodeAccountBookmarkSyncMetadata() override;
  void DecodeLocalOrSyncableBookmarkSyncMetadata(
      const std::string& metadata_str,
      const base::RepeatingClosure& schedule_save_closure) override;
  void DecodeAccountBookmarkSyncMetadata(
      const std::string& metadata_str,
      const base::RepeatingClosure& schedule_save_closure) override;
  void OnBookmarkNodeRemovedUndoable(
      const bookmarks::BookmarkNode* parent,
      size_t index,
      std::unique_ptr<bookmarks::BookmarkNode> node) override;

 private:
  // Pointer to the associated ProfileIOS. Must outlive
  // BookmarkClientImpl.
  const raw_ptr<ProfileIOS> profile_;

  // Pointer to the ManagedBookmarkService responsible for bookmark policy. May
  // be null during testing.
  const raw_ptr<bookmarks::ManagedBookmarkService> managed_bookmark_service_;

  // Pointers to the two BookmarkSyncService instances responsible for encoding
  // and decoding sync metadata persisted together with the bookmarks model.
  const raw_ptr<sync_bookmarks::BookmarkSyncService>
      local_or_syncable_bookmark_sync_service_;
  const raw_ptr<sync_bookmarks::BookmarkSyncService>
      account_bookmark_sync_service_;

  // Pointer to BookmarkUndoService, responsible for making operations undoable.
  const raw_ptr<BookmarkUndoService> bookmark_undo_service_;

  raw_ptr<bookmarks::BookmarkModel> model_ = nullptr;

  bool is_sync_feature_enabled_including_bookmarks_for_test_ = false;
};

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_MODEL_BOOKMARK_CLIENT_IMPL_H_
