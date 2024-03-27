// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/bookmarks/model/bookmark_model_factory.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/core_bookmark_model.h"
#include "components/bookmarks/browser/url_and_title.h"
#include "components/bookmarks/common/bookmark_constants.h"
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

const char kLocalOrSyncableIdsReassignedMetricName[] =
    "Bookmarks.IdsReassigned.OnProfileLoad.LocalOrSyncable";
const char kAccountIdsReassignedMetricName[] =
    "Bookmarks.IdsReassigned.OnProfileLoad.Account";

MATCHER_P(HasUrl, expected_url, "") {
  return arg.url == expected_url;
}

const base::FilePath& GetTestDataDir() {
  static base::NoDestructor<base::FilePath> dir([]() {
    base::FilePath dir;
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &dir);
    return dir.AppendASCII("components")
        .AppendASCII("test")
        .AppendASCII("data");
  }());
  return *dir;
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

class BookmarkModelFactoryWithIdCollisionsWithinOneFileOnDiskTest
    : public BookmarkModelFactoryTest {
 protected:
  BookmarkModelFactoryWithIdCollisionsWithinOneFileOnDiskTest() {}
  ~BookmarkModelFactoryWithIdCollisionsWithinOneFileOnDiskTest() override {}

  void SetUpBrowserStateBeforeCreatingServices() override {
    BookmarkModelFactoryTest::SetUpBrowserStateBeforeCreatingServices();

    // Populate the JSON files on disk, in a way that the local-or-syncable file
    // has duplicate IDs that trigger the reassignment of bookmark IDs. This
    // setup is similar to the components/bookmarks test
    // ModelLoaderTest.LoadTwoFilesWhereFirstHasInternalIdCollisions.
    ASSERT_TRUE(base::CopyFile(
        GetTestDataDir().AppendASCII("bookmarks/model_with_duplicate_ids.json"),
        chrome_browser_state_->GetStatePath().Append(
            bookmarks::kLocalOrSyncableBookmarksFileName)));
    ASSERT_TRUE(base::CopyFile(GetTestDataDir().AppendASCII(
                                   "bookmarks/model_with_sync_metadata_2.json"),
                               chrome_browser_state_->GetStatePath().Append(
                                   bookmarks::kAccountBookmarksFileName)));
  }

  base::HistogramTester histogram_tester_;
};

TEST_P(BookmarkModelFactoryWithIdCollisionsWithinOneFileOnDiskTest,
       ReassignIdsAndLogMetrics) {
  histogram_tester_.ExpectUniqueSample(kAccountIdsReassignedMetricName,
                                       /*sample=*/false,
                                       /*expected_bucket_count=*/1);
  histogram_tester_.ExpectUniqueSample(kLocalOrSyncableIdsReassignedMetricName,
                                       /*sample=*/true,
                                       /*expected_bucket_count=*/1);
}

INSTANTIATE_TEST_SUITE_P(
    UnifiedBookmarkModel,
    BookmarkModelFactoryWithIdCollisionsWithinOneFileOnDiskTest,
    testing::Bool());

class BookmarkModelFactoryWithIdCollisionsAcrossTwoFilesOnDiskTest
    : public BookmarkModelFactoryTest {
 protected:
  BookmarkModelFactoryWithIdCollisionsAcrossTwoFilesOnDiskTest() {}
  ~BookmarkModelFactoryWithIdCollisionsAcrossTwoFilesOnDiskTest() override {}

  void SetUpBrowserStateBeforeCreatingServices() override {
    BookmarkModelFactoryTest::SetUpBrowserStateBeforeCreatingServices();

    // Populate the JSON files on disk, in a way that there are ID collisions
    // across, which is achieved by reusing the very same file content. This
    // setup is similar to the components/bookmarks test
    // ModelLoaderTest.LoadTwoFilesWithCollidingIdsAcross.
    ASSERT_TRUE(
        base::CopyFile(GetTestDataDir().AppendASCII(
                           "bookmarks/model_with_sync_metadata_1.json"),
                       chrome_browser_state_->GetStatePath().Append(
                           bookmarks::kLocalOrSyncableBookmarksFileName)));
    ASSERT_TRUE(base::CopyFile(GetTestDataDir().AppendASCII(
                                   "bookmarks/model_with_sync_metadata_1.json"),
                               chrome_browser_state_->GetStatePath().Append(
                                   bookmarks::kAccountBookmarksFileName)));
  }

  base::HistogramTester histogram_tester_;
};

TEST_P(BookmarkModelFactoryWithIdCollisionsAcrossTwoFilesOnDiskTest,
       ReassignIdsAndLogMetrics) {
  histogram_tester_.ExpectUniqueSample(kAccountIdsReassignedMetricName,
                                       /*sample=*/false,
                                       /*expected_bucket_count=*/1);
  // If and only if a single BookmarkModel instance is used, the ID collisions
  // across two files are detected and reassigned.
  histogram_tester_.ExpectUniqueSample(
      kLocalOrSyncableIdsReassignedMetricName,
      /*sample=*/GetEnableBookmarkFoldersForAccountStorageTestParam(),
      /*expected_bucket_count=*/1);
}

INSTANTIATE_TEST_SUITE_P(
    UnifiedBookmarkModel,
    BookmarkModelFactoryWithIdCollisionsAcrossTwoFilesOnDiskTest,
    testing::Bool());

}  // namespace ios
