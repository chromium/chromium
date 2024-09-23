// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/model/ar_quick_look_tab_helper.h"

#import <memory>
#import <string>

#import "base/run_loop.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "ios/chrome/browser/download/model/download_directory_util.h"
#import "ios/chrome/browser/download/model/mime_type_util.h"
#import "ios/chrome/test/fakes/fake_ar_quick_look_tab_helper_delegate.h"
#import "ios/web/public/test/download_task_test_util.h"
#import "ios/web/public/test/fakes/fake_download_task.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "net/base/apple/url_conversions.h"
#import "net/base/net_errors.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

const char kHistogramName[] = "Download.IOSDownloadARModelState.USDZ";
const char kUrl[] = "https://test.test/";
const char kUrlDisallowingScaling[] =
    "https://test.test/#allowsContentScaling=0";
const char kUrlWithCanonicalWebPageUrl[] =
    "https://test.test/#canonicalWebPageURL=https%3A%2F%2Fwww.google.com";
const char kCanonicalWebPageUrlValue[] = "https://www.google.com";

const base::FilePath::CharType kTestSuggestedFileName[] =
    FILE_PATH_LITERAL("important_file.zip");
const base::FilePath::CharType kTestUsdzFileName[] =
    FILE_PATH_LITERAL("important_file.usdz");

}  // namespace

// Test fixture for testing ARQuickLookTabHelper class.
class ARQuickLookTabHelperTest : public PlatformTest,
                                 public ::testing::WithParamInterface<char*> {
 protected:
  ARQuickLookTabHelperTest()
      : delegate_([[FakeARQuickLookTabHelperDelegate alloc] init]) {
    ARQuickLookTabHelper::GetOrCreateForWebState(&web_state_)
        ->set_delegate(delegate_);
  }

  ARQuickLookTabHelper* tab_helper() {
    return ARQuickLookTabHelper::GetOrCreateForWebState(&web_state_);
  }

  FakeARQuickLookTabHelperDelegate* delegate() { return delegate_; }

  base::HistogramTester* histogram_tester() { return &histogram_tester_; }

 private:
  web::WebTaskEnvironment task_environment_;
  web::FakeWebState web_state_;
  FakeARQuickLookTabHelperDelegate* delegate_;
  base::HistogramTester histogram_tester_;
};

// Tests successfully downloading a USDZ file with the appropriate extension.
TEST_F(ARQuickLookTabHelperTest, SuccessFileExtention) {
  auto task = std::make_unique<web::FakeDownloadTask>(GURL(kUrl), "other");
  task->SetGeneratedFileName(base::FilePath(kTestUsdzFileName));
  web::FakeDownloadTask* task_ptr = task.get();

  {
    web::test::WaitDownloadTaskUpdated observer(task_ptr);
    tab_helper()->Download(std::move(task));
    observer.Wait();
  }

  task_ptr->SetDone(true);
  EXPECT_EQ(1U, delegate().fileURLs.count);
  EXPECT_TRUE([delegate().fileURLs.firstObject isKindOfClass:[NSURL class]]);
  EXPECT_TRUE(delegate().allowsContentScaling);
  EXPECT_FALSE(delegate().canonicalWebPageURL);

  // Downloaded file should be located in download directory.
  base::FilePath file = task_ptr->GetResponsePath();
  base::FilePath download_dir;
  ASSERT_TRUE(GetTempDownloadsDirectory(&download_dir));
  EXPECT_TRUE(download_dir.IsParent(file));

  histogram_tester()->ExpectBucketCount(
      kHistogramName,
      static_cast<base::HistogramBase::Sample>(
          IOSDownloadARModelState::kCreated),
      1);
  histogram_tester()->ExpectBucketCount(
      kHistogramName,
      static_cast<base::HistogramBase::Sample>(
          IOSDownloadARModelState::kStarted),
      1);
  histogram_tester()->ExpectBucketCount(
      kHistogramName,
      static_cast<base::HistogramBase::Sample>(
          IOSDownloadARModelState::kSuccessful),
      1);
}

