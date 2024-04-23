// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/download/download_native_task_impl.h"

#import "base/ios/ios_util.h"
#import "base/task/task_traits.h"
#import "base/task/thread_pool.h"
#import "base/test/task_environment.h"
#import "ios/web/public/test/download_task_test_util.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/test/fakes/fake_native_task_bridge.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace web {

namespace {

const char kUrl[] = "chromium://download.test/";
const char kContentDisposition[] = "attachment; filename=file.test";
const char kMimeType[] = "application/pdf";
const char kIdentifier[] = "testIdentifier";
const base::FilePath::CharType kTestFileName[] = FILE_PATH_LITERAL("file.test");
NSString* const kHttpMethod = @"POST";

}  //  namespace

// Test fixture for testing DownloadTaskImplTest class.
class DownloadNativeTaskImplTest : public PlatformTest {
 protected:
  DownloadNativeTaskImplTest() {
    // TODO(crbug.com/40189213): When removing this if condition, these
    // variables can be generated through the constructor's initializer list.
    fake_task_bridge_ =
        [[FakeNativeTaskBridge alloc] initWithDownload:fake_download_
                                              delegate:fake_delegate_];
    task_ = std::make_unique<DownloadNativeTaskImpl>(
        &web_state_, GURL(kUrl), kHttpMethod, kContentDisposition,
        /*total_bytes=*/-1, kMimeType, @(kIdentifier),
        base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::USER_BLOCKING}),
        fake_task_bridge_);
  }

  base::test::TaskEnvironment task_environment_;
  FakeWebState web_state_;
  WKDownload* fake_download_ = nil;
  id<DownloadNativeTaskBridgeDelegate> fake_delegate_ = nil;
  FakeNativeTaskBridge* fake_task_bridge_ = nil;
  std::unique_ptr<DownloadNativeTaskImpl> task_;
};

// Tests DownloadNativeTaskImpl default state after construction.
TEST_F(DownloadNativeTaskImplTest, DefaultState) {
  EXPECT_EQ(&web_state_, task_->GetWebState());
  EXPECT_EQ(DownloadTask::State::kNotStarted, task_->GetState());
  EXPECT_NSEQ(@(kIdentifier), task_->GetIdentifier());
  EXPECT_EQ(kUrl, task_->GetOriginalUrl());
  EXPECT_FALSE(task_->IsDone());
  EXPECT_EQ(0, task_->GetErrorCode());
  EXPECT_EQ(-1, task_->GetHttpCode());
  EXPECT_EQ(0, task_->GetReceivedBytes());
  EXPECT_EQ(-1, task_->GetTotalBytes());
  EXPECT_EQ(-1, task_->GetPercentComplete());
  EXPECT_EQ(kContentDisposition, task_->GetContentDisposition());
  EXPECT_EQ(kMimeType, task_->GetMimeType());
  EXPECT_EQ(kMimeType, task_->GetOriginalMimeType());
  EXPECT_EQ(base::FilePath(kTestFileName), task_->GenerateFileName());
}

TEST_F(DownloadNativeTaskImplTest, SuccessfulDownload) {
  // Simulates starting and successfully completing a download
  EXPECT_TRUE(fake_task_bridge_.calledStartDownloadBlock == NO);
  {
    web::test::WaitDownloadTaskDone observer(task_.get());
    task_->Start(base::FilePath());
    observer.Wait();
  }

  EXPECT_EQ(DownloadTask::State::kComplete, task_->GetState());
  EXPECT_TRUE(fake_task_bridge_.calledStartDownloadBlock == YES);
  EXPECT_EQ(fake_task_bridge_.progress.totalUnitCount, 100);
}

TEST_F(DownloadNativeTaskImplTest, CancelledDownload) {
  // Simulates download cancel and checks that `_startDownloadBlock` is called
  EXPECT_TRUE(fake_task_bridge_.calledStartDownloadBlock == NO);
  {
    web::test::WaitDownloadTaskDone observer(task_.get());
    task_->Cancel();
    observer.Wait();
  }

  EXPECT_EQ(DownloadTask::State::kCancelled, task_->GetState());
  EXPECT_TRUE(fake_task_bridge_.calledStartDownloadBlock == YES);
  EXPECT_EQ(fake_task_bridge_.progress.totalUnitCount, 0);
  EXPECT_TRUE(fake_task_bridge_.download == nil);
}

}  // namespace web
