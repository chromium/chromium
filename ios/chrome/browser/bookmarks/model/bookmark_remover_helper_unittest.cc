// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/bookmarks/model/bookmark_remover_helper.h"

#include "base/test/test_future.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "ios/chrome/browser/bookmarks/model/bookmark_ios_unit_test_support.h"
#include "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"

namespace {

class BookmarkRemoverHelperUnitTest : public BookmarkIOSUnitTestSupport {
 public:
  BookmarkRemoverHelperUnitTest()
      : BookmarkIOSUnitTestSupport(/*wait_for_initialization=*/false) {}
  ~BookmarkRemoverHelperUnitTest() override = default;
};

TEST_F(BookmarkRemoverHelperUnitTest,
       TestRemoveAllUserBookmarksIOSBeforeIntialization) {
  base::test::TestFuture<bool> test_future;
  BookmarkRemoverHelper helper(profile_.get());
  ASSERT_FALSE(bookmark_model_->loaded());
  helper.RemoveAllUserBookmarksIOS(FROM_HERE, test_future.GetCallback());

  bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model_);
  EXPECT_TRUE(test_future.Get());
}

TEST_F(BookmarkRemoverHelperUnitTest,
       TestRemoveAllUserBookmarksIOSAfterIntialization) {
  bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model_);

  base::test::TestFuture<bool> test_future;
  BookmarkRemoverHelper helper(profile_.get());
  ASSERT_TRUE(bookmark_model_->loaded());
  helper.RemoveAllUserBookmarksIOS(FROM_HERE, test_future.GetCallback());
  EXPECT_TRUE(test_future.Get());
}

TEST_F(BookmarkRemoverHelperUnitTest,
       TestDeleteModelWhileOngoingRemoveAllUserBookmarksIOS) {
  base::test::TestFuture<bool> test_future;
  BookmarkRemoverHelper helper(profile_.get());
  ASSERT_FALSE(bookmark_model_->loaded());
  helper.RemoveAllUserBookmarksIOS(FROM_HERE, test_future.GetCallback());

  // Mimic BookmarkModel being destructed.
  helper.BookmarkModelBeingDeleted();

  EXPECT_FALSE(test_future.Get());
}

}  // namespace
