// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/download/download_task_impl.h"

#import <Foundation/Foundation.h>
#import <WebKit/WebKit.h>

#import <memory>

#import "base/task/sequenced_task_runner.h"
#import "base/task/task_traits.h"
#import "base/task/thread_pool.h"
#import "base/test/task_environment.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

namespace web {

namespace {

const char kUrl[] = "chromium://download.test/";
const char kContentDisposition[] = "attachment; filename=file.test";
const char kMimeType[] = "application/pdf";
const base::FilePath::CharType kTestFileName[] = FILE_PATH_LITERAL("file.test");
NSString* const kHttpMethod = @"POST";

}  //  namespace

// Creates a non-virtual class to use for testing
class FakeDownloadTaskImpl final : public DownloadTaskImpl {
 public:
  FakeDownloadTaskImpl(
      WebState* web_state,
      const GURL& original_url,
      NSString* http_method,
      const std::string& content_disposition,
      int64_t total_bytes,
      const std::string& mime_type,
      NSString* identifier,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner)
      : DownloadTaskImpl(web_state,
                         original_url,
                         http_method,
                         content_disposition,
                         total_bytes,
                         mime_type,
                         identifier,
                         task_runner) {}

  void StartInternal(const base::FilePath& path) final {}
  void CancelInternal() final {}
};

// Test fixture for testing DownloadTaskImplTest class.
class DownloadTaskImplTest : public PlatformTest {
 protected:
  DownloadTaskImplTest()
      : task_(std::make_unique<FakeDownloadTaskImpl>(
            &web_state_,
            GURL(kUrl),
            kHttpMethod,
            kContentDisposition,
            /*total_bytes=*/-1,
            kMimeType,
            [[NSUUID UUID] UUIDString],
            base::ThreadPool::CreateSequencedTaskRunner(
                {base::MayBlock(), base::TaskPriority::USER_BLOCKING}))) {}

  base::test::TaskEnvironment task_environment_;
  FakeWebState web_state_;
  std::unique_ptr<FakeDownloadTaskImpl> task_;
};

// Tests DownloadTaskImpl default state after construction.
TEST_F(DownloadTaskImplTest, DefaultState) {
  EXPECT_EQ(&web_state_, task_->GetWebState());
  EXPECT_EQ(DownloadTask::State::kNotStarted, task_->GetState());
  EXPECT_NSNE(@"", task_->GetIdentifier());
  EXPECT_EQ(kUrl, task_->GetOriginalUrl());
  EXPECT_FALSE(task_->IsDone());
  EXPECT_EQ(0, task_->GetErrorCode());
  EXPECT_EQ(-1, task_->GetHttpCode());
  EXPECT_EQ(-1, task_->GetTotalBytes());
  EXPECT_EQ(0, task_->GetReceivedBytes());
  EXPECT_EQ(-1, task_->GetPercentComplete());
  EXPECT_EQ(kContentDisposition, task_->GetContentDisposition());
  EXPECT_EQ(kMimeType, task_->GetMimeType());
  EXPECT_EQ(kMimeType, task_->GetOriginalMimeType());
  EXPECT_EQ(base::FilePath(kTestFileName), task_->GenerateFileName());
}

// Tests that DownloadTaskImpl methods are overloaded
TEST_F(DownloadTaskImplTest, SuccessfulInitialization) {
  // Simulates successful download and tests that Start() and
  // OnDownloadFinished are overloaded correctly
  task_->Start(base::FilePath());
  EXPECT_EQ(DownloadTask::State::kInProgress, task_->GetState());

  // Tests that Cancel() is overloaded
  task_->Cancel();
  EXPECT_EQ(DownloadTask::State::kCancelled, task_->GetState());
}
}  // namespace web
