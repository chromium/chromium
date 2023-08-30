// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BOOKMARKS_MODEL_BOOKMARK_CLIENT_IMPL_H_
#define IOS_CHROME_BROWSER_BOOKMARKS_MODEL_BOOKMARK_CLIENT_IMPL_H_

#include <set>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/task/deferred_sequenced_task_runner.h"
#include "components/bookmarks/common/storage_type.h"
#include "components/power_bookmarks/core/bookmark_client_base.h"

class BookmarkUndoService;
class ChromeBrowserState;
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
      ChromeBrowserState* browser_state,
      bookmarks::ManagedBookmarkService* managed_bookmark_service,
      sync_bookmarks::BookmarkSyncService* bookmark_sync_service,
      BookmarkUndoService* bookmark_undo_service,
      bookmarks::StorageType storage_type_for_uma);

  BookmarkClientImpl(const BookmarkClientImpl&) = delete;
  BookmarkClientImpl& operator=(const BookmarkClientImpl&) = delete;

  ~BookmarkClientImpl() override;

  // bookmarks::BookmarkClient:
  void Init(bookmarks::BookmarkModel* model) override;
  base::CancelableTaskTracker::TaskId GetFaviconImageForPageURL(
      const GURL& page_url,
      favicon_base::FaviconImageCallback callback,
      base::CancelableTaskTracker* tracker) override;
  bool SupportsTypedCountForUrls() override;
  void GetTypedCountForUrls(UrlTypedCountMap* url_typed_count_map) override;
  bool IsPermanentNodeVisibleWhenEmpty(
      bookmarks::BookmarkNode::Type type) override;
  bookmarks::LoadManagedNodeCallback GetLoadManagedNodeCallback() override;
  bookmarks::metrics::StorageStateForUma GetStorageStateForUma() override;
  bool CanSetPermanentNodeTitle(
      const bookmarks::BookmarkNode* permanent_node) override;
  bool CanSyncNode(const bookmarks::BookmarkNode* node) override;
  bool CanBeEditedByUser(const bookmarks::BookmarkNode* node) override;
  std::string EncodeBookmarkSyncMetadata() override;
  void DecodeBookmarkSyncMetadata(
      const std::string& metadata_str,
      const base::RepeatingClosure& schedule_save_closure) override;
  void OnBookmarkNodeRemovedUndoable(
      bookmarks::BookmarkModel* model,
      const bookmarks::BookmarkNode* parent,
      size_t index,
      std::unique_ptr<bookmarks::BookmarkNode> node) override;

 private:
  // Pointer to the associated ChromeBrowserState. Must outlive
  // BookmarkClientImpl.
  const raw_ptr<ChromeBrowserState> browser_state_;

  // Pointer to the ManagedBookmarkService responsible for bookmark policy. May
  // be null during testing.
  const raw_ptr<bookmarks::ManagedBookmarkService> managed_bookmark_service_;

  // Pointer to the BookmarkSyncService responsible for encoding and decoding
  // sync metadata persisted together with the bookmarks model.
  const raw_ptr<sync_bookmarks::BookmarkSyncService> bookmark_sync_service_;

  // Pointer to BookmarkUndoService, responsible for making operations undoable.
  const raw_ptr<BookmarkUndoService> bookmark_undo_service_;

  const bookmarks::StorageType storage_type_for_uma_;

  raw_ptr<bookmarks::BookmarkModel> model_ = nullptr;
};

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_MODEL_BOOKMARK_CLIENT_IMPL_H_
