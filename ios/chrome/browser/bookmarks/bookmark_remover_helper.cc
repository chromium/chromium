// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/bookmarks/bookmark_remover_helper.h"

#include "base/bind.h"
#include "base/check_op.h"
#include "base/notreached.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#include "ios/chrome/browser/bookmarks/bookmarks_utils.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"

BookmarkRemoverHelper::BookmarkRemoverHelper(ChromeBrowserState* browser_state)
    : browser_state_(browser_state) {
  DCHECK(browser_state_);
}

BookmarkRemoverHelper::~BookmarkRemoverHelper() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!browser_state_);
}

void BookmarkRemoverHelper::BookmarkModelChanged() {
  NOTREACHED();
}

void BookmarkRemoverHelper::BookmarkModelLoaded(
    bookmarks::BookmarkModel* bookmark_model,
    bool) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bookmark_model->RemoveObserver(this);
  BookmarksRemoved(::RemoveAllUserBookmarksIOS(browser_state_));
}

void BookmarkRemoverHelper::BookmarkModelBeingDeleted(
    bookmarks::BookmarkModel* bookmark_model) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bookmark_model->RemoveObserver(this);
  BookmarksRemoved(false);
}

void BookmarkRemoverHelper::RemoveAllUserBookmarksIOS(Callback completion) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  completion_ = std::move(completion);

  bookmarks::BookmarkModel* bookmark_model =
      ios::BookmarkModelFactory::GetForBrowserState(browser_state_);

  if (!bookmark_model) {
    BookmarksRemoved(false);
    return;
  }

  if (bookmark_model->loaded()) {
    BookmarksRemoved(::RemoveAllUserBookmarksIOS(browser_state_));
  } else {
    // Wait for the BookmarkModel to finish loading before deleting entries.
    bookmark_model->AddObserver(this);
  }
}

void BookmarkRemoverHelper::BookmarksRemoved(bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  browser_state_ = nullptr;
  if (completion_) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(completion_), success));
  }
}
