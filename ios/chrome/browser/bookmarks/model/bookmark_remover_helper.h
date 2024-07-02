// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BOOKMARKS_MODEL_BOOKMARK_REMOVER_HELPER_H_
#define IOS_CHROME_BROWSER_BOOKMARKS_MODEL_BOOKMARK_REMOVER_HELPER_H_

#include "base/functional/callback.h"
#include "base/location.h"
#import "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"

class ChromeBrowserState;

namespace bookmarks {
class BookmarkModel;
}  // namespace bookmarks

// Helper class to asynchronously remove bookmarks.
class BookmarkRemoverHelper : public bookmarks::BaseBookmarkModelObserver {
 public:
  using Callback = base::OnceCallback<void(bool)>;

  explicit BookmarkRemoverHelper(ChromeBrowserState* browser_state);

  BookmarkRemoverHelper(const BookmarkRemoverHelper&) = delete;
  BookmarkRemoverHelper& operator=(const BookmarkRemoverHelper&) = delete;

  ~BookmarkRemoverHelper() override;

  // Removes all bookmarks and asynchronously invoke `completion` with
  // boolean indicating success or failure.
  void RemoveAllUserBookmarksIOS(const base::Location& location,
                                 Callback completion);

  // BaseBookmarkModelObserver implementation.
  void BookmarkModelChanged() override;

  // BookmarkModelObserver implementation.
  void BookmarkModelLoaded(bool ids_reassigned) override;
  void BookmarkModelBeingDeleted() override;

 private:
  void RemoveAllUserBookmarksFromLoadedModel();
  void TriggerCompletion(bool success);

  const raw_ptr<ChromeBrowserState> browser_state_;
  const raw_ptr<bookmarks::BookmarkModel> model_;

  base::Location location_;
  Callback completion_;
  base::ScopedObservation<bookmarks::BookmarkModel,
                          bookmarks::BookmarkModelObserver>
      bookmark_model_observation_{this};

  SEQUENCE_CHECKER(sequence_checker_);
};

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_MODEL_BOOKMARK_REMOVER_HELPER_H_
