// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/bookmarks/model/bookmark_client_impl.h"

#include <utility>

#include "base/feature_list.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_storage.h"
#include "components/bookmarks/managed/managed_bookmark_service.h"
#include "components/browser_sync/browser_sync_switches.h"
#include "components/favicon/core/favicon_util.h"
#include "components/favicon_base/favicon_types.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/url_database.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/sync/base/features.h"
#include "components/sync_bookmarks/bookmark_model_view.h"
#include "components/sync_bookmarks/bookmark_sync_service.h"
#include "components/undo/bookmark_undo_service.h"
#include "ios/chrome/browser/bookmarks/model/bookmarks_utils.h"
#include "ios/chrome/browser/favicon/model/favicon_service_factory.h"
#include "ios/chrome/browser/history/model/history_service_factory.h"
#include "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"

BookmarkClientImpl::BookmarkClientImpl(
    ChromeBrowserState* browser_state,
    bookmarks::ManagedBookmarkService* managed_bookmark_service,
    sync_bookmarks::BookmarkSyncService*
        local_or_syncable_bookmark_sync_service,
    sync_bookmarks::BookmarkSyncService* account_bookmark_sync_service,
    BookmarkUndoService* bookmark_undo_service)
    : browser_state_(browser_state),
      managed_bookmark_service_(managed_bookmark_service),
      local_or_syncable_bookmark_sync_service_(
          local_or_syncable_bookmark_sync_service),
      account_bookmark_sync_service_(account_bookmark_sync_service),
      bookmark_undo_service_(bookmark_undo_service) {}

BookmarkClientImpl::~BookmarkClientImpl() {}

void BookmarkClientImpl::Init(bookmarks::BookmarkModel* model) {
  if (managed_bookmark_service_) {
    managed_bookmark_service_->BookmarkModelCreated(model);
  }
  model_ = model;
}

void BookmarkClientImpl::RequiredRecoveryToLoad(
    const std::multimap<int64_t, int64_t>&
        local_or_syncable_reassigned_ids_per_old_id) {
  if (!account_bookmark_sync_service_) {
    // `account_bookmark_sync_service_` being null means `this` does NOT deal
    // with two BookmarkSyncService instances. This implies there must be two
    // BookmarkClientImpl instances (one per BookmarkSyncService) and therefore
    // two BookmarkModel instances too. Do nothing in that case, as the
    // migration in this function is primarily about the case where this client
    // transitioned from having two BookmarkModel instances to having one.
    return;
  }

  if (browser_state_->GetPrefs()) {
    MigrateLastUsedBookmarkFolderUponLocalIdsReassigned(
        browser_state_->GetPrefs(),
        local_or_syncable_reassigned_ids_per_old_id);
  }
}

base::CancelableTaskTracker::TaskId
BookmarkClientImpl::GetFaviconImageForPageURL(
    const GURL& page_url,
    favicon_base::FaviconImageCallback callback,
    base::CancelableTaskTracker* tracker) {
  return favicon::GetFaviconImageForPageURL(
      ios::FaviconServiceFactory::GetForBrowserState(
          browser_state_, ServiceAccessType::EXPLICIT_ACCESS),
      page_url, favicon_base::IconType::kFavicon, std::move(callback), tracker);
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
    // If `url_db` is the InMemoryDatabase, it might not cache all URLRows, but
    // it guarantees to contain those with `typed_count` > 0. Thus, if fetching
    // the URLRow fails, it is safe to assume that its `typed_count` is 0.
    int typed_count = 0;
    history::URLRow url_row;
    const GURL* url = url_typed_count_pair.first;
    if (url_db && url && url_db->GetRowForURL(*url, &url_row)) {
      typed_count = url_row.typed_count();
    }

    url_typed_count_pair.second = typed_count;
  }
}

bookmarks::LoadManagedNodeCallback
BookmarkClientImpl::GetLoadManagedNodeCallback() {
  if (managed_bookmark_service_) {
    return managed_bookmark_service_->GetLoadManagedNodeCallback();
  }
  return bookmarks::LoadManagedNodeCallback();
}

bool BookmarkClientImpl::IsSyncFeatureEnabledIncludingBookmarks() {
  // `kMigrateSyncingUserToSignedIn` is only used as an extra safeguard to avoid
  // behavioral changes. If this feature is enabled, sync-the-feature can be
  // safely considered disabled, as the remaining cases where
  // `IsTrackingMetadata()` below returns true should be very rare, usually
  // error cases.
  return local_or_syncable_bookmark_sync_service_->IsTrackingMetadata() &&
         !base::FeatureList::IsEnabled(switches::kMigrateSyncingUserToSignedIn);
}

bool BookmarkClientImpl::CanSetPermanentNodeTitle(
    const bookmarks::BookmarkNode* permanent_node) {
  if (managed_bookmark_service_) {
    return managed_bookmark_service_->CanSetPermanentNodeTitle(permanent_node);
  }
  return true;
}

bool BookmarkClientImpl::IsNodeManaged(const bookmarks::BookmarkNode* node) {
  if (managed_bookmark_service_) {
    return managed_bookmark_service_->IsNodeManaged(node);
  }
  return false;
}

std::string BookmarkClientImpl::EncodeLocalOrSyncableBookmarkSyncMetadata() {
  return local_or_syncable_bookmark_sync_service_->EncodeBookmarkSyncMetadata();
}

std::string BookmarkClientImpl::EncodeAccountBookmarkSyncMetadata() {
  if (!account_bookmark_sync_service_) {
    CHECK(!base::FeatureList::IsEnabled(
        syncer::kEnableBookmarkFoldersForAccountStorage));

    // On iOS, for historic reasons and before rolling out
    // `syncer::kEnableBookmarkFoldersForAccountStorage`, a dedicated
    // BookmarkModel is used for account bookmarks and, counter-intuitively, the
    // local-or-syncable nodes within are used to represent account data. The
    // same is true for sync metadata, so account sync metadata remains unused.
    return std::string();
  }

  return account_bookmark_sync_service_->EncodeBookmarkSyncMetadata();
}

void BookmarkClientImpl::DecodeLocalOrSyncableBookmarkSyncMetadata(
    const std::string& metadata_str,
    const base::RepeatingClosure& schedule_save_closure) {
  local_or_syncable_bookmark_sync_service_->DecodeBookmarkSyncMetadata(
      metadata_str, schedule_save_closure,
      std::make_unique<
          sync_bookmarks::BookmarkModelViewUsingLocalOrSyncableNodes>(model_));
}

void BookmarkClientImpl::DecodeAccountBookmarkSyncMetadata(
    const std::string& metadata_str,
    const base::RepeatingClosure& schedule_save_closure) {
  if (account_bookmark_sync_service_) {
    account_bookmark_sync_service_->DecodeBookmarkSyncMetadata(
        metadata_str, schedule_save_closure,
        std::make_unique<sync_bookmarks::BookmarkModelViewUsingAccountNodes>(
            model_));
  }
}

void BookmarkClientImpl::OnBookmarkNodeRemovedUndoable(
    bookmarks::BookmarkModel* model,
    const bookmarks::BookmarkNode* parent,
    size_t index,
    std::unique_ptr<bookmarks::BookmarkNode> node) {
  bookmark_undo_service_->AddUndoEntryForRemovedNode(model, parent, index,
                                                     std::move(node));
}
