// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/model/bookmark_model_metrics_service.h"

#import "components/bookmarks/browser/bookmark_model.h"
#import "ios/chrome/browser/crash_report/model/crash_keys_helper.h"

BookmarkModelMetricsService::BookmarkModelMetricsService(
    bookmarks::BookmarkModel* bookmark_model,
    ProfileIOS* profile)
    : bookmark_model_(bookmark_model), profile_(profile) {
  CHECK(bookmark_model_);
  if (bookmark_model_->loaded()) {
    UpdateBookmarkNodesCrashKey();
  }
  bookmark_model_observer_.Observe(bookmark_model_);
}

BookmarkModelMetricsService::~BookmarkModelMetricsService() {
  bookmark_model_observer_.Reset();
}

void BookmarkModelMetricsService::UpdateBookmarkNodesCrashKey() {
  if (in_extensive_changes_) {
    return;
  }

  crash_keys::SetBookmarkNodesCount(
      bookmark_model_->GetTotalNumberOfUrlsAndFoldersIncludingManagedNodes(),
      profile_);
}

void BookmarkModelMetricsService::BookmarkNodeAdded(
    const bookmarks::BookmarkNode* parent,
    size_t index,
    bool added_by_user) {
  UpdateBookmarkNodesCrashKey();
}

void BookmarkModelMetricsService::BookmarkNodeRemoved(
    const bookmarks::BookmarkNode* parent,
    size_t old_index,
    const bookmarks::BookmarkNode* node,
    const std::set<GURL>& removed_urls,
    const base::Location& location) {
  UpdateBookmarkNodesCrashKey();
}

void BookmarkModelMetricsService::BookmarkModelLoaded(bool ids_reassigned) {
  UpdateBookmarkNodesCrashKey();
}

void BookmarkModelMetricsService::BookmarkAllUserNodesRemoved(
    const std::set<GURL>& removed_urls,
    const base::Location& location) {
  UpdateBookmarkNodesCrashKey();
}

void BookmarkModelMetricsService::BookmarkModelBeingDeleted() {
  NOTREACHED();
}

void BookmarkModelMetricsService::ExtensiveBookmarkChangesBeginning() {
  in_extensive_changes_ = true;
}

void BookmarkModelMetricsService::ExtensiveBookmarkChangesEnded() {
  in_extensive_changes_ = false;
  UpdateBookmarkNodesCrashKey();
}

void BookmarkModelMetricsService::BookmarkNodeMoved(
    const bookmarks::BookmarkNode* old_parent,
    size_t old_index,
    const bookmarks::BookmarkNode* new_parent,
    size_t new_index) {}

void BookmarkModelMetricsService::BookmarkNodeChanged(
    const bookmarks::BookmarkNode* node) {}

void BookmarkModelMetricsService::BookmarkNodeFaviconChanged(
    const bookmarks::BookmarkNode* node) {}

void BookmarkModelMetricsService::BookmarkNodeChildrenReordered(
    const bookmarks::BookmarkNode* node) {}
