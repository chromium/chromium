// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/bookmarks/model/legacy_bookmark_model_test_helpers.h"

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "ios/chrome/browser/bookmarks/model/legacy_bookmark_model.h"

namespace {

// BookmarkLoadObserver is used when blocking until the LegacyBookmarkModel
// finishes loading. As soon as the BookmarkModel finishes loading the message
// loop is quit.
class BookmarkLoadObserver : public bookmarks::BaseBookmarkModelObserver {
 public:
  explicit BookmarkLoadObserver(base::OnceClosure quit_task)
      : quit_task_(std::move(quit_task)) {}

  BookmarkLoadObserver(const BookmarkLoadObserver&) = delete;
  BookmarkLoadObserver& operator=(const BookmarkLoadObserver&) = delete;

  ~BookmarkLoadObserver() override = default;

 private:
  // BaseBookmarkModelObserver:
  void BookmarkModelChanged() override {}
  void BookmarkModelLoaded(bool ids_reassigned) override {
    std::move(quit_task_).Run();
  }

  base::OnceClosure quit_task_;
};

}  // namespace

void WaitForLegacyBookmarkModelToLoad(LegacyBookmarkModel* model) {
  if (model->loaded()) {
    return;
  }
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);

  BookmarkLoadObserver observer(run_loop.QuitClosure());
  model->AddObserver(&observer);
  run_loop.Run();
  model->RemoveObserver(&observer);
  DCHECK(model->loaded());
}
