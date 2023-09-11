// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/bookmarks/model/bookmark_remover_helper.h"

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "ios/chrome/browser/bookmarks/model/account_bookmark_model_factory.h"
#include "ios/chrome/browser/bookmarks/model/bookmarks_utils.h"
#include "ios/chrome/browser/bookmarks/model/local_or_syncable_bookmark_model_factory.h"
#include "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"

BookmarkRemoverHelper::BookmarkRemoverHelper(ChromeBrowserState* browser_state)
    : browser_state_(browser_state) {
  DCHECK(browser_state_);
}

BookmarkRemoverHelper::~BookmarkRemoverHelper() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!browser_state_);
}

void BookmarkRemoverHelper::BookmarkModelChanged() {
  // Nothing to do here, we only care about all bookmark models being loaded.
}

void BookmarkRemoverHelper::BookmarkModelLoaded(
    bookmarks::BookmarkModel* bookmark_model,
    bool) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!AreAllAvailableBookmarkModelsLoaded(browser_state_)) {
    // Some models are still loading, need to wait more.
    return;
  }

  bookmark_model_observations_.RemoveAllObservations();
  BookmarksRemoved(::RemoveAllUserBookmarksIOS(browser_state_));
}

void BookmarkRemoverHelper::BookmarkModelBeingDeleted(
    bookmarks::BookmarkModel* bookmark_model) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bookmark_model_observations_.RemoveAllObservations();
  BookmarksRemoved(false);
}

void BookmarkRemoverHelper::RemoveAllUserBookmarksIOS(Callback completion) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  completion_ = std::move(completion);

  if (AreAllAvailableBookmarkModelsLoaded(browser_state_)) {
    BookmarksRemoved(::RemoveAllUserBookmarksIOS(browser_state_));
    return;
  }

  // Wait for BookmarkModels to finish loading before deleting entries.
  bookmarks::BookmarkModel* local_or_syncable_bookmark_model =
      ios::LocalOrSyncableBookmarkModelFactory::GetForBrowserState(
          browser_state_);
  bookmark_model_observations_.AddObservation(local_or_syncable_bookmark_model);

  bookmarks::BookmarkModel* account_bookmark_model =
      ios::AccountBookmarkModelFactory::GetForBrowserState(browser_state_);
  if (account_bookmark_model) {
    bookmark_model_observations_.AddObservation(account_bookmark_model);
  }
}

void BookmarkRemoverHelper::BookmarksRemoved(bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  browser_state_ = nullptr;
  if (completion_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(completion_), success));
  }
}
