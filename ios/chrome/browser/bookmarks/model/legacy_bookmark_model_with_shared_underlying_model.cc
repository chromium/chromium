// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/bookmarks/model/legacy_bookmark_model_with_shared_underlying_model.h"

#include <utility>

#include "base/check.h"
#include "components/bookmarks/browser/bookmark_client.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/managed/managed_bookmark_service.h"

namespace {

using NodeTypeForUuidLookup = bookmarks::BookmarkModel::NodeTypeForUuidLookup;

// Function used to sort nodes by their creation time. It returns true if `n1`
// was created after `n2`.
bool MoreRecentlyAdded(const bookmarks::BookmarkNode* n1,
                       const bookmarks::BookmarkNode* n2) {
  return n1->date_added() > n2->date_added();
}

const bookmarks::BookmarkNode* GetAncestorPermanentFolder(
    const bookmarks::BookmarkNode* node) {
  CHECK(node);
  CHECK(!node->is_root());

  const bookmarks::BookmarkNode* self_or_ancestor = node;

  while (!self_or_ancestor->is_permanent_node()) {
    self_or_ancestor = self_or_ancestor->parent();
    CHECK(self_or_ancestor);
  }

  return self_or_ancestor;
}

bool HasBookmarksRecursive(const bookmarks::BookmarkNode* node) {
  CHECK(node);
  if (node->is_url()) {
    return true;
  }
  for (auto& child : node->children()) {
    if (HasBookmarksRecursive(child.get())) {
      return true;
    }
  }
  return false;
}

}  // namespace

LegacyBookmarkModelWithSharedUnderlyingModel::NodeExcludedFromViewPredicate::
    NodeExcludedFromViewPredicate(
        const bookmarks::BookmarkNode* bookmark_bar_node,
        const bookmarks::BookmarkNode* other_node,
        const bookmarks::BookmarkNode* mobile_node,
        const bookmarks::BookmarkNode* managed_node)
    : bookmark_bar_node_(bookmark_bar_node),
      other_node_(other_node),
      mobile_node_(mobile_node),
      managed_node_(managed_node) {}

LegacyBookmarkModelWithSharedUnderlyingModel::NodeExcludedFromViewPredicate::
    ~NodeExcludedFromViewPredicate() = default;

bool LegacyBookmarkModelWithSharedUnderlyingModel::
    NodeExcludedFromViewPredicate::operator()(
        const bookmarks::BookmarkNode* node) {
  CHECK(node);
  if (node->is_root()) {
    // Special-case the root and allow the exposure, as it is also exposed as
    // parent of permanent folders.
    return false;
  }

  const bookmarks::BookmarkNode* ancestor_permanent_folder =
      GetAncestorPermanentFolder(node);
  CHECK(ancestor_permanent_folder);
  CHECK(ancestor_permanent_folder->is_permanent_node());

  return ancestor_permanent_folder != bookmark_bar_node_ &&
         ancestor_permanent_folder != other_node_ &&
         ancestor_permanent_folder != mobile_node_ &&
         ancestor_permanent_folder != managed_node_;
}

LegacyBookmarkModelWithSharedUnderlyingModel::
    LegacyBookmarkModelWithSharedUnderlyingModel(
        bookmarks::BookmarkModel* underlying_model,
        NodeTypeForUuidLookup node_type_for_uuid_lookup,
        bookmarks::ManagedBookmarkService* managed_bookmark_service)
    : underlying_model_(underlying_model),
      node_type_for_uuid_lookup_(node_type_for_uuid_lookup),
      managed_bookmark_service_(managed_bookmark_service) {
  CHECK(underlying_model_);
  scoped_observation_.Observe(underlying_model_);
}

LegacyBookmarkModelWithSharedUnderlyingModel::
    ~LegacyBookmarkModelWithSharedUnderlyingModel() = default;

const bookmarks::BookmarkModel*
LegacyBookmarkModelWithSharedUnderlyingModel::underlying_model() const {
  return underlying_model_.get();
}

bookmarks::BookmarkModel*
LegacyBookmarkModelWithSharedUnderlyingModel::underlying_model() {
  return underlying_model_.get();
}

const bookmarks::BookmarkNode*
LegacyBookmarkModelWithSharedUnderlyingModel::bookmark_bar_node() const {
  return node_type_for_uuid_lookup_ ==
                 NodeTypeForUuidLookup::kLocalOrSyncableNodes
             ? underlying_model()->bookmark_bar_node()
             : underlying_model()->account_bookmark_bar_node();
}