// Tests successfully downloading a USDZ file with the appropriate content-type.
TEST_P(ARQuickLookTabHelperTest, SuccessContentType) {
  auto task = std::make_unique<web::FakeDownloadTask>(GURL(kUrl), GetParam());
  task->SetGeneratedFileName(base::FilePath(kTestSuggestedFileName));
  web::FakeDownloadTask* task_ptr = task.get();

  {
    web::test::WaitDownloadTaskUpdated observer(task_ptr);
    tab_helper()->Download(std::move(task));
    observer.Wait();
  }

  task_ptr->SetDone(true);
  EXPECT_EQ(1U, delegate().fileURLs.count);
  EXPECT_TRUE([delegate().fileURLs.firstObject isKindOfClass:[NSURL class]]);
  EXPECT_TRUE(delegate().allowsContentScaling);
  EXPECT_FALSE(delegate().canonicalWebPageURL);

  // Downloaded file should be located in download directory.
  base::FilePath file = task_ptr->GetResponsePath();
  base::FilePath download_dir;
  ASSERT_TRUE(GetTempDownloadsDirectory(&download_dir));
  EXPECT_TRUE(download_dir.IsParent(file));

  histogram_tester()->ExpectBucketCount(
      kHistogramName,
      static_cast<base::HistogramBase::Sample>(
          IOSDownloadARModelState::kCreated),
      1);
  histogram_tester()->ExpectBucketCount(
      kHistogramName,
      static_cast<base::HistogramBase::Sample>(
          IOSDownloadARModelState::kStarted),
      1);
  histogram_tester()->ExpectBucketCount(
      kHistogramName,
      static_cast<base::HistogramBase::Sample>(
          IOSDownloadARModelState::kSuccessful),
      1);
}

// Tests successfully downloading a USDZ file when the specified URL includes
// a fragment that disallows content scaling.
TEST_P(ARQuickLookTabHelperTest, DisallowsContentScaling) {
  auto task = std::make_unique<web::FakeDownloadTask>(
      GURL(kUrlDisallowingScaling), GetParam());
  task->SetGeneratedFileName(base::FilePath(kTestSuggestedFileName));
  web::FakeDownloadTask* task_ptr = task.get();

  {
    web::test::WaitDownloadTaskUpdated observer(task_ptr);
    tab_helper()->Download(std::move(task));
    observer.Wait();
  }

  task_ptr->SetDone(true);
  EXPECT_EQ(1U, delegate().fileURLs.count);
  EXPECT_TRUE([delegate().fileURLs.firstObject isKindOfClass:[NSURL class]]);
  EXPECT_FALSE(delegate().allowsContentScaling);
  EXPECT_FALSE(delegate().canonicalWebPageURL);

  // Downloaded file should be located in download directory.
  base::FilePath file = task_ptr->GetResponsePath();
  base::FilePath download_dir;
  ASSERT_TRUE(GetTempDownloadsDirectory(&download_dir));
  EXPECT_TRUE(download_dir.IsParent(file));

  histogram_tester()->ExpectBucketCount(
      kHistogramName,
      static_cast<base::HistogramBase::Sample>(
          IOSDownloadARModelState::kCreated),
      1);
  histogram_tester()->ExpectBucketCount(
      kHistogramName,
      static_cast<base::HistogramBase::Sample>(
          IOSDownloadARModelState::kStarted),
      1);
  histogram_tester()->ExpectBucketCount(
      kHistogramName,
      static_cast<base::HistogramBase::Sample>(
          IOSDownloadARModelState::kSuccessful),
      1);
}

// Tests allowsContentScaling to be disabled given a specified URL includes a
// fragment that disallows content scaling and has a query string with more than
// one key-value pair.
TEST_P(ARQuickLookTabHelperTest, DisallowsContentScalingExtendedQuery) {
  auto task = std::make_unique<web::FakeDownloadTask>(
      GURL("https://test.test/#allowsContentScaling=0&testing=5"), GetParam());
  task->SetGeneratedFileName(base::FilePath(kTestSuggestedFileName));
  web::FakeDownloadTask* task_ptr = task.get();

  {
    web::test::WaitDownloadTaskUpdated observer(task_ptr);
    tab_helper()->Download(std::move(task));
    observer.Wait();
  }

  task_ptr->SetDone(true);
  EXPECT_FALSE(delegate().allowsContentScaling);
}

