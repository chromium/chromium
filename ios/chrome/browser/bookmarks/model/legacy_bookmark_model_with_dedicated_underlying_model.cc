// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/bookmarks/model/legacy_bookmark_model_with_dedicated_underlying_model.h"

#include <utility>

#include "base/check.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/managed/managed_bookmark_service.h"

LegacyBookmarkModelWithDedicatedUnderlyingModel::
    LegacyBookmarkModelWithDedicatedUnderlyingModel(
        std::unique_ptr<bookmarks::BookmarkModel> underlying_model,
        bookmarks::ManagedBookmarkService* managed_bookmark_service)
    : underlying_model_(std::move(underlying_model)),
      managed_bookmark_service_(managed_bookmark_service) {
  CHECK(underlying_model_);
}

LegacyBookmarkModelWithDedicatedUnderlyingModel::
    ~LegacyBookmarkModelWithDedicatedUnderlyingModel() = default;

const bookmarks::BookmarkModel*
LegacyBookmarkModelWithDedicatedUnderlyingModel::underlying_model() const {
  return underlying_model_.get();
}

bookmarks::BookmarkModel*
LegacyBookmarkModelWithDedicatedUnderlyingModel::underlying_model() {
  return underlying_model_.get();
}

const bookmarks::BookmarkNode*
LegacyBookmarkModelWithDedicatedUnderlyingModel::bookmark_bar_node() const {
  return underlying_model()->bookmark_bar_node();
}

const bookmarks::BookmarkNode*
LegacyBookmarkModelWithDedicatedUnderlyingModel::other_node() const {
  return underlying_model()->other_node();
}

const bookmarks::BookmarkNode*
LegacyBookmarkModelWithDedicatedUnderlyingModel::mobile_node() const {
  return underlying_model()->mobile_node();
}

const bookmarks::BookmarkNode*
LegacyBookmarkModelWithDedicatedUnderlyingModel::managed_node() const {
  return managed_bookmark_service_ ? managed_bookmark_service_->managed_node()
                                   : nullptr;
}

bool LegacyBookmarkModelWithDedicatedUnderlyingModel::IsBookmarked(
    const GURL& url) const {
  return underlying_model()->IsBookmarked(url);
}

bool LegacyBookmarkModelWithDedicatedUnderlyingModel::is_permanent_node(
    const bookmarks::BookmarkNode* node) const {
  return underlying_model()->is_permanent_node(node);
}

void LegacyBookmarkModelWithDedicatedUnderlyingModel::AddObserver(
    bookmarks::BookmarkModelObserver* observer) {
  underlying_model()->AddObserver(observer);
}

void LegacyBookmarkModelWithDedicatedUnderlyingModel::RemoveObserver(
    bookmarks::BookmarkModelObserver* observer) {
  underlying_model()->RemoveObserver(observer);
}

std::vector<raw_ptr<const bookmarks::BookmarkNode, VectorExperimental>>
LegacyBookmarkModelWithDedicatedUnderlyingModel::GetNodesByURL(
    const GURL& url) const {
  return underlying_model()->GetNodesByURL(url);
}

const bookmarks::BookmarkNode*
LegacyBookmarkModelWithDedicatedUnderlyingModel::GetNodeByUuid(
    const base::Uuid& uuid) const {
  return underlying_model()->GetNodeByUuid(
      uuid,
      bookmarks::BookmarkModel::NodeTypeForUuidLookup::kLocalOrSyncableNodes);
}

const bookmarks::BookmarkNode* LegacyBookmarkModelWithDedicatedUnderlyingModel::
    GetMostRecentlyAddedUserNodeForURL(const GURL& url) const {
  return underlying_model()->GetMostRecentlyAddedUserNodeForURL(url);
}

bool LegacyBookmarkModelWithDedicatedUnderlyingModel::HasBookmarks() const {
  return underlying_model()->HasBookmarks();
}

std::vector<const bookmarks::BookmarkNode*>
LegacyBookmarkModelWithDedicatedUnderlyingModel::GetBookmarksMatchingProperties(
    const bookmarks::QueryFields& query,
    size_t max_count) {
  return bookmarks::GetBookmarksMatchingProperties(underlying_model(), query,
                                                   max_count);
}

const bookmarks::BookmarkNode*
LegacyBookmarkModelWithDedicatedUnderlyingModel::GetNodeById(int64_t id) {
  return bookmarks::GetBookmarkNodeByID(underlying_model(), id);
}

bool LegacyBookmarkModelWithDedicatedUnderlyingModel::IsNodePartOfModel(
    const bookmarks::BookmarkNode* node) const {
  return node && node->HasAncestor(underlying_model_->root_node());
}

const bookmarks::BookmarkNode* LegacyBookmarkModelWithDedicatedUnderlyingModel::
    MoveToOtherModelPossiblyWithNewNodeIdsAndUuids(
        const bookmarks::BookmarkNode* node,
        LegacyBookmarkModel* dest_model,
        const bookmarks::BookmarkNode* dest_parent) {
  return underlying_model()->MoveToOtherModelWithNewNodeIdsAndUuids(
      node,
      static_cast<LegacyBookmarkModelWithDedicatedUnderlyingModel*>(dest_model)
          ->underlying_model(),
      dest_parent);
}

base::WeakPtr<LegacyBookmarkModel>
LegacyBookmarkModelWithDedicatedUnderlyingModel::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}