const bookmarks::BookmarkNode*
LegacyBookmarkModelWithSharedUnderlyingModel::other_node() const {
  return node_type_for_uuid_lookup_ ==
                 NodeTypeForUuidLookup::kLocalOrSyncableNodes
             ? underlying_model()->other_node()
             : underlying_model()->account_other_node();
}

const bookmarks::BookmarkNode*
LegacyBookmarkModelWithSharedUnderlyingModel::mobile_node() const {
  return node_type_for_uuid_lookup_ ==
                 NodeTypeForUuidLookup::kLocalOrSyncableNodes
             ? underlying_model()->mobile_node()
             : underlying_model()->account_mobile_node();
}

bool LegacyBookmarkModelWithSharedUnderlyingModel::IsBookmarked(
    const GURL& url) const {
  return !GetNodesByURL(url).empty();
}

bool LegacyBookmarkModelWithSharedUnderlyingModel::is_permanent_node(
    const bookmarks::BookmarkNode* node) const {
  return underlying_model()->is_permanent_node(node);
}

void LegacyBookmarkModelWithSharedUnderlyingModel::AddObserver(
    bookmarks::BookmarkModelObserver* observer) {
  observers_.AddObserver(observer);
}

void LegacyBookmarkModelWithSharedUnderlyingModel::RemoveObserver(
    bookmarks::BookmarkModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

std::vector<raw_ptr<const bookmarks::BookmarkNode, VectorExperimental>>
LegacyBookmarkModelWithSharedUnderlyingModel::GetNodesByURL(
    const GURL& url) const {
  std::vector<raw_ptr<const bookmarks::BookmarkNode, VectorExperimental>>
      nodes = underlying_model()->GetNodesByURL(url);
  std::erase_if(nodes, GetNodeExcludedFromViewPredicate());
  return nodes;
}

const bookmarks::BookmarkNode*
LegacyBookmarkModelWithSharedUnderlyingModel::GetNodeByUuid(
    const base::Uuid& uuid) const {
  return underlying_model()->GetNodeByUuid(uuid, node_type_for_uuid_lookup_);
}

const bookmarks::BookmarkNode* LegacyBookmarkModelWithSharedUnderlyingModel::
    GetMostRecentlyAddedUserNodeForURL(const GURL& url) const {
  std::vector<raw_ptr<const bookmarks::BookmarkNode, VectorExperimental>>
      nodes = GetNodesByURL(url);
  std::sort(nodes.begin(), nodes.end(), &MoreRecentlyAdded);

  // Look for the first node that the user can edit.
  for (const bookmarks::BookmarkNode* node : nodes) {
    if (!underlying_model_->client()->IsNodeManaged(node) &&
        !IsNodeExcludedFromView(node)) {
      return node;
    }
  }

  return nullptr;
}

bool LegacyBookmarkModelWithSharedUnderlyingModel::HasBookmarks() const {
  for (const bookmarks::BookmarkNode* permanent_node :
       {bookmark_bar_node(), other_node(), mobile_node(), managed_node()}) {
    if (permanent_node && HasBookmarksRecursive(permanent_node)) {
      return true;
    }
  }
  return false;
}

std::vector<const bookmarks::BookmarkNode*>
LegacyBookmarkModelWithSharedUnderlyingModel::GetBookmarksMatchingProperties(
    const bookmarks::QueryFields& query,
    size_t max_count) {
  std::vector<const bookmarks::BookmarkNode*> nodes =
      bookmarks::GetBookmarksMatchingProperties(underlying_model(), query,
                                                max_count);
  std::erase_if(nodes, GetNodeExcludedFromViewPredicate());
  return nodes;
}

const bookmarks::BookmarkNode*
LegacyBookmarkModelWithSharedUnderlyingModel::GetNodeById(int64_t id) {
  const bookmarks::BookmarkNode* node =
      bookmarks::GetBookmarkNodeByID(underlying_model(), id);
  if (!node) {
    return nullptr;
  }
  if (IsNodeExcludedFromView(node)) {
    return nullptr;
  }
  return node;
}

void LegacyBookmarkModelWithSharedUnderlyingModel::BookmarkModelLoaded(
    bool ids_reassigned) {
  for (bookmarks::BookmarkModelObserver& observer : observers_) {
    observer.BookmarkModelLoaded(ids_reassigned);
  }
}

void LegacyBookmarkModelWithSharedUnderlyingModel::BookmarkModelBeingDeleted() {
  scoped_observation_.Reset();
  for (bookmarks::BookmarkModelObserver& observer : observers_) {
    observer.BookmarkModelBeingDeleted();
  }
}

void LegacyBookmarkModelWithSharedUnderlyingModel::BookmarkNodeMoved(
    const bookmarks::BookmarkNode* old_parent,
    size_t old_index,
    const bookmarks::BookmarkNode* new_parent,
    size_t new_index) {
  const bool is_old_parent_excluded = IsNodeExcludedFromView(old_parent);
  const bool is_new_parent_excluded = IsNodeExcludedFromView(new_parent);

  if (is_old_parent_excluded && is_new_parent_excluded) {
    // Excluded from view before and after, nothing to report.
    return;
  }

  if (is_old_parent_excluded && !is_new_parent_excluded) {
    // Node became visible; report it as creation.
    for (bookmarks::BookmarkModelObserver& observer : observers_) {
      observer.BookmarkNodeAdded(new_parent, new_index,
                                 /*added_by_user=*/false);
    }
    return;
  }

  if (!is_old_parent_excluded && is_new_parent_excluded) {
    // Node no longer visible; report it as removal.
    const bookmarks::BookmarkNode* node =
        new_parent->children()[new_index].get();
    for (bookmarks::BookmarkModelObserver& observer : observers_) {
      observer.OnWillRemoveBookmarks(old_parent, old_index, node);
    }
    for (bookmarks::BookmarkModelObserver& observer : observers_) {
      observer.BookmarkNodeRemoved(old_parent, old_index, node,
                                   /*no_longer_bookmarked=*/{});
    }
    return;
  }

  // Node visible before and after the move; report it as normal move.
  for (bookmarks::BookmarkModelObserver& observer : observers_) {
    observer.BookmarkNodeMoved(old_parent, old_index, new_parent, new_index);
  }
}

void LegacyBookmarkModelWithSharedUnderlyingModel::BookmarkNodeAdded(
    const bookmarks::BookmarkNode* parent,
    size_t index,
    bool added_by_user) {
  if (!parent->is_root() && IsNodeExcludedFromView(parent)) {
    return;
  }
  for (bookmarks::BookmarkModelObserver& observer : observers_) {
    observer.BookmarkNodeAdded(parent, index, added_by_user);
  }
}

void LegacyBookmarkModelWithSharedUnderlyingModel::OnWillRemoveBookmarks(
    const bookmarks::BookmarkNode* parent,
    size_t old_index,
    const bookmarks::BookmarkNode* node) {
  if (!parent->is_root() && IsNodeExcludedFromView(parent)) {
    return;
  }
  for (bookmarks::BookmarkModelObserver& observer : observers_) {
    observer.OnWillRemoveBookmarks(parent, old_index, node);
  }
}

void LegacyBookmarkModelWithSharedUnderlyingModel::BookmarkNodeRemoved(
    const bookmarks::BookmarkNode* parent,
    size_t old_index,
    const bookmarks::BookmarkNode* node,
    const std::set<GURL>& no_longer_bookmarked) {
  if (!parent->is_root() && IsNodeExcludedFromView(parent)) {
    return;
  }
  // It isn't possible to compute `no_longer_bookmarked` so the workaround here
  // is to always pass an empty set, as it isn't actually consumed on ios.
  for (bookmarks::BookmarkModelObserver& observer : observers_) {
    observer.BookmarkNodeRemoved(parent, old_index, node,
                                 /*no_longer_bookmarked=*/{});
  }
}

void LegacyBookmarkModelWithSharedUnderlyingModel::OnWillChangeBookmarkNode(
    const bookmarks::BookmarkNode* node) {
  if (IsNodeExcludedFromView(node)) {
    return;
  }
  for (bookmarks::BookmarkModelObserver& observer : observers_) {
    observer.OnWillChangeBookmarkNode(node);
  }
}

void LegacyBookmarkModelWithSharedUnderlyingModel::BookmarkNodeChanged(
    const bookmarks::BookmarkNode* node) {
  if (IsNodeExcludedFromView(node)) {
    return;
  }
  for (bookmarks::BookmarkModelObserver& observer : observers_) {
    observer.BookmarkNodeChanged(node);
  }
}

void LegacyBookmarkModelWithSharedUnderlyingModel::OnWillChangeBookmarkMetaInfo(
    const bookmarks::BookmarkNode* node) {
  if (IsNodeExcludedFromView(node)) {
    return;
  }
  for (bookmarks::BookmarkModelObserver& observer : observers_) {
    observer.OnWillChangeBookmarkMetaInfo(node);
  }
}

void LegacyBookmarkModelWithSharedUnderlyingModel::BookmarkMetaInfoChanged(
    const bookmarks::BookmarkNode* node) {
  if (IsNodeExcludedFromView(node)) {
    return;
  }
  for (bookmarks::BookmarkModelObserver& observer : observers_) {
    observer.BookmarkMetaInfoChanged(node);
  }
}

void LegacyBookmarkModelWithSharedUnderlyingModel::BookmarkNodeFaviconChanged(
    const bookmarks::BookmarkNode* node) {
  if (IsNodeExcludedFromView(node)) {
    return;
  }
  for (bookmarks::BookmarkModelObserver& observer : observers_) {
    observer.BookmarkNodeFaviconChanged(node);
  }
}

void LegacyBookmarkModelWithSharedUnderlyingModel::OnWillReorderBookmarkNode(
    const bookmarks::BookmarkNode* node) {
  if (IsNodeExcludedFromView(node)) {
    return;
  }
  for (bookmarks::BookmarkModelObserver& observer : observers_) {
    observer.OnWillReorderBookmarkNode(node);
  }
}

void LegacyBookmarkModelWithSharedUnderlyingModel::
    BookmarkNodeChildrenReordered(const bookmarks::BookmarkNode* node) {
  if (IsNodeExcludedFromView(node)) {
    return;
  }
  for (bookmarks::BookmarkModelObserver& observer : observers_) {
    observer.BookmarkNodeChildrenReordered(node);
  }
}

void LegacyBookmarkModelWithSharedUnderlyingModel::
    ExtensiveBookmarkChangesBeginning() {
  for (bookmarks::BookmarkModelObserver& observer : observers_) {
    observer.ExtensiveBookmarkChangesBeginning();
  }
}

void LegacyBookmarkModelWithSharedUnderlyingModel::
    ExtensiveBookmarkChangesEnded() {
  for (bookmarks::BookmarkModelObserver& observer : observers_) {
    observer.ExtensiveBookmarkChangesEnded();
  }
}

void LegacyBookmarkModelWithSharedUnderlyingModel::
    OnWillRemoveAllUserBookmarks() {
  for (bookmarks::BookmarkModelObserver& observer : observers_) {
    observer.OnWillRemoveAllUserBookmarks();
  }
}

void LegacyBookmarkModelWithSharedUnderlyingModel::BookmarkAllUserNodesRemoved(
    const std::set<GURL>& removed_urls) {
  // Computing `removed_urls` isn't possible so this simply uses an empty set,
  // as it isn't consumed anyway on ios/.
  for (bookmarks::BookmarkModelObserver& observer : observers_) {
    observer.BookmarkAllUserNodesRemoved(/*removed_urls=*/{});
  }
}

void LegacyBookmarkModelWithSharedUnderlyingModel::
    GroupedBookmarkChangesBeginning() {
  for (bookmarks::BookmarkModelObserver& observer : observers_) {
    observer.GroupedBookmarkChangesBeginning();
  }
}

void LegacyBookmarkModelWithSharedUnderlyingModel::
    GroupedBookmarkChangesEnded() {
  for (bookmarks::BookmarkModelObserver& observer : observers_) {
    observer.GroupedBookmarkChangesEnded();
  }
}

bool LegacyBookmarkModelWithSharedUnderlyingModel::IsNodePartOfModel(
    const bookmarks::BookmarkNode* node) const {
  return node && !IsNodeExcludedFromView(node);
}

const bookmarks::BookmarkNode* LegacyBookmarkModelWithSharedUnderlyingModel::
    MoveToOtherModelPossiblyWithNewNodeIdsAndUuids(
        const bookmarks::BookmarkNode* node,
        LegacyBookmarkModel* dest_model,
        const bookmarks::BookmarkNode* dest_parent) {
  underlying_model()->Move(node, dest_parent, dest_parent->children().size());
  return node;
}

base::WeakPtr<LegacyBookmarkModel>
LegacyBookmarkModelWithSharedUnderlyingModel::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

LegacyBookmarkModelWithSharedUnderlyingModel::NodeExcludedFromViewPredicate
LegacyBookmarkModelWithSharedUnderlyingModel::GetNodeExcludedFromViewPredicate()
    const {
  return NodeExcludedFromViewPredicate(bookmark_bar_node(), other_node(),
                                       mobile_node(), managed_node());
}

bool LegacyBookmarkModelWithSharedUnderlyingModel::IsNodeExcludedFromView(
    const bookmarks::BookmarkNode* node) const {
  return GetNodeExcludedFromViewPredicate()(node);
}

const bookmarks::BookmarkNode*
LegacyBookmarkModelWithSharedUnderlyingModel::managed_node() const {
  return managed_bookmark_service_ ? managed_bookmark_service_->managed_node()
                                   : nullptr;
}
