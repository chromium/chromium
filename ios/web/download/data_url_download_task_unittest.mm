// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/download/data_url_download_task.h"

#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/files/scoped_temp_dir.h"
#import "base/notreached.h"
#import "base/run_loop.h"
#import "base/scoped_observation.h"
#import "base/strings/utf_string_conversions.h"
#import "base/task/task_traits.h"
#import "base/task/thread_pool.h"
#import "base/test/ios/wait_util.h"
#import "ios/web/public/download/download_task_observer.h"
#import "ios/web/public/test/download_task_test_util.h"
#import "ios/web/public/test/fakes/fake_browser_state.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

namespace {

const char kValidDataUrl[] = "data:text/plain;base64,Q2hyb21pdW0=";
const char kEmptyDataUrl[] = "data://";
const char kContentDisposition[] = "attachment; filename=file.test";
const char kMimeType[] = "application/pdf";
const char kTestData[] = "Chromium";
const int kTestDataLen = sizeof(kTestData) - 1;
NSString* const kMethodGet = @"GET";

}  //  namespace

// Test fixture for testing DownloadTaskImplTest class.
class DataUrlDownloadTaskTest : public PlatformTest {
 protected:
  DataUrlDownloadTaskTest() {
    browser_state_.SetOffTheRecord(true);
    web_state_.SetBrowserState(&browser_state_);
  }

  web::WebTaskEnvironment task_environment_;
  FakeBrowserState browser_state_;
  FakeWebState web_state_;
};

// Tests valid data:// url downloads.
TEST_F(DataUrlDownloadTaskTest, ValidDataUrl) {
  // Create data:// url download task.
  DataUrlDownloadTask task(
      &web_state_, GURL(kValidDataUrl), kMethodGet, kContentDisposition,
      /*total_bytes=*/-1, kMimeType, [[NSUUID UUID] UUIDString],
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_BLOCKING}));

  // Start the task and wait for completion.
  {
    web::test::WaitDownloadTaskDone observer(&task);
    task.Start(base::FilePath());
    observer.Wait();
  }

  // Verify the state of downloaded task.
  EXPECT_EQ(DownloadTask::State::kComplete, task.GetState());
  EXPECT_EQ(net::OK, task.GetErrorCode());
  EXPECT_EQ(kTestDataLen, task.GetTotalBytes());
  EXPECT_EQ(kTestDataLen, task.GetReceivedBytes());
  EXPECT_EQ(100, task.GetPercentComplete());
  EXPECT_EQ("text/plain", task.GetMimeType());
  EXPECT_TRUE(task.GetResponsePath().empty());
  EXPECT_NSEQ(@(kTestData),
              [[NSString alloc]
                  initWithData:web::test::GetDownloadTaskResponseData(&task)
                      encoding:NSUTF8StringEncoding]);
}

// Tests valid data:// url downloads to a file.
TEST_F(DataUrlDownloadTaskTest, ValidUrlToFile) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());

  // Create data:// url download task.
  DataUrlDownloadTask task(
      &web_state_, GURL(kValidDataUrl), kMethodGet, kContentDisposition,
      /*total_bytes=*/-1, kMimeType, [[NSUUID UUID] UUIDString],
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_BLOCKING}));

  base::FilePath path =
      scoped_temp_dir.GetPath().Append(task.GenerateFileName());

  // Start the task and wait for completion.
  {
    web::test::WaitDownloadTaskDone observer(&task);
    task.Start(path);
    observer.Wait();
  }

  // Verify the state of downloaded task.
  EXPECT_EQ(DownloadTask::State::kComplete, task.GetState());
  EXPECT_EQ(net::OK, task.GetErrorCode());
  EXPECT_EQ(kTestDataLen, task.GetTotalBytes());
  EXPECT_EQ(kTestDataLen, task.GetReceivedBytes());
  EXPECT_EQ(100, task.GetPercentComplete());
  EXPECT_EQ("text/plain", task.GetMimeType());
  EXPECT_NSEQ(@(kTestData),
              [[NSString alloc]
                  initWithData:web::test::GetDownloadTaskResponseData(&task)
                      encoding:NSUTF8StringEncoding]);

  std::string file_content;
  EXPECT_EQ(path, task.GetResponsePath());
  ASSERT_TRUE(base::ReadFileToString(task.GetResponsePath(), &file_content));
  EXPECT_EQ(file_content, std::string(kTestData));
}

// Tests valid data:// url downloads to a non-existent location.
TEST_F(DataUrlDownloadTaskTest, ValidUrlNonExistentFile) {
  // Create data:// url download task.
  DataUrlDownloadTask task(
      &web_state_, GURL(kValidDataUrl), kMethodGet, kContentDisposition,
      /*total_bytes=*/-1, kMimeType, [[NSUUID UUID] UUIDString],
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_BLOCKING}));

  // Start the task and wait for completion.
  {
    web::test::WaitDownloadTaskDone observer(&task);
    task.Start(base::FilePath(FILE_PATH_LITERAL("/no-such-dir/file.txt")));
    observer.Wait();
  }

  // Verify the state of downloaded task.
  EXPECT_EQ(DownloadTask::State::kFailed, task.GetState());
  EXPECT_EQ(net::ERR_ACCESS_DENIED, task.GetErrorCode());
  EXPECT_EQ(-1, task.GetTotalBytes());
  EXPECT_EQ(0, task.GetReceivedBytes());
  EXPECT_EQ(0, task.GetPercentComplete());
  EXPECT_NSEQ(@"",
              [[NSString alloc]
                  initWithData:web::test::GetDownloadTaskResponseData(&task)
                      encoding:NSUTF8StringEncoding]);
}

// Tests empty data:// url downloads.
TEST_F(DataUrlDownloadTaskTest, EmptyDataUrl) {
  // Create data:// url download task.
  DataUrlDownloadTask task(
      &web_state_, GURL(kEmptyDataUrl), kMethodGet, kContentDisposition,
      /*total_bytes=*/-1, kMimeType, [[NSUUID UUID] UUIDString],
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_BLOCKING}));

  // Start the task and wait for completion.
  {
    web::test::WaitDownloadTaskDone observer(&task);
    task.Start(base::FilePath());
    observer.Wait();
  }

  // Verify the state of downloaded task.
  EXPECT_EQ(DownloadTask::State::kFailed, task.GetState());
  EXPECT_EQ(net::ERR_INVALID_URL, task.GetErrorCode());
  EXPECT_EQ(-1, task.GetTotalBytes());
  EXPECT_EQ(0, task.GetReceivedBytes());
  EXPECT_EQ(0, task.GetPercentComplete());
  EXPECT_NSEQ(@"",
              [[NSString alloc]
                  initWithData:web::test::GetDownloadTaskResponseData(&task)
                      encoding:NSUTF8StringEncoding]);
}

}  // namespace web