// Tests successfully downloading a USDZ file when the specified URL includes
// a fragment that is unrelated to content scaling.
TEST_P(ARQuickLookTabHelperTest, AllowsContentScaling) {
  auto task = std::make_unique<web::FakeDownloadTask>(
      GURL("https://test.test/#randomFragment=0"), GetParam());
  task->SetGeneratedFileName(base::FilePath(kTestSuggestedFileName));
  web::FakeDownloadTask* task_ptr = task.get();

  {
    web::test::WaitDownloadTaskUpdated observer(task_ptr);
    tab_helper()->Download(std::move(task));
    observer.Wait();
  }

  task_ptr->SetDone(true);
  EXPECT_EQ(1U, delegate().fileURLs.count);
  EXPECT_TRUE([delegate().fileURLs.firstObject isKindOfClass:[NSURL class]]);
  EXPECT_TRUE(delegate().allowsContentScaling);
  EXPECT_FALSE(delegate().canonicalWebPageURL);

  // Downloaded file should be located in download directory.
  base::FilePath file = task_ptr->GetResponsePath();
  base::FilePath download_dir;
  ASSERT_TRUE(GetTempDownloadsDirectory(&download_dir));
  EXPECT_TRUE(download_dir.IsParent(file));

  histogram_tester()->ExpectBucketCount(
      kHistogramName,
      static_cast<base::HistogramBase::Sample>(
          IOSDownloadARModelState::kCreated),
      1);
  histogram_tester()->ExpectBucketCount(
      kHistogramName,
      static_cast<base::HistogramBase::Sample>(
          IOSDownloadARModelState::kStarted),
      1);
  histogram_tester()->ExpectBucketCount(
      kHistogramName,
      static_cast<base::HistogramBase::Sample>(
          IOSDownloadARModelState::kSuccessful),
      1);
}

// Tests allowsContentScaling to be allowed given specified URLs where query
// value isn't 0
TEST_P(ARQuickLookTabHelperTest, AllowContentScalingEqualToOne) {
  auto task = std::make_unique<web::FakeDownloadTask>(
      GURL("https://test.test/#allowsContentScaling=1"), GetParam());
  task->SetGeneratedFileName(base::FilePath(kTestSuggestedFileName));
  web::FakeDownloadTask* task_ptr = task.get();

  {
    web::test::WaitDownloadTaskUpdated observer(task_ptr);
    tab_helper()->Download(std::move(task));
    observer.Wait();
  }

  task_ptr->SetDone(true);
  EXPECT_TRUE(delegate().allowsContentScaling);
}

// Tests allowsContentScaling to be allowed given specified URLs where query
// value is a random string
TEST_P(ARQuickLookTabHelperTest, AllowContentScalingEqualToRandomValue) {
  auto task = std::make_unique<web::FakeDownloadTask>(
      GURL("https://test.test/#allowsContentScaling=randomThing"), GetParam());
  task->SetGeneratedFileName(base::FilePath(kTestSuggestedFileName));
  web::FakeDownloadTask* task_ptr = task.get();

  {
    web::test::WaitDownloadTaskUpdated observer(task_ptr);
    tab_helper()->Download(std::move(task));
    observer.Wait();
  }

  task_ptr->SetDone(true);
  EXPECT_TRUE(delegate().allowsContentScaling);
}

// Tests successfully downloading a USDZ file when the specified URL includes
// a canonical url to use when presenting the share sheet.
TEST_P(ARQuickLookTabHelperTest, CanonicalWebPageURL) {
  auto task = std::make_unique<web::FakeDownloadTask>(
      GURL(kUrlWithCanonicalWebPageUrl), GetParam());
  task->SetGeneratedFileName(base::FilePath(kTestSuggestedFileName));
  web::FakeDownloadTask* task_ptr = task.get();

  {
    web::test::WaitDownloadTaskUpdated observer(task_ptr);
    tab_helper()->Download(std::move(task));
    observer.Wait();
  }

  task_ptr->SetDone(true);
  EXPECT_EQ(1U, delegate().fileURLs.count);
  EXPECT_TRUE([delegate().fileURLs.firstObject isKindOfClass:[NSURL class]]);
  EXPECT_TRUE(delegate().allowsContentScaling);
  EXPECT_NSEQ(delegate().canonicalWebPageURL,
              net::NSURLWithGURL(GURL(kCanonicalWebPageUrlValue)));

  // Downloaded file should be located in download directory.
  base::FilePath file = task_ptr->GetResponsePath();
  base::FilePath download_dir;
  ASSERT_TRUE(GetTempDownloadsDirectory(&download_dir));
  EXPECT_TRUE(download_dir.IsParent(file));

  histogram_tester()->ExpectBucketCount(
      kHistogramName,
      static_cast<base::HistogramBase::Sample>(
          IOSDownloadARModelState::kCreated),
      1);
  histogram_tester()->ExpectBucketCount(
      kHistogramName,
      static_cast<base::HistogramBase::Sample>(
          IOSDownloadARModelState::kStarted),
      1);
  histogram_tester()->ExpectBucketCount(
      kHistogramName,
      static_cast<base::HistogramBase::Sample>(
          IOSDownloadARModelState::kSuccessful),
      1);
}

