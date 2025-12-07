// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/download/crw_web_view_download.h"

#import <WebKit/WebKit.h>

#import "base/files/scoped_temp_dir.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "url/gurl.h"

// Test fixture for testing CRWWebViewDownloadTest class.
class CRWWebViewDownloadTest : public PlatformTest {
  void TearDown() override {
    EXPECT_OCMOCK_VERIFY((id)web_view_);
    EXPECT_OCMOCK_VERIFY((id)wk_download_);
    EXPECT_OCMOCK_VERIFY((id)delegate_);
    PlatformTest::TearDown();
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  WKWebView* web_view_ = OCMStrictClassMock([WKWebView class]);
  WKDownload* wk_download_ = OCMStrictClassMock([WKDownload class]);
  id<CRWWebViewDownloadDelegate> delegate_ =
      OCMStrictProtocolMock(@protocol(CRWWebViewDownloadDelegate));
};

TEST_F(CRWWebViewDownloadTest, TestDownloadHTTPFile) {
  NSURLRequest* request = [[NSURLRequest alloc]
      initWithURL:[NSURL URLWithString:@"https://example.test"]];
  CRWWebViewDownload* download =
      [[CRWWebViewDownload alloc] initWithPath:@"/path/foo/bar"
                                       request:request
                                       webview:web_view_
                                      delegate:delegate_];

  __block bool start_called = false;
  OCMExpect([web_view_
      startDownloadUsingRequest:request
              completionHandler:[OCMArg checkWithBlock:^(void (^completion)(
                                    WKDownload* download)) {
                completion(wk_download_);
                start_called = true;
                return YES;
              }]]);
  OCMExpect([wk_download_ setDelegate:[OCMArg any]]);
  [download startDownload];
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool() {
        return start_called;
      }));
}

TEST_F(CRWWebViewDownloadTest, TestDownloadLocalFile) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());

  const base::FilePath root = scoped_temp_dir.GetPath();

  const base::FilePath from = root.Append("from");
  const base::FilePath dest = root.Append("to");

  // Create a file in a sub-directory.
  NSData* data = [@"data" dataUsingEncoding:NSUTF8StringEncoding];

  EXPECT_TRUE([data writeToFile:base::SysUTF8ToNSString(from.value())
                     atomically:YES]);
  NSURLRequest* request = [[NSURLRequest alloc]
      initWithURL:[NSURL
                      fileURLWithPath:base::SysUTF8ToNSString(from.value())]];

  CRWWebViewDownload* download = [[CRWWebViewDownload alloc]
      initWithPath:base::SysUTF8ToNSString(dest.value())
           request:request
           webview:web_view_
          delegate:delegate_];
  __block bool finish_called = false;
  OCMExpect([delegate_ downloadDidFinish]).andDo(^(NSInvocation* invocation) {
    finish_called = true;
  });
  [download startDownload];
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool() {
        return finish_called;
      }));
}
