// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/history/model/history_client_impl.h"

#include <set>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/browser/model_loader.h"
#include "components/history/core/browser/history_service.h"
#include "ios/chrome/browser/history/model/history_backend_client_impl.h"
#include "ios/chrome/browser/history/model/history_utils.h"
#include "url/gurl.h"

HistoryClientImpl::HistoryClientImpl(bookmarks::BookmarkModel* bookmark_model)
    : bookmark_model_(bookmark_model) {
  if (bookmark_model_) {
    bookmark_model_observation_.Observe(bookmark_model_.get());
  }
}

HistoryClientImpl::~HistoryClientImpl() = default;

void HistoryClientImpl::StopObservingBookmarkModel() {
  bookmark_model_ = nullptr;
  bookmark_model_observation_.Reset();
}

void HistoryClientImpl::OnHistoryServiceCreated(
    history::HistoryService* history_service) {
  on_bookmarks_removed_ =
      base::BindRepeating(&history::HistoryService::URLsNoLongerBookmarked,
                          base::Unretained(history_service));
  favicons_changed_subscription_ =
      history_service->AddFaviconsChangedCallback(base::BindRepeating(
          &HistoryClientImpl::OnFaviconsChanged, base::Unretained(this)));
}

void HistoryClientImpl::Shutdown() {
  favicons_changed_subscription_ = {};
  StopObservingBookmarkModel();
}

history::CanAddURLCallback HistoryClientImpl::GetThreadSafeCanAddURLCallback()
    const {
  return base::BindRepeating(&ios::CanAddURLToHistory);
}

void HistoryClientImpl::NotifyProfileError(sql::InitStatus init_status,
                                           const std::string& diagnostics) {}

std::unique_ptr<history::HistoryBackendClient>
HistoryClientImpl::CreateBackendClient() {
  scoped_refptr<bookmarks::ModelLoader> loader;

  if (bookmark_model_) {
    loader = bookmark_model_->model_loader();
  }

  return std::make_unique<HistoryBackendClientImpl>(std::move(loader));
}

void HistoryClientImpl::UpdateBookmarkLastUsedTime(int64_t bookmark_node_id,
                                                   base::Time time) {
  if (!bookmark_model_) {
    return;
  }

  const bookmarks::BookmarkNode* node =
      GetBookmarkNodeByID(bookmark_model_, bookmark_node_id);

  // This call is async so the BookmarkNode could have already been deleted.
  if (!node) {
    return;
  }

  bookmark_model_->UpdateLastUsedTime(node, time,
                                      /*just_opened=*/true);
}

void HistoryClientImpl::BookmarkModelChanged() {
}

void HistoryClientImpl::BookmarkModelBeingDeleted() {
  StopObservingBookmarkModel();
}

void HistoryClientImpl::BookmarkNodeRemoved(
    const bookmarks::BookmarkNode* parent,
    size_t old_index,
    const bookmarks::BookmarkNode* node,
    const std::set<GURL>& no_longer_bookmarked,
    const base::Location& location) {
  HandleBookmarksRemovedFromModel(no_longer_bookmarked);
}

void HistoryClientImpl::BookmarkAllUserNodesRemoved(
    const std::set<GURL>& removed_urls,
    const base::Location& location) {
  HandleBookmarksRemovedFromModel(removed_urls);
}

void HistoryClientImpl::OnFaviconsChanged(const std::set<GURL>& page_urls,
                                          const GURL& favicon_url) {
  if (bookmark_model_) {
    bookmark_model_->OnFaviconsChanged(page_urls, favicon_url);
  }
}

void HistoryClientImpl::HandleBookmarksRemovedFromModel(
    const std::set<GURL>& removed_urls) {
  if (!on_bookmarks_removed_) {
    return;
  }

  on_bookmarks_removed_.Run(removed_urls);
}
