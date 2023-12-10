// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/history/model/history_client_impl.h"

#include <set>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/time/time.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/browser/model_loader.h"
#include "components/history/core/browser/history_service.h"
#include "ios/chrome/browser/history/model/history_backend_client_impl.h"
#include "ios/chrome/browser/history/model/history_utils.h"
#include "url/gurl.h"

HistoryClientImpl::HistoryClientImpl(
    bookmarks::BookmarkModel* local_or_syncable_bookmark_model,
    bookmarks::BookmarkModel* account_bookmark_model)
    : local_or_syncable_bookmark_model_(local_or_syncable_bookmark_model),
      account_bookmark_model_(account_bookmark_model) {
  if (local_or_syncable_bookmark_model_) {
    bookmark_model_observations_.AddObservation(
        local_or_syncable_bookmark_model_);
  }
  if (account_bookmark_model_) {
    bookmark_model_observations_.AddObservation(account_bookmark_model_);
  }
}

HistoryClientImpl::~HistoryClientImpl() = default;

void HistoryClientImpl::StopObservingBookmarkModels() {
  local_or_syncable_bookmark_model_ = nullptr;
  account_bookmark_model_ = nullptr;
  bookmark_model_observations_.RemoveAllObservations();
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
  StopObservingBookmarkModels();
}

history::CanAddURLCallback HistoryClientImpl::GetThreadSafeCanAddURLCallback()
    const {
  return base::BindRepeating(&ios::CanAddURLToHistory);
}

void HistoryClientImpl::NotifyProfileError(sql::InitStatus init_status,
                                           const std::string& diagnostics) {}

std::unique_ptr<history::HistoryBackendClient>
HistoryClientImpl::CreateBackendClient() {
  std::vector<scoped_refptr<bookmarks::ModelLoader>> model_loaders;
  for (bookmarks::BookmarkModel* model :
       {local_or_syncable_bookmark_model_, account_bookmark_model_}) {
    if (!model) {
      continue;
    }
    scoped_refptr<bookmarks::ModelLoader> loader = model->model_loader();
    CHECK(loader);
    model_loaders.push_back(std::move(loader));
  }
  return std::make_unique<HistoryBackendClientImpl>(std::move(model_loaders));
}

void HistoryClientImpl::UpdateBookmarkLastUsedTime(
    const base::Uuid& bookmark_node_uuid,
    base::Time time) {
  for (bookmarks::BookmarkModel* bookmark_model :
       {local_or_syncable_bookmark_model_, account_bookmark_model_}) {
    if (!bookmark_model) {
      continue;
    }
    const bookmarks::BookmarkNode* node =
        bookmark_model->GetNodeByUuid(bookmark_node_uuid);
    if (!node) {
      continue;
    }
    // In the unlikely scenario where the two bookmark models have a bookmark
    // node with the same UUID, they are both updated.
    bookmark_model->UpdateLastUsedTime(node, time, /*just_opened=*/true);
  }
}

void HistoryClientImpl::BookmarkModelChanged() {
}

void HistoryClientImpl::BookmarkModelBeingDeleted(
    bookmarks::BookmarkModel* model) {
  StopObservingBookmarkModels();
}

void HistoryClientImpl::BookmarkNodeRemoved(
    bookmarks::BookmarkModel* model,
    const bookmarks::BookmarkNode* parent,
    size_t old_index,
    const bookmarks::BookmarkNode* node,
    const std::set<GURL>& no_longer_bookmarked) {
  HandleBookmarksRemovedFromModel(model, no_longer_bookmarked);
}

void HistoryClientImpl::BookmarkAllUserNodesRemoved(
    bookmarks::BookmarkModel* model,
    const std::set<GURL>& removed_urls) {
  HandleBookmarksRemovedFromModel(model, removed_urls);
}

void HistoryClientImpl::OnFaviconsChanged(const std::set<GURL>& page_urls,
                                          const GURL& favicon_url) {
  for (bookmarks::BookmarkModel* bookmark_model :
       {local_or_syncable_bookmark_model_, account_bookmark_model_}) {
    if (!bookmark_model) {
      continue;
    }
    bookmark_model->OnFaviconsChanged(page_urls, favicon_url);
  }
}

void HistoryClientImpl::HandleBookmarksRemovedFromModel(
    bookmarks::BookmarkModel* model,
    const std::set<GURL>& removed_urls) {
  CHECK(model == local_or_syncable_bookmark_model_ ||
        model == account_bookmark_model_);

  if (!on_bookmarks_removed_) {
    return;
  }

  // Only notify when bookmarks are removed from both models.
  bookmarks::BookmarkModel* other_model =
      model == local_or_syncable_bookmark_model_
          ? account_bookmark_model_
          : local_or_syncable_bookmark_model_;
  CHECK_NE(model, other_model);
  // Compute URLs that were removed from `model` and are not bookmarked in
  // `other_model`.
  std::set<GURL> removed_from_both_models = removed_urls;
  if (other_model) {
    std::erase_if(removed_from_both_models, [other_model](const GURL& url) {
      return other_model->IsBookmarked(url);
    });
  }

  if (!removed_from_both_models.empty()) {
    on_bookmarks_removed_.Run(removed_from_both_models);
  }
}
