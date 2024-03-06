// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/bookmarks/model/bookmark_model_factory.h"

#include "base/test/scoped_feature_list.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/core_bookmark_model.h"
#include "components/bookmarks/browser/url_and_title.h"
#include "components/sync/base/features.h"
#include "ios/chrome/browser/bookmarks/model/account_bookmark_model_factory.h"
#include "ios/chrome/browser/bookmarks/model/bookmark_ios_unit_test_support.h"
#include "ios/chrome/browser/bookmarks/model/bookmark_model_factory.h"
#include "ios/chrome/browser/bookmarks/model/legacy_bookmark_model.h"
#include "ios/chrome/browser/bookmarks/model/local_or_syncable_bookmark_model_factory.h"
#include "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ios {

namespace {

using testing::UnorderedElementsAre;

MATCHER_P(HasUrl, expected_url, "") {
  return arg.url == expected_url;
}

}  // namespace

class BookmarkModelFactoryTest : public BookmarkIOSUnitTestSupport,
                                 public ::testing::WithParamInterface<bool> {
 public:
  BookmarkModelFactoryTest() {
    if (GetEnableBookmarkFoldersForAccountStorageTestParam()) {
      scoped_feature_list_.InitAndEnableFeature(
          syncer::kEnableBookmarkFoldersForAccountStorage);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          syncer::kEnableBookmarkFoldersForAccountStorage);
    }
  }

  bool GetEnableBookmarkFoldersForAccountStorageTestParam() const {
    return GetParam();
  }

  void SetUp() override {
    BookmarkIOSUnitTestSupport::SetUp();

    if (GetEnableBookmarkFoldersForAccountStorageTestParam()) {
      // Ensure account permanent folders exist so tests are free to create
      // bookmarks underneath.
      BookmarkModelFactory::GetModelForBrowserStateIfUnificationEnabledOrDie(
          chrome_browser_state_.get())
          ->CreateAccountPermanentFolders();
    }
  }

  ~BookmarkModelFactoryTest() override = default;
};

TEST_P(BookmarkModelFactoryTest, IsBookmarked) {
  const GURL kUrl1("https://foo.com/");
  const GURL kUrl2("https://bar.com/");
  const GURL kUrl3("https://baz.com/");

  local_or_syncable_bookmark_model_->AddURL(
      local_or_syncable_bookmark_model_->bookmark_bar_node(), 0, u"title",
      kUrl1);
  account_bookmark_model_->AddURL(account_bookmark_model_->bookmark_bar_node(),
                                  0, u"title", kUrl2);

  ASSERT_TRUE(local_or_syncable_bookmark_model_->IsBookmarked(kUrl1));
  ASSERT_FALSE(local_or_syncable_bookmark_model_->IsBookmarked(kUrl2));
  ASSERT_FALSE(local_or_syncable_bookmark_model_->IsBookmarked(kUrl3));

  ASSERT_FALSE(account_bookmark_model_->IsBookmarked(kUrl1));
  ASSERT_TRUE(account_bookmark_model_->IsBookmarked(kUrl2));
  ASSERT_FALSE(account_bookmark_model_->IsBookmarked(kUrl3));

  // The merged view should return true if either of the two underlying trees
  // has the URL bookmarked.
  EXPECT_TRUE(bookmark_model_->IsBookmarked(kUrl1));
  EXPECT_TRUE(bookmark_model_->IsBookmarked(kUrl2));
  EXPECT_FALSE(bookmark_model_->IsBookmarked(kUrl3));
}

TEST_P(BookmarkModelFactoryTest, GetUniqueUrls) {
  const GURL kUrl1("https://foo.com/");
  const GURL kUrl2("https://bar.com/");
  const GURL kUrl3("https://baz.com/");

  local_or_syncable_bookmark_model_->AddURL(
      local_or_syncable_bookmark_model_->bookmark_bar_node(), 0, u"title1",
      kUrl1);
  account_bookmark_model_->AddURL(account_bookmark_model_->bookmark_bar_node(),
                                  0, u"title2", kUrl2);

  // `kUrl3` exists in both.
  local_or_syncable_bookmark_model_->AddURL(
      local_or_syncable_bookmark_model_->bookmark_bar_node(), 0, u"title3",
      kUrl3);
  account_bookmark_model_->AddURL(account_bookmark_model_->bookmark_bar_node(),
                                  0, u"title4", kUrl3);

  EXPECT_THAT(
      bookmark_model_->GetUniqueUrls(),
      UnorderedElementsAre(HasUrl(kUrl1), HasUrl(kUrl2), HasUrl(kUrl3)));
}

INSTANTIATE_TEST_SUITE_P(UnifiedBookmarkModel,
                         BookmarkModelFactoryTest,
                         testing::Bool());

}  // namespace ios
