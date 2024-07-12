// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/bookmarks/model/bookmark_remover_helper.h"

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "ios/chrome/browser/bookmarks/model/bookmark_model_factory.h"
#include "ios/chrome/browser/bookmarks/model/bookmarks_utils.h"
#include "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"

BookmarkRemoverHelper::BookmarkRemoverHelper(ChromeBrowserState* browser_state)
    : browser_state_(browser_state),
      model_(ios::BookmarkModelFactory::GetForBrowserState(browser_state)) {
  CHECK(browser_state_);
  CHECK(model_);
}

BookmarkRemoverHelper::~BookmarkRemoverHelper() = default;

void BookmarkRemoverHelper::BookmarkModelChanged() {
  // Nothing to do here, we only care about the bookmark model being loaded.
}

void BookmarkRemoverHelper::BookmarkModelLoaded(bool ids_reassigned) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bookmark_model_observation_.Reset();
  RemoveAllUserBookmarksFromLoadedModel();
}

void BookmarkRemoverHelper::BookmarkModelBeingDeleted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bookmark_model_observation_.Reset();
  TriggerCompletion(/*success=*/false);
}

void BookmarkRemoverHelper::RemoveAllUserBookmarksIOS(
    const base::Location& location,
    Callback completion) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  location_ = std::move(location);
  completion_ = std::move(completion);

  if (model_->loaded()) {
    RemoveAllUserBookmarksFromLoadedModel();
    return;
  }

  // Wait for BookmarkModel to finish loading before deleting entries.
  bookmark_model_observation_.Observe(model_);
}

void BookmarkRemoverHelper::RemoveAllUserBookmarksFromLoadedModel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(model_->loaded());

  model_->RemoveAllUserBookmarks(location_);
  ResetLastUsedBookmarkFolder(browser_state_->GetPrefs());
  TriggerCompletion(/*success=*/true);
}

void BookmarkRemoverHelper::TriggerCompletion(bool success) {
  if (completion_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(completion_), success));
  }
}
