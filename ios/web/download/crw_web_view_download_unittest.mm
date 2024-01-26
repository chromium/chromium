// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <WebKit/WebKit.h>

#import "ios/web/download/crw_web_view_download.h"

#import "base/files/scoped_temp_dir.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "url/gurl.h"

// Test fixture for testing CRWWebViewDownloadTest class.
class CRWWebViewDownloadTest : public PlatformTest {
 protected:
  web::WebTaskEnvironment task_environment_;
};

TEST_F(CRWWebViewDownloadTest, TestDownloadHTTPFile) {
  NSURLRequest* request = [[NSURLRequest alloc]
      initWithURL:[NSURL URLWithString:@"https://example.test"]];
  id web_view = OCMStrictClassMock([WKWebView class]);
  id wk_download = OCMStrictClassMock([WKDownload class]);
  id delegate = OCMStrictProtocolMock(@protocol(CRWWebViewDownloadDelegate));
  CRWWebViewDownload* download =
      [[CRWWebViewDownload alloc] initWithPath:@"/path/foo/bar"
                                       request:request
                                       webview:web_view
                                      delegate:delegate];

  __block bool start_called = false;
  [[web_view expect]
      startDownloadUsingRequest:request
              completionHandler:[OCMArg checkWithBlock:^(void (^completion)(
                                    WKDownload* download)) {
                completion(wk_download);
                start_called = true;
                return YES;
              }]];
  [[wk_download expect] setDelegate:[OCMArg any]];
  [download startDownload];
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool() {
        return start_called;
      }));
}

TEST_F(CRWWebViewDownloadTest, TestDownloadLocalFile) {
  id web_view = OCMStrictClassMock([WKWebView class]);
  id delegate = OCMStrictProtocolMock(@protocol(CRWWebViewDownloadDelegate));

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
           webview:web_view
          delegate:delegate];
  __block bool finish_called = false;
  [[[delegate expect] andDo:^(NSInvocation* invocation) {
    finish_called = true;
  }] downloadDidFinish];
  [download startDownload];
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool() {
        return finish_called;
      }));
}
