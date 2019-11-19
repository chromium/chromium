// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/ar_quick_look_tab_helper.h"

#include <memory>
#include <string>

#include "base/run_loop.h"
#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "ios/chrome/browser/download/download_directory_util.h"
#include "ios/chrome/browser/download/usdz_mime_type.h"
#import "ios/chrome/test/fakes/fake_ar_quick_look_tab_helper_delegate.h"
#import "ios/web/public/test/fakes/fake_download_task.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "ios/web/public/test/web_task_environment.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_fetcher_response_writer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForDownloadTimeout;

namespace {

const char kHistogramName[] = "Download.IOSDownloadARModelState.USDZ";
const char kUrl[] = "https://test.test/";

NSString* const kTestSuggestedFileName = @"important_file.zip";
NSString* const kTestUsdzFileName = @"important_file.usdz";

}  // namespace

// Test fixture for testing ARQuickLookTabHelper class.
class ARQuickLookTabHelperTest : public PlatformTest,
                                 public ::testing::WithParamInterface<char*> {
 protected:
  ARQuickLookTabHelperTest()
      : delegate_([[FakeARQuickLookTabHelperDelegate alloc] init]) {
    ARQuickLookTabHelper::CreateForWebState(&web_state_);
    ARQuickLookTabHelper::FromWebState(&web_state_)->set_delegate(delegate_);
  }

  ARQuickLookTabHelper* tab_helper() {
    return ARQuickLookTabHelper::FromWebState(&web_state_);
  }

  FakeARQuickLookTabHelperDelegate* delegate() { return delegate_; }

  base::HistogramTester* histogram_tester() { return &histogram_tester_; }

 private:
  web::WebTaskEnvironment task_environment_;
  web::TestWebState web_state_;
  FakeARQuickLookTabHelperDelegate* delegate_;
  base::HistogramTester histogram_tester_;
};

// Tests successfully downloading a USDZ file with the appropriate extension.
TEST_F(ARQuickLookTabHelperTest, SuccessFileExtention) {
  auto task = std::make_unique<web::FakeDownloadTask>(GURL(kUrl), "other");
  task->SetSuggestedFilename(base::SysNSStringToUTF16(kTestUsdzFileName));
  web::FakeDownloadTask* task_ptr = task.get();
  tab_helper()->Download(std::move(task));
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForDownloadTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return task_ptr->GetState() == web::DownloadTask::State::kInProgress;
  }));
  task_ptr->SetDone(true);
  EXPECT_EQ(1U, delegate().fileURLs.count);
  EXPECT_TRUE([delegate().fileURLs.firstObject isKindOfClass:[NSURL class]]);

  // Downloaded file should be located in download directory.
  base::FilePath file =
      task_ptr->GetResponseWriter()->AsFileWriter()->file_path();
  base::FilePath download_dir;
  ASSERT_TRUE(GetDownloadsDirectory(&download_dir));
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
  task->SetSuggestedFilename(base::SysNSStringToUTF16(kTestSuggestedFileName));
  web::FakeDownloadTask* task_ptr = task.get();
  tab_helper()->Download(std::move(task));
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForDownloadTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return task_ptr->GetState() == web::DownloadTask::State::kInProgress;
  }));
  task_ptr->SetDone(true);
  EXPECT_EQ(1U, delegate().fileURLs.count);
  EXPECT_TRUE([delegate().fileURLs.firstObject isKindOfClass:[NSURL class]]);

  // Downloaded file should be located in download directory.
  base::FilePath file =
      task_ptr->GetResponseWriter()->AsFileWriter()->file_path();
  base::FilePath download_dir;
  ASSERT_TRUE(GetDownloadsDirectory(&download_dir));
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

// Tests replacing the download task brefore it's started.
TEST_P(ARQuickLookTabHelperTest, ReplaceUnstartedDownload) {
  auto task = std::make_unique<web::FakeDownloadTask>(GURL(kUrl), GetParam());
  task->SetSuggestedFilename(base::SysNSStringToUTF16(kTestSuggestedFileName));
  tab_helper()->Download(std::move(task));

  auto task2 = std::make_unique<web::FakeDownloadTask>(GURL(kUrl), GetParam());
  task2->SetSuggestedFilename(base::SysNSStringToUTF16(kTestSuggestedFileName));
  web::FakeDownloadTask* task_ptr2 = task2.get();
  tab_helper()->Download(std::move(task2));
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForDownloadTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return task_ptr2->GetState() == web::DownloadTask::State::kInProgress;
  }));
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
      1);
  histogram_tester()->ExpectBucketCount(
      kHistogramName,
      static_cast<base::HistogramBase::Sample>(
          IOSDownloadARModelState::kSuccessful),
      1);
}

// Tests replacing the download task while it's in progress.
TEST_P(ARQuickLookTabHelperTest, ReplaceInProgressDownload) {
  auto task = std::make_unique<web::FakeDownloadTask>(GURL(kUrl), GetParam());
  task->SetSuggestedFilename(base::SysNSStringToUTF16(kTestSuggestedFileName));
  web::FakeDownloadTask* task_ptr = task.get();
  tab_helper()->Download(std::move(task));
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForDownloadTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return task_ptr->GetState() == web::DownloadTask::State::kInProgress;
  }));

  auto task2 = std::make_unique<web::FakeDownloadTask>(GURL(kUrl), GetParam());
  task2->SetSuggestedFilename(base::SysNSStringToUTF16(kTestSuggestedFileName));
  web::FakeDownloadTask* task_ptr2 = task2.get();
  tab_helper()->Download(std::move(task2));
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForDownloadTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return task_ptr2->GetState() == web::DownloadTask::State::kInProgress;
  }));
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
  task->SetSuggestedFilename(base::SysNSStringToUTF16(kTestSuggestedFileName));
  web::FakeDownloadTask* task_ptr = task.get();
  tab_helper()->Download(std::move(task));
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForDownloadTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return task_ptr->GetState() == web::DownloadTask::State::kInProgress;
  }));
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
  task->SetSuggestedFilename(base::SysNSStringToUTF16(kTestSuggestedFileName));
  web::FakeDownloadTask* task_ptr = task.get();
  tab_helper()->Download(std::move(task));
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForDownloadTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return task_ptr->GetState() == web::DownloadTask::State::kInProgress;
  }));
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
  task->SetSuggestedFilename(base::SysNSStringToUTF16(kTestSuggestedFileName));
  web::FakeDownloadTask* task_ptr = task.get();
  tab_helper()->Download(std::move(task));
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForDownloadTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return task_ptr->GetState() == web::DownloadTask::State::kInProgress;
  }));
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
  task->SetSuggestedFilename(base::SysNSStringToUTF16(kTestSuggestedFileName));
  web::FakeDownloadTask* task_ptr = task.get();
  tab_helper()->Download(std::move(task));
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForDownloadTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return task_ptr->GetState() == web::DownloadTask::State::kInProgress;
  }));
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
