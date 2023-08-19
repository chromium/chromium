// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/cwv_download_task_internal.h"

#import <Foundation/Foundation.h>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "base/test/task_environment.h"
#import "ios/web/public/test/fakes/fake_download_task.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForFileOperationTimeout;

namespace ios_web_view {

class CWVDownloadTaskTest : public PlatformTest {
 public:
  CWVDownloadTaskTest()
      : valid_local_file_path_(testing::TempDir() + "/foo.txt") {
    auto task_ptr = std::make_unique<web::FakeDownloadTask>(
        GURL("http://example.com/foo.txt"), "text/plain");
    fake_internal_task_ = task_ptr.get();
    cwv_task_ =
        [[CWVDownloadTask alloc] initWithInternalTask:std::move(task_ptr)];
    mock_delegate_ = OCMProtocolMock(@protocol(CWVDownloadTaskDelegate));
    cwv_task_.delegate = mock_delegate_;
  }

 protected:
  std::string valid_local_file_path_;
  base::test::TaskEnvironment task_environment_;
  web::FakeDownloadTask* fake_internal_task_ = nullptr;
  id<CWVDownloadTaskDelegate> mock_delegate_ = nil;
  CWVDownloadTask* cwv_task_ = nil;

  // Waits until fake_internal_task_->Start() is called.
  [[nodiscard]] bool WaitUntilTaskStarts() {
    return WaitUntilConditionOrTimeout(kWaitForFileOperationTimeout, ^{
      task_environment_.RunUntilIdle();
      return fake_internal_task_->GetState() ==
             web::DownloadTask::State::kInProgress;
    });
  }
};

// Tests a flow where the download starts and finishes successfully.
TEST_F(CWVDownloadTaskTest, SuccessfulFlow) {
  OCMExpect([mock_delegate_ downloadTaskProgressDidChange:cwv_task_]);
  [cwv_task_ startDownloadToLocalFileAtPath:base::SysUTF8ToNSString(
                                                valid_local_file_path_)];
  ASSERT_TRUE(WaitUntilTaskStarts());
  EXPECT_OCMOCK_VERIFY((id)mock_delegate_);

  OCMExpect([mock_delegate_ downloadTaskProgressDidChange:cwv_task_]);
  fake_internal_task_->SetPercentComplete(50);
  EXPECT_OCMOCK_VERIFY((id)mock_delegate_);

  OCMExpect([mock_delegate_ downloadTask:cwv_task_ didFinishWithError:nil]);
  fake_internal_task_->SetDone(true);
  EXPECT_OCMOCK_VERIFY((id)mock_delegate_);
}

// Tests a flow where the download finishes with an error.
TEST_F(CWVDownloadTaskTest, FailedFlow) {
  [cwv_task_ startDownloadToLocalFileAtPath:base::SysUTF8ToNSString(
                                                valid_local_file_path_)];
  ASSERT_TRUE(WaitUntilTaskStarts());

  OCMExpect([mock_delegate_
            downloadTask:cwv_task_
      didFinishWithError:[OCMArg checkWithBlock:^(NSError* error) {
        return error.code == CWVDownloadErrorFailed;
      }]]);
  fake_internal_task_->SetErrorCode(net::ERR_FAILED);
  fake_internal_task_->SetDone(true);
  EXPECT_OCMOCK_VERIFY((id)mock_delegate_);
}

// Tests a flow where the download is cancelled.
TEST_F(CWVDownloadTaskTest, CancelledFlow) {
  [cwv_task_ startDownloadToLocalFileAtPath:base::SysUTF8ToNSString(
                                                valid_local_file_path_)];
  ASSERT_TRUE(WaitUntilTaskStarts());

  OCMExpect([mock_delegate_
            downloadTask:cwv_task_
      didFinishWithError:[OCMArg checkWithBlock:^(NSError* error) {
        return error.code == CWVDownloadErrorAborted;
      }]]);

  [cwv_task_ cancel];
  EXPECT_EQ(web::DownloadTask::State::kCancelled,
            fake_internal_task_->GetState());

  // Simulate behavior of a real web::DownloadTask which transition to state
  // kComplete with error code net::ERR_ABORTED when cancelled.
  fake_internal_task_->SetErrorCode(net::ERR_ABORTED);
  fake_internal_task_->SetDone(true);

  EXPECT_OCMOCK_VERIFY((id)mock_delegate_);
}

// Tests a case when it fails to write to the specified local file path.
TEST_F(CWVDownloadTaskTest, WriteFailure) {
  __block bool did_finish_called = false;
  OCMStub([mock_delegate_ downloadTask:cwv_task_
                    didFinishWithError:[OCMArg isNotNil]])
      .andDo(^(NSInvocation*) {
        did_finish_called = true;
      });

  NSString* path =
      base::SysUTF8ToNSString(testing::TempDir() + "/non_existent_dir/foo.txt");
  [cwv_task_ startDownloadToLocalFileAtPath:path];
  // Simulate behavior of a real web::DownloadTask which transitions to state
  // net::ERR_ABORTED when a nonexistent directory is used as the path to write
  // to
  fake_internal_task_->SetErrorCode(net::ERR_ABORTED);
  fake_internal_task_->SetDone(true);

  EXPECT_TRUE(did_finish_called);
}

// Tests properties of CWVDownloadTask.
TEST_F(CWVDownloadTaskTest, Properties) {
  // Specified in the constructor.
  EXPECT_NSEQ([NSURL URLWithString:@"http://example.com/foo.txt"],
              cwv_task_.originalURL);
  EXPECT_NSEQ(@"text/plain", cwv_task_.MIMEType);

  fake_internal_task_->SetGeneratedFileName(
      base::FilePath(FILE_PATH_LITERAL("foo.txt")));
  EXPECT_NSEQ(@"foo.txt", cwv_task_.suggestedFileName);

  fake_internal_task_->SetTotalBytes(1024);
  EXPECT_EQ(1024, cwv_task_.totalBytes);

  fake_internal_task_->SetTotalBytes(-1);  // Unknown
  EXPECT_EQ(CWVDownloadSizeUnknown, cwv_task_.totalBytes);

  fake_internal_task_->SetReceivedBytes(512);
  EXPECT_EQ(512, cwv_task_.receivedBytes);

  fake_internal_task_->SetPercentComplete(50);
  EXPECT_FLOAT_EQ(0.5, cwv_task_.progress);

  fake_internal_task_->SetPercentComplete(-1);  // Unknown
  EXPECT_TRUE(isnan(cwv_task_.progress));
}

}  // namespace ios_web_view
