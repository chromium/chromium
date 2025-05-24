// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_METRICS_MODEL_BOOKMARK_MODEL_METRICS_SERVICE_H_
#define IOS_CHROME_BROWSER_METRICS_MODEL_BOOKMARK_MODEL_METRICS_SERVICE_H_

#import "base/memory/raw_ptr.h"
#import "base/scoped_observation.h"
#import "components/bookmarks/browser/bookmark_model_observer.h"
#import "components/keyed_service/core/keyed_service.h"

namespace bookmarks {
class BookmarkModel;
}
class ProfileIOS;

// Bookmark model service, used to monitor metrics related to the bookmark
// model.
class BookmarkModelMetricsService : public KeyedService,
                                    public bookmarks::BookmarkModelObserver {
 public:
  // Constructor. The `bookmark_model` must be non-nil.
  explicit BookmarkModelMetricsService(bookmarks::BookmarkModel* bookmark_model,
                                       ProfileIOS* profile);
  ~BookmarkModelMetricsService() override;

  // Disallow copy/assign.
  BookmarkModelMetricsService(const BookmarkModelMetricsService&) = delete;
  BookmarkModelMetricsService& operator=(const BookmarkModelMetricsService&) =
      delete;

  // BookmarkModelObserver:
  void BookmarkModelLoaded(bool ids_reassigned) override;
  void BookmarkModelBeingDeleted() override;
  void BookmarkNodeMoved(const bookmarks::BookmarkNode* old_parent,
                         size_t old_index,
                         const bookmarks::BookmarkNode* new_parent,
                         size_t new_index) override;
  void BookmarkNodeAdded(const bookmarks::BookmarkNode* parent,
                         size_t index,
                         bool added_by_user) override;
  void BookmarkNodeRemoved(const bookmarks::BookmarkNode* parent,
                           size_t old_index,
                           const bookmarks::BookmarkNode* node,
                           const std::set<GURL>& removed_urls,
                           const base::Location& location) override;
  void ExtensiveBookmarkChangesBeginning() override;
  void ExtensiveBookmarkChangesEnded() override;
  void BookmarkAllUserNodesRemoved(const std::set<GURL>& removed_urls,
                                   const base::Location& location) override;
  void BookmarkNodeChanged(const bookmarks::BookmarkNode* node) override;
  void BookmarkNodeFaviconChanged(const bookmarks::BookmarkNode* node) override;
  void BookmarkNodeChildrenReordered(
      const bookmarks::BookmarkNode* node) override;

 private:
  // Updates the crash key related to the number of bookmark nodes.
  void UpdateBookmarkNodesCrashKey();

  // Store whether extensive bookmark operations are currently happening.
  bool in_extensive_changes_ = false;

  const raw_ptr<bookmarks::BookmarkModel> bookmark_model_;
  raw_ptr<ProfileIOS> profile_;
  base::ScopedObservation<bookmarks::BookmarkModel,
                          bookmarks::BookmarkModelObserver>
      bookmark_model_observer_{this};
};

#endif  // IOS_CHROME_BROWSER_METRICS_MODEL_BOOKMARK_MODEL_METRICS_SERVICE_H_
