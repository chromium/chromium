// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/download/web_state_content_download_task.h"

#import "base/task/thread_pool.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {
const char kValidUrl[] = "https://foo.test";
NSString* const kMethodGet = @"GET";
const char kContentDisposition[] = "attachment; filename=file.test";
const char kMimeType[] = "application/pdf";
}  // namespace

// Test fixture for testing WebStateContentDownloadTask class.
class WebStateContentDownloadTaskTest : public PlatformTest {
 protected:
  web::WebTaskEnvironment task_environment_;
  web::FakeWebState web_state_;
};

// Test successful download.
TEST_F(WebStateContentDownloadTaskTest, TestDownloadContentSuccess) {
  web::WebStateContentDownloadTask task(
      &web_state_, GURL(kValidUrl), kMethodGet, kContentDisposition,
      /*total_bytes=*/-1, kMimeType, [[NSUUID UUID] UUIDString],
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_BLOCKING}));

  task.Start(base::FilePath("/tmp/test"));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(web::DownloadTask::State::kInProgress, task.GetState());
  web_state_.OnDownloadFinished(nil);
  EXPECT_EQ(web::DownloadTask::State::kComplete, task.GetState());
}

// Test Failing download.
TEST_F(WebStateContentDownloadTaskTest, TestDownloadContentSuccessFail) {
  web::WebStateContentDownloadTask task(
      &web_state_, GURL(kValidUrl), kMethodGet, kContentDisposition,
      /*total_bytes=*/-1, kMimeType, [[NSUUID UUID] UUIDString],
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_BLOCKING}));

  task.Start(base::FilePath("/tmp/test"));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(web::DownloadTask::State::kInProgress, task.GetState());
  web_state_.OnDownloadFinished([NSError errorWithDomain:@"test"
                                                    code:1
                                                userInfo:nil]);
  EXPECT_EQ(web::DownloadTask::State::kFailed, task.GetState());
}