// Tests replacing the download task before it make any progress.
TEST_P(ARQuickLookTabHelperTest, ReplaceStartedNoProgressDownload) {
  auto task = std::make_unique<web::FakeDownloadTask>(GURL(kUrl), GetParam());
  task->SetGeneratedFileName(base::FilePath(kTestSuggestedFileName));
  tab_helper()->Download(std::move(task));

  auto task2 = std::make_unique<web::FakeDownloadTask>(GURL(kUrl), GetParam());
  task2->SetGeneratedFileName(base::FilePath(kTestSuggestedFileName));
  web::FakeDownloadTask* task_ptr2 = task2.get();

  {
    web::test::WaitDownloadTaskUpdated observer(task_ptr2);
    tab_helper()->Download(std::move(task2));
    observer.Wait();
  }

  task_ptr2->SetDone(true);
  EXPECT_EQ(1U, delegate().fileURLs.count);
  EXPECT_TRUE([delegate().fileURLs.firstObject isKindOfClass:[NSURL class]]);

  histogram_tester()->ExpectBucketCount(
      kHistogramName,
      static_cast<base::HistogramBase::Sample>(
          IOSDownloadARModelState::kCreated),
      2);
  histogram_tester()->ExpectBucketCount(
      kHistogramName,
      static_cast<base::HistogramBase::Sample>(
          IOSDownloadARModelState::kStarted),
      2);
  histogram_tester()->ExpectBucketCount(
      kHistogramName,
      static_cast<base::HistogramBase::Sample>(
          IOSDownloadARModelState::kSuccessful),
      1);
}

// Tests replacing the download task while it's in progress.
TEST_P(ARQuickLookTabHelperTest, ReplaceInProgressDownload) {
  auto task = std::make_unique<web::FakeDownloadTask>(GURL(kUrl), GetParam());
  task->SetGeneratedFileName(base::FilePath(kTestSuggestedFileName));
  web::FakeDownloadTask* task_ptr = task.get();

  {
    web::test::WaitDownloadTaskUpdated observer(task_ptr);
    tab_helper()->Download(std::move(task));
    observer.Wait();
  }

  auto task2 = std::make_unique<web::FakeDownloadTask>(GURL(kUrl), GetParam());
  task2->SetGeneratedFileName(base::FilePath(kTestSuggestedFileName));
  web::FakeDownloadTask* task_ptr2 = task2.get();

  {
    web::test::WaitDownloadTaskUpdated observer(task_ptr2);
    tab_helper()->Download(std::move(task2));
    observer.Wait();
  }

  task_ptr2->SetDone(true);
  EXPECT_EQ(1U, delegate().fileURLs.count);
  EXPECT_TRUE([delegate().fileURLs.firstObject isKindOfClass:[NSURL class]]);

  histogram_tester()->ExpectBucketCount(
      kHistogramName,
      static_cast<base::HistogramBase::Sample>(
          IOSDownloadARModelState::kCreated),
      2);
  histogram_tester()->ExpectBucketCount(
      kHistogramName,
      static_cast<base::HistogramBase::Sample>(
          IOSDownloadARModelState::kStarted),
      2);
  histogram_tester()->ExpectBucketCount(
      kHistogramName,
      static_cast<base::HistogramBase::Sample>(
          IOSDownloadARModelState::kSuccessful),
      1);
}

// Tests the change of MIME type during the download. Can happen if the second
// response returned authentication page.
TEST_P(ARQuickLookTabHelperTest, MimeTypeChange) {
  auto task = std::make_unique<web::FakeDownloadTask>(GURL(kUrl), GetParam());
  task->SetGeneratedFileName(base::FilePath(kTestSuggestedFileName));
  web::FakeDownloadTask* task_ptr = task.get();

  {
    web::test::WaitDownloadTaskUpdated observer(task_ptr);
    tab_helper()->Download(std::move(task));
    observer.Wait();
  }

  task_ptr->SetMimeType("text/html");
  task_ptr->SetDone(true);
  EXPECT_EQ(0U, delegate().fileURLs.count);

  histogram_tester()->ExpectBucketCount(
      kHistogramName,
      static_cast<base::HistogramBase::Sample>(
          IOSDownloadARModelState::kCreated),
      1);
  histogram_tester()->ExpectBucketCount(
      kHistogramName,
      static_cast<base::HistogramBase::Sample>(
          IOSDownloadARModelState::kStarted),
      1);
  histogram_tester()->ExpectBucketCount(
      kHistogramName,
      static_cast<base::HistogramBase::Sample>(
          IOSDownloadARModelState::kWrongMimeTypeFailure),
      1);
}

