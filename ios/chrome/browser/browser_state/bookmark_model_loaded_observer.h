// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSER_STATE_BOOKMARK_MODEL_LOADED_OBSERVER_H_
#define IOS_CHROME_BROWSER_BROWSER_STATE_BOOKMARK_MODEL_LOADED_OBSERVER_H_

#include "components/bookmarks/browser/base_bookmark_model_observer.h"

class ChromeBrowserState;

class BookmarkModelLoadedObserver
    : public bookmarks::BaseBookmarkModelObserver {
 public:
  explicit BookmarkModelLoadedObserver(ChromeBrowserState* browser_state);

  BookmarkModelLoadedObserver(const BookmarkModelLoadedObserver&) = delete;
  BookmarkModelLoadedObserver& operator=(const BookmarkModelLoadedObserver&) =
      delete;

 private:
  void BookmarkModelChanged() override;
  void BookmarkModelLoaded(bookmarks::BookmarkModel* model,
                           bool ids_reassigned) override;
  void BookmarkModelBeingDeleted(bookmarks::BookmarkModel* model) override;

  ChromeBrowserState* browser_state_;
};

#endif  // IOS_CHROME_BROWSER_BROWSER_STATE_BOOKMARK_MODEL_LOADED_OBSERVER_H_
