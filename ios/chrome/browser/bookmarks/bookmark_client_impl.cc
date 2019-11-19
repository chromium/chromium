// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/bookmarks/bookmark_client_impl.h"

#include "base/logging.h"
#include "base/metrics/user_metrics.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_storage.h"
#include "components/favicon/core/favicon_util.h"
#include "components/favicon_base/favicon_types.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/url_database.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/sync_bookmarks/bookmark_sync_service.h"
#include "ios/chrome/browser/favicon/favicon_service_factory.h"
#include "ios/chrome/browser/history/history_service_factory.h"

BookmarkClientImpl::BookmarkClientImpl(
    ios::ChromeBrowserState* browser_state,
    sync_bookmarks::BookmarkSyncService* bookmark_sync_service)
    : browser_state_(browser_state),
      bookmark_sync_service_(bookmark_sync_service) {}

BookmarkClientImpl::~BookmarkClientImpl() {}

void BookmarkClientImpl::Init(bookmarks::BookmarkModel* model) {
  model_ = model;
}

bool BookmarkClientImpl::PreferTouchIcon() {
  return true;
}

base::CancelableTaskTracker::TaskId
BookmarkClientImpl::GetFaviconImageForPageURL(
    const GURL& page_url,
    favicon_base::IconType type,
    favicon_base::FaviconImageCallback callback,
    base::CancelableTaskTracker* tracker) {
  return favicon::GetFaviconImageForPageURL(
      ios::FaviconServiceFactory::GetForBrowserState(
          browser_state_, ServiceAccessType::EXPLICIT_ACCESS),
      page_url, type, std::move(callback), tracker);
}

bool BookmarkClientImpl::SupportsTypedCountForUrls() {
  return true;
}

void BookmarkClientImpl::GetTypedCountForUrls(
    UrlTypedCountMap* url_typed_count_map) {
  history::HistoryService* history_service =
      ios::HistoryServiceFactory::GetForBrowserState(
          browser_state_, ServiceAccessType::EXPLICIT_ACCESS);
  history::URLDatabase* url_db =
      history_service ? history_service->InMemoryDatabase() : nullptr;
  for (auto& url_typed_count_pair : *url_typed_count_map) {
    // If |url_db| is the InMemoryDatabase, it might not cache all URLRows, but
    // it guarantees to contain those with |typed_count| > 0. Thus, if fetching
    // the URLRow fails, it is safe to assume that its |typed_count| is 0.
    int typed_count = 0;
    history::URLRow url_row;
    const GURL* url = url_typed_count_pair.first;
    if (url_db && url && url_db->GetRowForURL(*url, &url_row))
      typed_count = url_row.typed_count();

    url_typed_count_pair.second = typed_count;
  }
}

bool BookmarkClientImpl::IsPermanentNodeVisible(
    const bookmarks::BookmarkPermanentNode* node) {
  return node->type() == bookmarks::BookmarkNode::MOBILE;
}

void BookmarkClientImpl::RecordAction(const base::UserMetricsAction& action) {
  base::RecordAction(action);
}

bookmarks::LoadManagedNodeCallback
BookmarkClientImpl::GetLoadManagedNodeCallback() {
  return bookmarks::LoadManagedNodeCallback();
}

bool BookmarkClientImpl::CanSetPermanentNodeTitle(
    const bookmarks::BookmarkNode* permanent_node) {
  return true;
}

bool BookmarkClientImpl::CanSyncNode(const bookmarks::BookmarkNode* node) {
  return true;
}

bool BookmarkClientImpl::CanBeEditedByUser(
    const bookmarks::BookmarkNode* node) {
  return true;
}

std::string BookmarkClientImpl::EncodeBookmarkSyncMetadata() {
  return bookmark_sync_service_->EncodeBookmarkSyncMetadata();
}

void BookmarkClientImpl::DecodeBookmarkSyncMetadata(
    const std::string& metadata_str,
    const base::RepeatingClosure& schedule_save_closure) {
  bookmark_sync_service_->DecodeBookmarkSyncMetadata(
      metadata_str, schedule_save_closure, model_);
}
