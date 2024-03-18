// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BOOKMARKS_MODEL_LEGACY_BOOKMARK_MODEL_WITH_SHARED_UNDERLYING_MODEL_H_
#define IOS_CHROME_BROWSER_BOOKMARKS_MODEL_LEGACY_BOOKMARK_MODEL_WITH_SHARED_UNDERLYING_MODEL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_model_observer.h"
#include "components/bookmarks/common/bookmark_metrics.h"
#include "ios/chrome/browser/bookmarks/model/legacy_bookmark_model.h"

class GURL;

namespace base {
class Uuid;
}  // namespace base

namespace bookmarks {
class BookmarkNode;
class ManagedBookmarkService;
struct QueryFields;
}  // namespace bookmarks

// Implementation of LegacyBookmarkModel that uses a BookmarkModel instance that
// may be shared with other LegacyBookmarkModel instances. It exposes a subset
// of the bookmark tree, depending on the arguments in the constructor.
class LegacyBookmarkModelWithSharedUnderlyingModel
    : public LegacyBookmarkModel,
      public bookmarks::BookmarkModelObserver {
 public:
  // `node_type_for_uuid_lookup` not only influences UUID lookups, but also
  // generally determines which subset of the BookmarkModel is exposed in the
  // view represented by `this`.
  LegacyBookmarkModelWithSharedUnderlyingModel(
      bookmarks::BookmarkModel* underlying_model,
      bookmarks::BookmarkModel::NodeTypeForUuidLookup node_type_for_uuid_lookup,
      bookmarks::ManagedBookmarkService* managed_bookmark_service);
  ~LegacyBookmarkModelWithSharedUnderlyingModel() override;

  // LegacyBookmarkModel overrides.
  const bookmarks::BookmarkNode* bookmark_bar_node() const override;
  const bookmarks::BookmarkNode* other_node() const override;
  const bookmarks::BookmarkNode* mobile_node() const override;
  const bookmarks::BookmarkNode* managed_node() const override;
  bool IsBookmarked(const GURL& url) const override;
  bool is_permanent_node(const bookmarks::BookmarkNode* node) const override;
  void AddObserver(bookmarks::BookmarkModelObserver* observer) override;
  void RemoveObserver(bookmarks::BookmarkModelObserver* observer) override;
  [[nodiscard]] std::vector<
      raw_ptr<const bookmarks::BookmarkNode, VectorExperimental>>
  GetNodesByURL(const GURL& url) const override;
  const bookmarks::BookmarkNode* GetNodeByUuid(
      const base::Uuid& uuid) const override;
  const bookmarks::BookmarkNode* GetMostRecentlyAddedUserNodeForURL(
      const GURL& url) const override;
  bool HasBookmarks() const override;
  std::vector<const bookmarks::BookmarkNode*> GetBookmarksMatchingProperties(
      const bookmarks::QueryFields& query,
      size_t max_count) override;
  const bookmarks::BookmarkNode* GetNodeById(int64_t id) override;
  bool IsNodePartOfModel(const bookmarks::BookmarkNode* node) const override;
  const bookmarks::BookmarkNode* MoveToOtherModelPossiblyWithNewNodeIdsAndUuids(
      const bookmarks::BookmarkNode* node,
      LegacyBookmarkModel* dest_model,
      const bookmarks::BookmarkNode* dest_parent) override;
  base::WeakPtr<LegacyBookmarkModel> AsWeakPtr() override;

  // BookmarkModelObserver overrides.
  void BookmarkModelLoaded(bool ids_reassigned) override;
  void BookmarkModelBeingDeleted() override;
  void BookmarkNodeMoved(const bookmarks::BookmarkNode* old_parent,
                         size_t old_index,
                         const bookmarks::BookmarkNode* new_parent,
                         size_t new_index) override;
  void BookmarkNodeAdded(const bookmarks::BookmarkNode* parent,
                         size_t index,
                         bool added_by_user) override;
  void OnWillRemoveBookmarks(const bookmarks::BookmarkNode* parent,
                             size_t old_index,
                             const bookmarks::BookmarkNode* node) override;
  void BookmarkNodeRemoved(const bookmarks::BookmarkNode* parent,
                           size_t old_index,
                           const bookmarks::BookmarkNode* node,
                           const std::set<GURL>& no_longer_bookmarked) override;
  void OnWillChangeBookmarkNode(const bookmarks::BookmarkNode* node) override;
  void BookmarkNodeChanged(const bookmarks::BookmarkNode* node) override;
  void OnWillChangeBookmarkMetaInfo(
      const bookmarks::BookmarkNode* node) override;
  void BookmarkMetaInfoChanged(const bookmarks::BookmarkNode* node) override;
  void BookmarkNodeFaviconChanged(const bookmarks::BookmarkNode* node) override;
  void OnWillReorderBookmarkNode(const bookmarks::BookmarkNode* node) override;
  void BookmarkNodeChildrenReordered(
      const bookmarks::BookmarkNode* node) override;
  void ExtensiveBookmarkChangesBeginning() override;
  void ExtensiveBookmarkChangesEnded() override;
  void OnWillRemoveAllUserBookmarks() override;
  void BookmarkAllUserNodesRemoved(const std::set<GURL>& removed_urls) override;
  void GroupedBookmarkChangesBeginning() override;
  void GroupedBookmarkChangesEnded() override;

 protected:
  const bookmarks::BookmarkModel* underlying_model() const override;
  bookmarks::BookmarkModel* underlying_model() override;

 private:
  // Class representing a predicate exposed via operator() that is useful to
  // filter out bookmark nodes that should not be exposed in `this`.
  class NodeExcludedFromViewPredicate {
   public:
    NodeExcludedFromViewPredicate(
        const bookmarks::BookmarkNode* bookmark_bar_node,
        const bookmarks::BookmarkNode* other_node,
        const bookmarks::BookmarkNode* mobile_node,
        const bookmarks::BookmarkNode* managed_node);
    ~NodeExcludedFromViewPredicate();

    bool operator()(const bookmarks::BookmarkNode* node);

   private:
    const raw_ptr<const bookmarks::BookmarkNode> bookmark_bar_node_;
    const raw_ptr<const bookmarks::BookmarkNode> other_node_;
    const raw_ptr<const bookmarks::BookmarkNode> mobile_node_;
    const raw_ptr<const bookmarks::BookmarkNode> managed_node_;
  };

  // Predicate that determines whether a specific node is relevant or visible
  // in the context of this view. For example, if `this` is exposing account
  // bookmarks, then this predicate will exclude local-or-syncable nodes,
  // including permanent folders themselves. It always returns false for the
  // root node.
  NodeExcludedFromViewPredicate GetNodeExcludedFromViewPredicate() const;
  bool IsNodeExcludedFromView(const bookmarks::BookmarkNode* node) const;

  const raw_ptr<bookmarks::BookmarkModel> underlying_model_;
  const bookmarks::BookmarkModel::NodeTypeForUuidLookup
      node_type_for_uuid_lookup_;
  const raw_ptr<bookmarks::ManagedBookmarkService> managed_bookmark_service_;

  base::ObserverList<BookmarkModelObserver, true> observers_;
  base::ScopedObservation<bookmarks::BookmarkModel,
                          bookmarks::BookmarkModelObserver>
      scoped_observation_{this};
  base::WeakPtrFactory<LegacyBookmarkModelWithSharedUnderlyingModel>
      weak_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_MODEL_LEGACY_BOOKMARK_MODEL_WITH_SHARED_UNDERLYING_MODEL_H_
