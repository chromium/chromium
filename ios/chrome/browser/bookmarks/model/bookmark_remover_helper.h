// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BOOKMARKS_MODEL_BOOKMARK_REMOVER_HELPER_H_
#define IOS_CHROME_BROWSER_BOOKMARKS_MODEL_BOOKMARK_REMOVER_HELPER_H_

#import "base/functional/callback.h"
#import "base/location.h"
#import "base/memory/raw_ptr.h"
#import "base/scoped_observation.h"
#import "base/sequence_checker.h"
#import "components/bookmarks/browser/base_bookmark_model_observer.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

namespace bookmarks {
class BookmarkModel;
}  // namespace bookmarks

// Helper class to asynchronously remove bookmarks.
class BookmarkRemoverHelper : public bookmarks::BaseBookmarkModelObserver {
 public:
  using Callback = base::OnceCallback<void(bool)>;

  explicit BookmarkRemoverHelper(ProfileIOS* profile);

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

  const raw_ptr<ProfileIOS> profile_;
  const raw_ptr<bookmarks::BookmarkModel> model_;

  base::Location location_;
  Callback completion_;
  base::ScopedObservation<bookmarks::BookmarkModel,
                          bookmarks::BookmarkModelObserver>
      bookmark_model_observation_{this};

  SEQUENCE_CHECKER(sequence_checker_);
};

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_MODEL_BOOKMARK_REMOVER_HELPER_H_
