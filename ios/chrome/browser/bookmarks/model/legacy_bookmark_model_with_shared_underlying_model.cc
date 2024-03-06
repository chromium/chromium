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
  // TODO(crbug.com/326185948): Implement observers properly as the approach
  // below leaks undesired state changes.
  underlying_model()->AddObserver(observer);
}

void LegacyBookmarkModelWithSharedUnderlyingModel::RemoveObserver(
    bookmarks::BookmarkModelObserver* observer) {
  // TODO(crbug.com/326185948): Implement observers properly.
  underlying_model()->RemoveObserver(observer);
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

void LegacyBookmarkModelWithSharedUnderlyingModel::
    GetBookmarksMatchingProperties(
        const bookmarks::QueryFields& query,
        size_t max_count,
        std::vector<const bookmarks::BookmarkNode*>* nodes) {
  bookmarks::GetBookmarksMatchingProperties(underlying_model(), query,
                                            max_count, nodes);
  std::erase_if(*nodes, GetNodeExcludedFromViewPredicate());
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
