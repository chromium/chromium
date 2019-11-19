// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/download/download_manager_mediator.h"

#import <UIKit/UIKit.h>

#include "base/run_loop.h"
#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "ios/chrome/browser/download/download_directory_util.h"
#import "ios/chrome/browser/download/google_drive_app_util.h"
#import "ios/chrome/test/fakes/fake_download_manager_consumer.h"
#import "ios/web/public/test/fakes/fake_download_task.h"
#include "ios/web/public/test/web_task_environment.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_fetcher_response_writer.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::WaitUntilConditionOrTimeout;

namespace {

// Constants for configuring a fake download task.
const char kTestUrl[] = "https://chromium.test/download.txt";
const char kTestMimeType[] = "text/html";
const int64_t kTestTotalBytes = 10;
const int64_t kTestReceivedBytes = 0;
NSString* const kTestSuggestedFileName = @"important_file.zip";

}  // namespace

// Test fixture for testing DownloadManagerMediator class.
class DownloadManagerMediatorTest : public PlatformTest {
 protected:
  DownloadManagerMediatorTest()
      : consumer_([[FakeDownloadManagerConsumer alloc] init]),
        application_(OCMClassMock([UIApplication class])),
        task_(GURL(kTestUrl), kTestMimeType) {
    OCMStub([application_ sharedApplication]).andReturn(application_);
  }
  ~DownloadManagerMediatorTest() override { [application_ stopMocking]; }

  web::FakeDownloadTask* task() { return &task_; }

  DownloadManagerMediator mediator_;
  FakeDownloadManagerConsumer* consumer_;
  id application_;

 private:
  web::WebTaskEnvironment task_environment_;
  web::FakeDownloadTask task_;
};

// Tests starting the download and immediately destroying the task.
// DownloadManagerMediator should not crash.
TEST_F(DownloadManagerMediatorTest, DestoryTaskAfterStart) {
  auto task =
      std::make_unique<web::FakeDownloadTask>(GURL(kTestUrl), kTestMimeType);
  mediator_.SetDownloadTask(task.get());
  mediator_.StartDowloading();
  task.reset();
}

// Tests starting the download. Verifies that download task is started and its
// file writer is configured to write into download directory.
TEST_F(DownloadManagerMediatorTest, Start) {
  task()->SetSuggestedFilename(
      base::SysNSStringToUTF16(kTestSuggestedFileName));
  mediator_.SetDownloadTask(task());
  mediator_.SetConsumer(consumer_);
  mediator_.StartDowloading();

  // Starting download is async for task and sync for consumer.
  EXPECT_EQ(kDownloadManagerStateInProgress, consumer_.state);
  EXPECT_FALSE(consumer_.installDriveButtonVisible);
  ASSERT_TRUE(
      WaitUntilConditionOrTimeout(base::test::ios::kWaitForDownloadTimeout, ^{
        base::RunLoop().RunUntilIdle();
        return task()->GetState() == web::DownloadTask::State::kInProgress;
      }));

  // Download file should be located in download directory.
  base::FilePath file =
      task()->GetResponseWriter()->AsFileWriter()->file_path();
  base::FilePath download_dir;
  ASSERT_TRUE(GetDownloadsDirectory(&download_dir));
  EXPECT_TRUE(download_dir.IsParent(file));
}

// Tests starting and failing the download. Simulates download failure from
// inability to create a file writer.
TEST_F(DownloadManagerMediatorTest, StartFailure) {
  // Writer can not be created without file name, which will fail the download.
  mediator_.SetDownloadTask(task());
  mediator_.SetConsumer(consumer_);
  mediator_.StartDowloading();

  // Writer is created by a background task, so wait for failure.
  ASSERT_TRUE(
      WaitUntilConditionOrTimeout(base::test::ios::kWaitForDownloadTimeout, ^{
        base::RunLoop().RunUntilIdle();
        return consumer_.state == kDownloadManagerStateFailed;
      }));
  EXPECT_FALSE(consumer_.installDriveButtonVisible);
}

// Tests that consumer is updated right after it's set.
TEST_F(DownloadManagerMediatorTest, ConsumerInstantUpdate) {
  OCMStub([application_ canOpenURL:GetGoogleDriveAppUrl()]).andReturn(YES);

  task()->SetDone(true);
  task()->SetSuggestedFilename(
      base::SysNSStringToUTF16(kTestSuggestedFileName));
  task()->SetTotalBytes(kTestTotalBytes);
  task()->SetReceivedBytes(kTestReceivedBytes);
  task()->SetPercentComplete(80);

  mediator_.SetDownloadTask(task());
  mediator_.SetConsumer(consumer_);

  EXPECT_EQ(kDownloadManagerStateSucceeded, consumer_.state);
  EXPECT_FALSE(consumer_.installDriveButtonVisible);
  EXPECT_NSEQ(kTestSuggestedFileName, consumer_.fileName);
  EXPECT_EQ(kTestTotalBytes, consumer_.countOfBytesExpectedToReceive);
  EXPECT_EQ(kTestReceivedBytes, consumer_.countOfBytesReceived);
  EXPECT_FLOAT_EQ(0.8f, consumer_.progress);
}

// Tests that consumer changes the state to kDownloadManagerStateFailed if task
// competed with an error.
TEST_F(DownloadManagerMediatorTest, ConsumerFailedStateUpdate) {
  mediator_.SetDownloadTask(task());
  mediator_.SetConsumer(consumer_);

  task()->SetErrorCode(net::ERR_INTERNET_DISCONNECTED);
  task()->SetDone(true);
  EXPECT_EQ(kDownloadManagerStateFailed, consumer_.state);
  EXPECT_FALSE(consumer_.installDriveButtonVisible);
}

// Tests that consumer changes the state to kDownloadManagerStateSucceeded if
// task competed without an error.
TEST_F(DownloadManagerMediatorTest, ConsumerSuceededStateUpdate) {
  OCMStub([application_ canOpenURL:GetGoogleDriveAppUrl()]).andReturn(YES);

  mediator_.SetDownloadTask(task());
  mediator_.SetConsumer(consumer_);

  task()->SetDone(true);
  EXPECT_EQ(kDownloadManagerStateSucceeded, consumer_.state);
  EXPECT_FALSE(consumer_.installDriveButtonVisible);
}

// Tests that consumer changes the state to kDownloadManagerStateSucceeded if
// task competed without an error and Google Drive app is not installed.
TEST_F(DownloadManagerMediatorTest,
       ConsumerSuceededStateUpdateWithoutDriveAppInstalled) {
  OCMStub([application_ canOpenURL:GetGoogleDriveAppUrl()]).andReturn(NO);

  mediator_.SetDownloadTask(task());
  mediator_.SetConsumer(consumer_);

  task()->SetDone(true);
  EXPECT_EQ(kDownloadManagerStateSucceeded, consumer_.state);
  EXPECT_TRUE(consumer_.installDriveButtonVisible);
}

// Tests that consumer changes the state to kDownloadManagerStateInProgress if
// the task has started.
TEST_F(DownloadManagerMediatorTest, ConsumerInProgressStateUpdate) {
  mediator_.SetDownloadTask(task());
  mediator_.SetConsumer(consumer_);

  task()->Start(std::make_unique<net::URLFetcherStringWriter>());
  EXPECT_EQ(kDownloadManagerStateInProgress, consumer_.state);
  EXPECT_FALSE(consumer_.installDriveButtonVisible);
  EXPECT_EQ(0.0, consumer_.progress);
}
