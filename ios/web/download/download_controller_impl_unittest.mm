// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/download/download_controller_impl.h"

#import <memory>

#import "base/run_loop.h"
#import "base/strings/utf_string_conversions.h"
#import "ios/web/public/test/fakes/fake_download_controller_delegate.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/test/web_test.h"
#import "ios/web/test/fakes/fake_native_task_bridge.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"

namespace web {

namespace {
const char kContentDisposition[] = "attachment; filename=file.test";
const char kMimeType[] = "application/pdf";
const base::FilePath::CharType kTestFileName[] = FILE_PATH_LITERAL("file.test");
}  //  namespace

// Test fixture for testing DownloadControllerImpl class.
class DownloadControllerImplTest : public WebTest {
 protected:
  DownloadControllerImplTest() {}

  void SetUp() override {
    WebTest::SetUp();

    download_controller_ = std::make_unique<DownloadControllerImpl>();
    delegate_ = std::make_unique<FakeDownloadControllerDelegate>(
        download_controller_.get());
    web_state_.SetBrowserState(GetBrowserState());
  }

  FakeWebState web_state_;
  std::unique_ptr<DownloadControllerImpl> download_controller_;
  std::unique_ptr<FakeDownloadControllerDelegate> delegate_;
};

// Tests that DownloadController::GetDelegate returns delegate_.
TEST_F(DownloadControllerImplTest, Delegate) {
  ASSERT_EQ(delegate_.get(), download_controller_->GetDelegate());
}

// Tests that DownloadController::FromBrowserState returns the same object for
// each call.
TEST_F(DownloadControllerImplTest, FromBrowserState) {
  DownloadController* first_call_controller =
      DownloadController::FromBrowserState(GetBrowserState());
  ASSERT_TRUE(first_call_controller);
  DownloadController* second_call_controller =
      DownloadController::FromBrowserState(GetBrowserState());

  ASSERT_EQ(first_call_controller, second_call_controller);
}

// Tests that DownloadController::CreateNativeDownloadTask calls
// DownloadControllerDelegate::OnDownloadCreated.
TEST_F(DownloadControllerImplTest, OnNativeDownloadCreated) {
  NSString* identifier = [NSUUID UUID].UUIDString;
  GURL url("https://download.test");

  WKDownload* fake_download = nil;
  id<DownloadNativeTaskBridgeDelegate> fake_delegate = nil;
  FakeNativeTaskBridge* fake_task_bridge =
      [[FakeNativeTaskBridge alloc] initWithDownload:fake_download
                                            delegate:fake_delegate];

  download_controller_->CreateNativeDownloadTask(
      &web_state_, identifier, url, @"POST", kContentDisposition,
      /*total_bytes=*/-1, kMimeType, fake_task_bridge);

  ASSERT_EQ(1U, delegate_->alive_download_tasks().size());
  DownloadTask* task = delegate_->alive_download_tasks()[0].second.get();
  EXPECT_EQ(&web_state_, delegate_->alive_download_tasks()[0].first);
  EXPECT_NSEQ(identifier, task->GetIdentifier());
  EXPECT_EQ(url, task->GetOriginalUrl());
  EXPECT_NSEQ(@"POST", task->GetHttpMethod());
  EXPECT_FALSE(task->IsDone());
  EXPECT_EQ(0, task->GetErrorCode());
  EXPECT_EQ(-1, task->GetTotalBytes());
  EXPECT_EQ(-1, task->GetPercentComplete());
  EXPECT_EQ(kContentDisposition, task->GetContentDisposition());
  EXPECT_EQ(kMimeType, task->GetMimeType());
  EXPECT_EQ(base::FilePath(kTestFileName), task->GenerateFileName());
}

// Tests that DownloadController::FromBrowserState does not crash if used
// without delegate.
TEST_F(DownloadControllerImplTest, NullDelegate) {
  download_controller_->SetDelegate(nullptr);
  GURL url("https://download.test");

  WKDownload* fake_download = nil;
  id<DownloadNativeTaskBridgeDelegate> fake_delegate = nil;
  FakeNativeTaskBridge* fake_task_bridge =
      [[FakeNativeTaskBridge alloc] initWithDownload:fake_download
                                            delegate:fake_delegate];

  download_controller_->CreateNativeDownloadTask(
      &web_state_, [NSUUID UUID].UUIDString, url, @"GET", kContentDisposition,
      /*total_bytes=*/-1, kMimeType, fake_task_bridge);
}

}  // namespace web
