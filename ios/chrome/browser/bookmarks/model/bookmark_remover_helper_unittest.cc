// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/bookmarks/model/bookmark_remover_helper.h"

#include "base/test/test_future.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "ios/chrome/browser/bookmarks/model/bookmark_ios_unit_test_support.h"
#include "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"

class BookmarkRemoverHelperUnitTest : public BookmarkIOSUnitTestSupport {
 public:
  BookmarkRemoverHelperUnitTest()
      : BookmarkIOSUnitTestSupport(/*wait_for_initialization=*/false) {}
  ~BookmarkRemoverHelperUnitTest() override = default;
};

TEST_F(BookmarkRemoverHelperUnitTest,
       TestRemoveAllUserBookmarksIOSBeforeIntialization) {
  base::test::TestFuture<bool> test_future;
  BookmarkRemoverHelper helper(chrome_browser_state_.get());
  helper.RemoveAllUserBookmarksIOS(test_future.GetCallback());
  ASSERT_FALSE(test_future.IsReady());
  bookmarks::test::WaitForBookmarkModelToLoad(
      local_or_syncable_bookmark_model_);
  bookmarks::test::WaitForBookmarkModelToLoad(account_bookmark_model_);
  ASSERT_TRUE(test_future.Get());
}

// TODO(crbug.com/1480008): Add more tests for BookmarkRemoverHelper.