// Tests the download failing with an error.
TEST_P(ARQuickLookTabHelperTest, DownloadError) {
  auto task = std::make_unique<web::FakeDownloadTask>(GURL(kUrl), GetParam());
  task->SetGeneratedFileName(base::FilePath(kTestSuggestedFileName));
  web::FakeDownloadTask* task_ptr = task.get();

  {
    web::test::WaitDownloadTaskUpdated observer(task_ptr);
    tab_helper()->Download(std::move(task));
    observer.Wait();
  }

  task_ptr->SetErrorCode(net::ERR_INTERNET_DISCONNECTED);
  task_ptr->SetDone(true);
  EXPECT_EQ(0U, delegate().fileURLs.count);

  histogram_tester()->ExpectBucketCount(
      kHistogramName,
      static_cast<base::HistogramBase::Sample>(
          IOSDownloadARModelState::kCreated),
      1);
  histogram_tester()->ExpectBucketCount(
      kHistogramName,
      static_cast<base::HistogramBase::Sample>(
          IOSDownloadARModelState::kStarted),
      1);
  histogram_tester()->ExpectBucketCount(
      kHistogramName,
      static_cast<base::HistogramBase::Sample>(
          IOSDownloadARModelState::kOtherFailure),
      1);
}

// Tests download HTTP response code of 401.
TEST_P(ARQuickLookTabHelperTest, UnauthorizedHttpResponse) {
  auto task = std::make_unique<web::FakeDownloadTask>(GURL(kUrl), GetParam());
  task->SetGeneratedFileName(base::FilePath(kTestSuggestedFileName));
  web::FakeDownloadTask* task_ptr = task.get();

  {
    web::test::WaitDownloadTaskUpdated observer(task_ptr);
    tab_helper()->Download(std::move(task));
    observer.Wait();
  }

  task_ptr->SetHttpCode(401);
  task_ptr->SetDone(true);
  EXPECT_EQ(0U, delegate().fileURLs.count);

  histogram_tester()->ExpectBucketCount(
      kHistogramName,
      static_cast<base::HistogramBase::Sample>(
          IOSDownloadARModelState::kCreated),
      1);
  histogram_tester()->ExpectBucketCount(
      kHistogramName,
      static_cast<base::HistogramBase::Sample>(
          IOSDownloadARModelState::kStarted),
      1);
  histogram_tester()->ExpectBucketCount(
      kHistogramName,
      static_cast<base::HistogramBase::Sample>(
          IOSDownloadARModelState::kUnauthorizedFailure),
      1);
}

// Tests download HTTP response code of 403.
TEST_P(ARQuickLookTabHelperTest, ForbiddenHttpResponse) {
  auto task = std::make_unique<web::FakeDownloadTask>(GURL(kUrl), GetParam());
  task->SetGeneratedFileName(base::FilePath(kTestSuggestedFileName));
  web::FakeDownloadTask* task_ptr = task.get();

  {
    web::test::WaitDownloadTaskUpdated observer(task_ptr);
    tab_helper()->Download(std::move(task));
    observer.Wait();
  }

  task_ptr->SetHttpCode(403);
  task_ptr->SetDone(true);
  EXPECT_EQ(0U, delegate().fileURLs.count);

  histogram_tester()->ExpectBucketCount(
      kHistogramName,
      static_cast<base::HistogramBase::Sample>(
          IOSDownloadARModelState::kCreated),
      1);
  histogram_tester()->ExpectBucketCount(
      kHistogramName,
      static_cast<base::HistogramBase::Sample>(
          IOSDownloadARModelState::kStarted),
      1);
  histogram_tester()->ExpectBucketCount(
      kHistogramName,
      static_cast<base::HistogramBase::Sample>(
          IOSDownloadARModelState::kUnauthorizedFailure),
      1);
}

INSTANTIATE_TEST_SUITE_P(,
                         ARQuickLookTabHelperTest,
                         ::testing::Values(kUsdzMimeType,
                                           kLegacyUsdzMimeType,
                                           kLegacyPixarUsdzMimeType));
