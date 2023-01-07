// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BOOKMARKS_BOOKMARK_CLIENT_IMPL_H_
#define IOS_CHROME_BROWSER_BOOKMARKS_BOOKMARK_CLIENT_IMPL_H_

#include <set>
#include <string>
#include <vector>

#include "base/task/deferred_sequenced_task_runner.h"
#include "components/bookmarks/browser/bookmark_client.h"

class ChromeBrowserState;
class GURL;

namespace bookmarks {
class BookmarkModel;
class ManagedBookmarkService;
}

namespace sync_bookmarks {
class BookmarkSyncService;
}

class BookmarkClientImpl : public bookmarks::BookmarkClient {
 public:
  BookmarkClientImpl(
      ChromeBrowserState* browser_state,
      bookmarks::ManagedBookmarkService* managed_bookmark_service,
      sync_bookmarks::BookmarkSyncService* bookmark_sync_service);

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
  void RecordAction(const base::UserMetricsAction& action) override;
  bookmarks::LoadManagedNodeCallback GetLoadManagedNodeCallback() override;
  bool CanSetPermanentNodeTitle(
      const bookmarks::BookmarkNode* permanent_node) override;
  bool CanSyncNode(const bookmarks::BookmarkNode* node) override;
  bool CanBeEditedByUser(const bookmarks::BookmarkNode* node) override;
  std::string EncodeBookmarkSyncMetadata() override;
  void DecodeBookmarkSyncMetadata(
      const std::string& metadata_str,
      const base::RepeatingClosure& schedule_save_closure) override;

 private:
  // Pointer to the associated ChromeBrowserState. Must outlive
  // BookmarkClientImpl.
  ChromeBrowserState* browser_state_ = nullptr;

  // Pointer to the ManagedBookmarkService responsible for bookmark policy. May
  // be null during testing.
  bookmarks::ManagedBookmarkService* managed_bookmark_service_ = nullptr;

  bookmarks::BookmarkModel* model_ = nullptr;

  // Pointer to the BookmarkSyncService responsible for encoding and decoding
  // sync metadata persisted together with the bookmarks model.
  sync_bookmarks::BookmarkSyncService* bookmark_sync_service_ = nullptr;
};

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_BOOKMARK_CLIENT_IMPL_H_
