// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HISTORY_HISTORY_CLIENT_IMPL_H_
#define IOS_CHROME_BROWSER_HISTORY_HISTORY_CLIENT_IMPL_H_

#include <memory>
#include <set>

#include "base/callback_forward.h"
#include "base/callback_list.h"
#include "base/macros.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "components/history/core/browser/history_client.h"

class GURL;

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}

class HistoryClientImpl : public history::HistoryClient,
                          public bookmarks::BaseBookmarkModelObserver {
 public:
  explicit HistoryClientImpl(bookmarks::BookmarkModel* bookmark_model);
  ~HistoryClientImpl() override;

 private:
  void StopObservingBookmarkModel();

  // history::HistoryClient implementation.
  void OnHistoryServiceCreated(
      history::HistoryService* history_service) override;
  void Shutdown() override;
  bool CanAddURL(const GURL& url) override;
  void NotifyProfileError(sql::InitStatus init_status,
                          const std::string& diagnostics) override;
  std::unique_ptr<history::HistoryBackendClient> CreateBackendClient() override;

  // bookmarks::BaseBookmarkModelObserver implementation.
  void BookmarkModelChanged() override;
  void BookmarkModelBeingDeleted(bookmarks::BookmarkModel* model) override;
  void BookmarkNodeRemoved(bookmarks::BookmarkModel* model,
                           const bookmarks::BookmarkNode* parent,
                           size_t old_index,
                           const bookmarks::BookmarkNode* node,
                           const std::set<GURL>& no_longer_bookmarked) override;
  void BookmarkAllUserNodesRemoved(bookmarks::BookmarkModel* model,
                                   const std::set<GURL>& removed_urls) override;

  // BookmarkModel instance providing access to bookmarks. May be null during
  // testing, and is null while shutting down.
  bookmarks::BookmarkModel* bookmark_model_;

  // Callback invoked when URLs are removed from BookmarkModel.
  base::Callback<void(const std::set<GURL>&)> on_bookmarks_removed_;

  // Subscription for notifications of changes to favicons.
  std::unique_ptr<base::CallbackList<void(const std::set<GURL>&,
                                          const GURL&)>::Subscription>
      favicons_changed_subscription_;

  DISALLOW_COPY_AND_ASSIGN(HistoryClientImpl);
};

#endif  // IOS_CHROME_BROWSER_HISTORY_HISTORY_CLIENT_IMPL_H_
