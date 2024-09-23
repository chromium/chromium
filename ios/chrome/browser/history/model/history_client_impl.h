// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HISTORY_MODEL_HISTORY_CLIENT_IMPL_H_
#define IOS_CHROME_BROWSER_HISTORY_MODEL_HISTORY_CLIENT_IMPL_H_

#include <memory>
#include <set>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#import "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "components/history/core/browser/history_client.h"
#include "components/history/core/browser/history_service.h"

class GURL;

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}

class HistoryClientImpl : public history::HistoryClient,
                          public bookmarks::BaseBookmarkModelObserver {
 public:
  explicit HistoryClientImpl(bookmarks::BookmarkModel* bookmark_model);

  HistoryClientImpl(const HistoryClientImpl&) = delete;
  HistoryClientImpl& operator=(const HistoryClientImpl&) = delete;

  ~HistoryClientImpl() override;

 private:
  void StopObservingBookmarkModel();

  // history::HistoryClient implementation.
  void OnHistoryServiceCreated(
      history::HistoryService* history_service) override;
  void Shutdown() override;
  history::CanAddURLCallback GetThreadSafeCanAddURLCallback() const override;
  void NotifyProfileError(sql::InitStatus init_status,
                          const std::string& diagnostics) override;
  std::unique_ptr<history::HistoryBackendClient> CreateBackendClient() override;
  void UpdateBookmarkLastUsedTime(int64_t bookmark_node_id,
                                  base::Time time) override;

  // bookmarks::BaseBookmarkModelObserver implementation.
  void BookmarkModelChanged() override;
  void BookmarkModelBeingDeleted() override;
  void BookmarkNodeRemoved(const bookmarks::BookmarkNode* parent,
                           size_t old_index,
                           const bookmarks::BookmarkNode* node,
                           const std::set<GURL>& no_longer_bookmarked,
                           const base::Location& location) override;
  void BookmarkAllUserNodesRemoved(const std::set<GURL>& removed_urls,
                                   const base::Location& location) override;

  // Callback registered in `favicons_changed_subscription_`.
  void OnFaviconsChanged(const std::set<GURL>& page_urls,
                         const GURL& favicon_url);

  // Called when bookmarks are removed from a model and calls
  // `on_bookmarks_removed_`.
  void HandleBookmarksRemovedFromModel(const std::set<GURL>& removed_urls);

  // BookmarkModel instance providing access to bookmarks. May be null.
  raw_ptr<bookmarks::BookmarkModel> bookmark_model_ = nullptr;

  // Callback invoked when URLs are removed from BookmarkModel.
  base::RepeatingCallback<void(const std::set<GURL>&)> on_bookmarks_removed_;

  // Subscription for notifications of changes to favicons.
  base::CallbackListSubscription favicons_changed_subscription_;

  base::ScopedObservation<bookmarks::BookmarkModel,
                          bookmarks::BaseBookmarkModelObserver>
      bookmark_model_observation_{this};
};

#endif  // IOS_CHROME_BROWSER_HISTORY_MODEL_HISTORY_CLIENT_IMPL_H_
