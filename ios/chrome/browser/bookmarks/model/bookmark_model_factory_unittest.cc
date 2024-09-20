// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/bookmarks/model/bookmark_model_factory.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/url_and_title.h"
#include "components/bookmarks/common/bookmark_constants.h"
#include "ios/chrome/browser/bookmarks/model/bookmark_ios_unit_test_support.h"
#include "ios/chrome/browser/bookmarks/model/bookmark_model_factory.h"
#include "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
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

class BookmarkModelFactoryTest : public BookmarkIOSUnitTestSupport {
 public:
  BookmarkModelFactoryTest() = default;
  ~BookmarkModelFactoryTest() override = default;
};

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
        profile_->GetStatePath().Append(
            bookmarks::kLocalOrSyncableBookmarksFileName)));
    ASSERT_TRUE(base::CopyFile(
        GetTestDataDir().AppendASCII(
            "bookmarks/model_with_sync_metadata_2.json"),
        profile_->GetStatePath().Append(bookmarks::kAccountBookmarksFileName)));
  }

  base::HistogramTester histogram_tester_;
};

TEST_F(BookmarkModelFactoryWithIdCollisionsWithinOneFileOnDiskTest,
       ReassignIdsAndLogMetrics) {
  histogram_tester_.ExpectUniqueSample(kAccountIdsReassignedMetricName,
                                       /*sample=*/false,
                                       /*expected_bucket_count=*/1);
  histogram_tester_.ExpectUniqueSample(kLocalOrSyncableIdsReassignedMetricName,
                                       /*sample=*/true,
                                       /*expected_bucket_count=*/1);
}

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
                       profile_->GetStatePath().Append(
                           bookmarks::kLocalOrSyncableBookmarksFileName)));
    ASSERT_TRUE(base::CopyFile(
        GetTestDataDir().AppendASCII(
            "bookmarks/model_with_sync_metadata_1.json"),
        profile_->GetStatePath().Append(bookmarks::kAccountBookmarksFileName)));
  }

  base::HistogramTester histogram_tester_;
};

TEST_F(BookmarkModelFactoryWithIdCollisionsAcrossTwoFilesOnDiskTest,
       ReassignIdsAndLogMetrics) {
  histogram_tester_.ExpectUniqueSample(kAccountIdsReassignedMetricName,
                                       /*sample=*/false,
                                       /*expected_bucket_count=*/1);
  // The ID collisions across two files are detected and reassigned.
  histogram_tester_.ExpectUniqueSample(kLocalOrSyncableIdsReassignedMetricName,
                                       /*sample=*/1,
                                       /*expected_bucket_count=*/1);
}

}  // namespace ios
