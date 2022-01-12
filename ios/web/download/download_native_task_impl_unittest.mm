// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/download/download_native_task_impl.h"

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/web/public/download/download_controller.h"
#import "ios/web/public/download/download_task_observer.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#include "ios/web/public/test/web_test.h"
#import "ios/web/test/fakes/fake_native_task_bridge.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

namespace {

const char kUrl[] = "chromium://download.test/";
const char kContentDisposition[] = "attachment; filename=file.test";
const char kMimeType[] = "application/pdf";
const char kIdentifier[] = "testIdentifier";
NSString* const kHttpMethod = @"POST";

class MockDownloadTaskObserver : public DownloadTaskObserver {
 public:
  MOCK_METHOD1(OnDownloadUpdated, void(DownloadTask* task));
  void OnDownloadDestroyed(DownloadTask* task) override {
    // Removing observer here works as a test that
    // DownloadTaskObserver::OnDownloadDestroyed is actually called.
    // DownloadTask DCHECKs if it is destroyed without observer removal.
    task->RemoveObserver(this);
  }
};

// Mocks DownloadTaskImpl::Delegate's OnTaskUpdated and OnTaskDestroyed
// methods and stubs DownloadTaskImpl::Delegate::CreateSession with session
// mock.
class FakeDownloadNativeTaskImplDelegate : public DownloadTaskImpl::Delegate {
 public:
  FakeDownloadNativeTaskImplDelegate() {}

  MOCK_METHOD1(OnTaskDestroyed, void(DownloadTaskImpl* task));

  // Returns mock, which can be accessed via session() method.
  NSURLSession* CreateSession(NSString* identifier,
                              NSArray<NSHTTPCookie*>* cookies,
                              id<NSURLSessionDataDelegate> delegate,
                              NSOperationQueue* delegate_queue) {
    // Make sure this method isn't called at all
    ADD_FAILURE();
    return nil;
  }
};

}  //  namespace

// Test fixture for testing DownloadTaskImplTest class.
class DownloadNativeTaskImplTest : public PlatformTest {
 protected:
  DownloadNativeTaskImplTest()
      : fake_task_bridge_([[FakeNativeTaskBridge alloc]
            initWithDownload:fake_download_
                    delegate:fake_delegate_]),
        task_(std::make_unique<DownloadNativeTaskImpl>(&web_state_,
                                                       GURL(kUrl),
                                                       kHttpMethod,
                                                       kContentDisposition,
                                                       /*total_bytes=*/-1,
                                                       kMimeType,
                                                       @(kIdentifier),
                                                       fake_task_bridge_,
                                                       &task_delegate_)) {
    task_->AddObserver(&task_observer_);
  }

  web::WebTaskEnvironment task_environment_;
  FakeBrowserState browser_state_;
  FakeWebState web_state_;
  testing::StrictMock<FakeDownloadNativeTaskImplDelegate> task_delegate_;
  WKDownload* fake_download_ API_AVAILABLE(ios(15)) = nil;
  id<DownloadNativeTaskBridgeDelegate> fake_delegate_ = nil;
  FakeNativeTaskBridge* fake_task_bridge_;
  std::unique_ptr<DownloadNativeTaskImpl> task_;
  MockDownloadTaskObserver task_observer_;
};

// Tests DownloadNativeTaskImpl default state after construction.
TEST_F(DownloadNativeTaskImplTest, DefaultState) {
  EXPECT_EQ(&web_state_, task_->GetWebState());
  EXPECT_EQ(DownloadTask::State::kNotStarted, task_->GetState());
  EXPECT_NSEQ(@(kIdentifier), task_->GetIndentifier());
  EXPECT_EQ(kUrl, task_->GetOriginalUrl());
  EXPECT_FALSE(task_->IsDone());
  EXPECT_EQ(0, task_->GetErrorCode());
  EXPECT_EQ(-1, task_->GetHttpCode());
  EXPECT_EQ(0, task_->GetReceivedBytes());
  if (@available(iOS 15, *)) {
    EXPECT_EQ(0, task_->GetTotalBytes());
    EXPECT_EQ(0, task_->GetPercentComplete());
  } else {
    EXPECT_EQ(-1, task_->GetTotalBytes());
    EXPECT_EQ(-1, task_->GetPercentComplete());
  }
  EXPECT_EQ(kContentDisposition, task_->GetContentDisposition());
  EXPECT_EQ(kMimeType, task_->GetMimeType());
  EXPECT_EQ(kMimeType, task_->GetOriginalMimeType());
  EXPECT_EQ("file.test", base::UTF16ToUTF8(task_->GetSuggestedFilename()));

  EXPECT_CALL(task_delegate_, OnTaskDestroyed(task_.get()));
}

TEST_F(DownloadNativeTaskImplTest, SuccessfulDownload) {
  // Simulates starting and successfully completing a download
  EXPECT_TRUE(fake_task_bridge_.calledStartDownloadBlock == NO);
  task_->Start(base::FilePath(), DownloadTask::Destination::kToMemory);
  EXPECT_EQ(DownloadTask::State::kInProgress, task_->GetState());
  if (@available(iOS 15, *)) {
    EXPECT_TRUE(fake_task_bridge_.calledStartDownloadBlock == YES);
    EXPECT_EQ(fake_task_bridge_.progress.totalUnitCount, 100);
  }
  EXPECT_CALL(task_delegate_, OnTaskDestroyed(task_.get()));
}

TEST_F(DownloadNativeTaskImplTest, CancelledDownload) {
  // Simulates download cancel and checks that |_startDownloadBlock| is called
  EXPECT_TRUE(fake_task_bridge_.calledStartDownloadBlock == NO);
  task_->Cancel();
  EXPECT_EQ(DownloadTask::State::kCancelled, task_->GetState());

  if (@available(iOS 15, *)) {
    EXPECT_TRUE(fake_task_bridge_.calledStartDownloadBlock == YES);
    EXPECT_EQ(fake_task_bridge_.progress.totalUnitCount, 0);
    EXPECT_TRUE(fake_task_bridge_.download == nil);
  }
  EXPECT_CALL(task_delegate_, OnTaskDestroyed(task_.get()));
}

}  // namespace web
