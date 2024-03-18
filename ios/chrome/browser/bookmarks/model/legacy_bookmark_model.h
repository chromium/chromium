// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BOOKMARKS_MODEL_LEGACY_BOOKMARK_MODEL_H_
#define IOS_CHROME_BROWSER_BOOKMARKS_MODEL_LEGACY_BOOKMARK_MODEL_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/bookmarks/common/bookmark_metrics.h"
#include "components/keyed_service/core/keyed_service.h"

class GURL;

namespace base {
class Uuid;
}  // namespace base

namespace bookmarks {
class BookmarkModel;
class BookmarkModelObserver;
class BookmarkNode;
struct QueryFields;
}  // namespace bookmarks

namespace ios {
class AccountBookmarkModelFactory;
class LocalOrSyncableBookmarkModelFactory;
}  // namespace ios

// iOS-specific interface that mimcs bookmarks::BookmarkModel that allows
// a gradual migration of code under ios/ to use one BookmarkModel instance
// instead of two, by allowing subclasses to expose the legacy behavior (two
// instances) on top of one shared underlying BookmarkModel instance.
class LegacyBookmarkModel : public KeyedService {
 public:
  LegacyBookmarkModel();
  LegacyBookmarkModel(const LegacyBookmarkModel&) = delete;
  ~LegacyBookmarkModel() override;

  LegacyBookmarkModel& operator=(const LegacyBookmarkModel&) = delete;

  // KeyedService overrides.
  void Shutdown() override;

  // Returns the root node. The 'bookmark bar' node and 'other' node are
  // children of the root node.
  // WARNING: avoid exercising this API, in particular if the caller may use
  // the node to iterate children. This is because the behavior of this function
  // changes based on whether or not feature
  // `syncer::kEnableBookmarkFoldersForAccountStorage` is enabled.
  const bookmarks::BookmarkNode* subtle_root_node_with_unspecified_children()
      const;

  // All public functions below are identical to the functions with the same
  // name in bookmarks::BookmarkModel API or, in some cases, related utility
  // libraries.
  bool loaded() const;
  void Remove(const bookmarks::BookmarkNode* node,
              bookmarks::metrics::BookmarkEditSource source);
  void Move(const bookmarks::BookmarkNode* node,
            const bookmarks::BookmarkNode* new_parent,
            size_t index);
  void Copy(const bookmarks::BookmarkNode* node,
            const bookmarks::BookmarkNode* new_parent,
            size_t index);
  void SetTitle(const bookmarks::BookmarkNode* node,
                const std::u16string& title,
                bookmarks::metrics::BookmarkEditSource source);
  void SetURL(const bookmarks::BookmarkNode* node,
              const GURL& url,
              bookmarks::metrics::BookmarkEditSource source);
  void SetDateAdded(const bookmarks::BookmarkNode* node, base::Time date_added);
  bool HasNoUserCreatedBookmarksOrFolders() const;
  const bookmarks::BookmarkNode* AddFolder(
      const bookmarks::BookmarkNode* parent,
      size_t index,
      const std::u16string& title);
  const bookmarks::BookmarkNode* AddNewURL(
      const bookmarks::BookmarkNode* parent,
      size_t index,
      const std::u16string& title,
      const GURL& url);
  const bookmarks::BookmarkNode* AddURL(const bookmarks::BookmarkNode* parent,
                                        size_t index,
                                        const std::u16string& title,
                                        const GURL& url);
  void RemoveMany(const std::set<const bookmarks::BookmarkNode*>& nodes,
                  bookmarks::metrics::BookmarkEditSource source);
  void CommitPendingWriteForTest();

  // LegacyBookmarkModel has three top-level permanent nodes (as opposed to
  // bookmarks::BookmarkModel that can have up to six).
  virtual const bookmarks::BookmarkNode* bookmark_bar_node() const = 0;
  virtual const bookmarks::BookmarkNode* other_node() const = 0;
  virtual const bookmarks::BookmarkNode* mobile_node() const = 0;
  virtual const bookmarks::BookmarkNode* managed_node() const = 0;

  virtual bool IsBookmarked(const GURL& url) const = 0;
  virtual bool is_permanent_node(const bookmarks::BookmarkNode* node) const = 0;
  virtual void AddObserver(bookmarks::BookmarkModelObserver* observer) = 0;
  virtual void RemoveObserver(bookmarks::BookmarkModelObserver* observer) = 0;
  [[nodiscard]] virtual std::vector<
      raw_ptr<const bookmarks::BookmarkNode, VectorExperimental>>
  GetNodesByURL(const GURL& url) const = 0;
  virtual const bookmarks::BookmarkNode* GetNodeByUuid(
      const base::Uuid& uuid) const = 0;
  virtual const bookmarks::BookmarkNode* GetMostRecentlyAddedUserNodeForURL(
      const GURL& url) const = 0;
  virtual bool HasBookmarks() const = 0;

  // Functions that aren't present in BookmarkModel but in utility libraries
  // that require a subclass-specific implementation.
  virtual std::vector<const bookmarks::BookmarkNode*>
  GetBookmarksMatchingProperties(const bookmarks::QueryFields& query,
                                 size_t max_count) = 0;
  virtual const bookmarks::BookmarkNode* GetNodeById(int64_t id) = 0;
  // Returns whether `node` is part of, or relevant, in the scope of `this`.
  virtual bool IsNodePartOfModel(const bookmarks::BookmarkNode* node) const = 0;
  virtual const bookmarks::BookmarkNode*
  MoveToOtherModelPossiblyWithNewNodeIdsAndUuids(
      const bookmarks::BookmarkNode* node,
      LegacyBookmarkModel* dest_model,
      const bookmarks::BookmarkNode* dest_parent) = 0;

  virtual base::WeakPtr<LegacyBookmarkModel> AsWeakPtr() = 0;

 protected:
  // Allow factories to access the underlying model.
  friend class ios::AccountBookmarkModelFactory;
  friend class ios::LocalOrSyncableBookmarkModelFactory;

  virtual const bookmarks::BookmarkModel* underlying_model() const = 0;
  virtual bookmarks::BookmarkModel* underlying_model() = 0;
};

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_MODEL_LEGACY_BOOKMARK_MODEL_H_
