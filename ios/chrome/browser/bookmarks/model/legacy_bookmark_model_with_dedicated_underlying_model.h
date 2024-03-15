// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BOOKMARKS_MODEL_LEGACY_BOOKMARK_MODEL_WITH_DEDICATED_UNDERLYING_MODEL_H_
#define IOS_CHROME_BROWSER_BOOKMARKS_MODEL_LEGACY_BOOKMARK_MODEL_WITH_DEDICATED_UNDERLYING_MODEL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/bookmarks/common/bookmark_metrics.h"
#include "ios/chrome/browser/bookmarks/model/legacy_bookmark_model.h"

class GURL;

namespace base {
class Uuid;
}  // namespace base

namespace bookmarks {
class BookmarkModel;
class BookmarkModelObserver;
class BookmarkNode;
class ManagedBookmarkService;
struct QueryFields;
}  // namespace bookmarks

// Implementation of LegacyBookmarkModel that owns a dedicated BookmarkModel
// that isn't shared with other instances. This instance doesn't make use of
// the BookmarkModel APIs for account bookmarks, and instead always exposes the
// local-or-syncable nodes (even if `this` is used for account bookmarks).
class LegacyBookmarkModelWithDedicatedUnderlyingModel
    : public LegacyBookmarkModel {
 public:
  LegacyBookmarkModelWithDedicatedUnderlyingModel(
      std::unique_ptr<bookmarks::BookmarkModel> underlying_model,
      bookmarks::ManagedBookmarkService* managed_bookmark_service);
  ~LegacyBookmarkModelWithDedicatedUnderlyingModel() override;

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

 protected:
  const bookmarks::BookmarkModel* underlying_model() const override;
  bookmarks::BookmarkModel* underlying_model() override;

 private:
  const std::unique_ptr<bookmarks::BookmarkModel> underlying_model_;
  const raw_ptr<bookmarks::ManagedBookmarkService> managed_bookmark_service_;
  base::WeakPtrFactory<LegacyBookmarkModelWithDedicatedUnderlyingModel>
      weak_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_MODEL_LEGACY_BOOKMARK_MODEL_WITH_DEDICATED_UNDERLYING_MODEL_H_
