// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <WebKit/WebKit.h>

#import <string>
#import <string_view>

#import "base/apple/foundation_util.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/files/scoped_temp_dir.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "base/task/bind_post_task.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/test_future.h"
#import "ios/testing/embedded_test_server_handlers.h"
#import "ios/web/download/crw_web_view_download.h"
#import "ios/web/public/download/crw_web_view_download.h"
#import "ios/web/test/web_test_with_web_controller.h"
#import "ios/web/web_state/ui/crw_web_controller.h"
#import "net/base/apple/url_conversions.h"
#import "net/test/embedded_test_server/default_handlers.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/request_handler_util.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"

namespace {

constexpr std::string_view kDownloadPath = "/download";
constexpr std::string_view kSlowDownloadPath = "/slow?100";
// `testing::HandleDownload` responds with 1 null byte when no length query is
// provided.
constexpr std::string_view kExpectedRemoteDownloadContent("\0", 1);
constexpr std::string_view kTestContent = "test download content";
constexpr std::string_view kSourceFileName = "source.bin";
constexpr std::string_view kDestinationFileName = "destination.bin";
constexpr std::string_view kMissingFileName = "missing.bin";

}  // namespace

// Helper delegate forwarding `CRWWebViewDownloadDelegate` callbacks to C++
// test futures.
@interface FakeWebViewDownloadDelegate : NSObject <CRWWebViewDownloadDelegate>

// Initializes the delegate with finish and failure callbacks.
- (instancetype)initWithFinishCallback:(base::OnceClosure)finishCallback
                       failureCallback:
                           (base::OnceCallback<void(NSError*)>)failureCallback;

@end

@implementation FakeWebViewDownloadDelegate {
  base::OnceClosure _finishCallback;
  base::OnceCallback<void(NSError*)> _failureCallback;
}

- (instancetype)initWithFinishCallback:(base::OnceClosure)finishCallback
                       failureCallback:
                           (base::OnceCallback<void(NSError*)>)failureCallback {
  self = [super init];
  if (self) {
    _finishCallback = std::move(finishCallback);
    _failureCallback = std::move(failureCallback);
  }
  return self;
}

#pragma mark - CRWWebViewDownloadDelegate

- (void)downloadDidFinish {
  if (_finishCallback) {
    std::move(_finishCallback).Run();
  }
}

- (void)downloadDidFailWithError:(NSError*)error {
  if (_failureCallback) {
    std::move(_failureCallback).Run(error);
  }
}

@end

namespace web {

// Integration test fixture for `CRWWebViewDownload`.
class CRWWebViewDownloadInttest : public WebTestWithWebController {
 protected:
  void SetUp() override {
    WebTestWithWebController::SetUp();
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    web_view_ = [web_controller() ensureWebViewCreated];
    server_.RegisterRequestHandler(base::BindRepeating(
        &net::test_server::HandlePrefixedRequest, std::string(kDownloadPath),
        base::BindRepeating(&testing::HandleDownload)));
    net::test_server::RegisterDefaultHandlers(&server_);
  }

  base::FilePath GetDestinationPath() const {
    return temp_dir_.GetPath().Append(kDestinationFileName);
  }

  base::ScopedTempDir temp_dir_;
  WKWebView* web_view_ = nil;
  net::EmbeddedTestServer server_;
};

// Tests that downloading an HTTP URL writes the file and records completion
// metrics.
TEST_F(CRWWebViewDownloadInttest, RemoteDownloadSuccess) {
  ASSERT_TRUE(server_.Start());

  base::HistogramTester histogram_tester;
  base::test::TestFuture<void> finish_future;
  base::test::TestFuture<NSError*> failure_future;
  FakeWebViewDownloadDelegate* delegate = [[FakeWebViewDownloadDelegate alloc]
      initWithFinishCallback:finish_future.GetCallback()
             failureCallback:failure_future.GetCallback()];

  NSURL* url = net::NSURLWithGURL(server_.GetURL(kDownloadPath));
  NSURLRequest* request = [NSURLRequest requestWithURL:url];
  base::FilePath destination_path = GetDestinationPath();

  CRWWebViewDownload* download = [[CRWWebViewDownload alloc]
      initWithPath:base::apple::FilePathToNSString(destination_path)
           request:request
           webview:web_view_
          delegate:delegate];

  [download startDownload];
  ASSERT_TRUE(finish_future.Wait());
  EXPECT_FALSE(failure_future.IsReady());

  // Calling `cancelDownload:` after completion (as `SharingCoordinator -stop`
  // does) must not record a second outcome.
  base::test::TestFuture<void> cancel_future;
  [download cancelDownload:base::CallbackToBlock(cancel_future.GetCallback())];
  ASSERT_TRUE(cancel_future.Wait());

  std::string content;
  ASSERT_TRUE(base::ReadFileToString(destination_path, &content));
  EXPECT_EQ(kExpectedRemoteDownloadContent, content);

  histogram_tester.ExpectUniqueSample(kIOSDownloadWebViewResultHistogram,
                                      IOSDownloadWebViewResult::kCompleted, 1);
  histogram_tester.ExpectTotalCount(kIOSDownloadWebViewDurationHistogram, 1);
  histogram_tester.ExpectTotalCount(kIOSDownloadWebViewLocalSucceededHistogram,
                                    0);
}

// Tests that a failing HTTP download notifies the delegate and records
// `kError`.
TEST_F(CRWWebViewDownloadInttest, RemoteDownloadError) {
  ASSERT_TRUE(server_.Start());
  NSURL* url = net::NSURLWithGURL(server_.GetURL(kDownloadPath));
  ASSERT_TRUE(server_.ShutdownAndWaitUntilComplete());

  base::HistogramTester histogram_tester;
  base::test::TestFuture<void> finish_future;
  base::test::TestFuture<NSError*> failure_future;
  FakeWebViewDownloadDelegate* delegate = [[FakeWebViewDownloadDelegate alloc]
      initWithFinishCallback:finish_future.GetCallback()
             failureCallback:failure_future.GetCallback()];

  NSURLRequest* request = [NSURLRequest requestWithURL:url];
  CRWWebViewDownload* download = [[CRWWebViewDownload alloc]
      initWithPath:base::apple::FilePathToNSString(GetDestinationPath())
           request:request
           webview:web_view_
          delegate:delegate];

  [download startDownload];
  ASSERT_TRUE(failure_future.Wait());
  EXPECT_FALSE(finish_future.IsReady());

  // Calling `cancelDownload:` after failure (as `SharingCoordinator -stop`
  // does) must not record a second outcome.
  base::test::TestFuture<void> cancel_future;
  [download cancelDownload:base::CallbackToBlock(cancel_future.GetCallback())];
  ASSERT_TRUE(cancel_future.Wait());

  histogram_tester.ExpectUniqueSample(kIOSDownloadWebViewResultHistogram,
                                      IOSDownloadWebViewResult::kError, 1);
  histogram_tester.ExpectTotalCount(kIOSDownloadWebViewDurationHistogram, 0);
  histogram_tester.ExpectTotalCount(kIOSDownloadWebViewLocalSucceededHistogram,
                                    0);
}

// Tests that canceling an in-flight HTTP download records `kCanceled` and no
// duration metric.
TEST_F(CRWWebViewDownloadInttest, RemoteDownloadCanceled) {
  base::test::TestFuture<void> request_started;
  server_.RegisterRequestMonitor(base::BindPostTaskToCurrentDefault(
      base::IgnoreArgs<const net::test_server::HttpRequest&>(
          request_started.GetRepeatingCallback())));
  ASSERT_TRUE(server_.Start());

  base::HistogramTester histogram_tester;
  base::test::TestFuture<void> finish_future;
  base::test::TestFuture<NSError*> failure_future;
  FakeWebViewDownloadDelegate* delegate = [[FakeWebViewDownloadDelegate alloc]
      initWithFinishCallback:finish_future.GetCallback()
             failureCallback:failure_future.GetCallback()];

  NSURL* url = net::NSURLWithGURL(server_.GetURL(kSlowDownloadPath));
  NSURLRequest* request = [NSURLRequest requestWithURL:url];
  CRWWebViewDownload* download = [[CRWWebViewDownload alloc]
      initWithPath:base::apple::FilePathToNSString(GetDestinationPath())
           request:request
           webview:web_view_
          delegate:delegate];

  [download startDownload];
  ASSERT_TRUE(request_started.Wait());

  base::test::TestFuture<void> cancel_future;
  [download cancelDownload:base::CallbackToBlock(cancel_future.GetCallback())];
  ASSERT_TRUE(cancel_future.Wait());

  // Calling `cancelDownload:` a second time (as `SharingCoordinator -stop`
  // does after cancellation) must not record a duplicate `kCanceled` sample.
  base::test::TestFuture<void> second_cancel_future;
  [download
      cancelDownload:base::CallbackToBlock(second_cancel_future.GetCallback())];
  ASSERT_TRUE(second_cancel_future.Wait());

  EXPECT_FALSE(finish_future.IsReady());
  EXPECT_FALSE(failure_future.IsReady());

  histogram_tester.ExpectUniqueSample(kIOSDownloadWebViewResultHistogram,
                                      IOSDownloadWebViewResult::kCanceled, 1);
  histogram_tester.ExpectTotalCount(kIOSDownloadWebViewDurationHistogram, 0);
  histogram_tester.ExpectTotalCount(kIOSDownloadWebViewLocalSucceededHistogram,
                                    0);
}

// Tests that downloading a local `file://` URL copies the file and records
// local download success.
TEST_F(CRWWebViewDownloadInttest, LocalDownloadSuccess) {
  base::FilePath source_path = temp_dir_.GetPath().Append(kSourceFileName);
  ASSERT_TRUE(base::WriteFile(source_path, kTestContent));

  base::HistogramTester histogram_tester;
  base::test::TestFuture<void> finish_future;
  base::test::TestFuture<NSError*> failure_future;
  FakeWebViewDownloadDelegate* delegate = [[FakeWebViewDownloadDelegate alloc]
      initWithFinishCallback:finish_future.GetCallback()
             failureCallback:failure_future.GetCallback()];

  NSURL* source_url = base::apple::FilePathToNSURL(source_path);
  NSURLRequest* request = [NSURLRequest requestWithURL:source_url];
  base::FilePath destination_path = GetDestinationPath();

  CRWWebViewDownload* download = [[CRWWebViewDownload alloc]
      initWithPath:base::apple::FilePathToNSString(destination_path)
           request:request
           webview:web_view_
          delegate:delegate];

  [download startDownload];
  ASSERT_TRUE(finish_future.Wait());
  EXPECT_FALSE(failure_future.IsReady());

  std::string content;
  ASSERT_TRUE(base::ReadFileToString(destination_path, &content));
  EXPECT_EQ(kTestContent, content);

  histogram_tester.ExpectUniqueSample(
      kIOSDownloadWebViewLocalSucceededHistogram, true, 1);
  histogram_tester.ExpectTotalCount(kIOSDownloadWebViewResultHistogram, 0);
  histogram_tester.ExpectTotalCount(kIOSDownloadWebViewDurationHistogram, 0);
}

// Tests that failing to copy a missing local `file://` URL notifies the
// delegate and records local download failure.
TEST_F(CRWWebViewDownloadInttest, LocalDownloadFileIOError) {
  base::FilePath missing_path = temp_dir_.GetPath().Append(kMissingFileName);

  base::HistogramTester histogram_tester;
  base::test::TestFuture<void> finish_future;
  base::test::TestFuture<NSError*> failure_future;
  FakeWebViewDownloadDelegate* delegate = [[FakeWebViewDownloadDelegate alloc]
      initWithFinishCallback:finish_future.GetCallback()
             failureCallback:failure_future.GetCallback()];

  NSURL* missing_url = base::apple::FilePathToNSURL(missing_path);
  NSURLRequest* request = [NSURLRequest requestWithURL:missing_url];
  CRWWebViewDownload* download = [[CRWWebViewDownload alloc]
      initWithPath:base::apple::FilePathToNSString(GetDestinationPath())
           request:request
           webview:web_view_
          delegate:delegate];

  [download startDownload];
  ASSERT_TRUE(failure_future.Wait());
  EXPECT_FALSE(finish_future.IsReady());

  histogram_tester.ExpectUniqueSample(
      kIOSDownloadWebViewLocalSucceededHistogram, false, 1);
  histogram_tester.ExpectTotalCount(kIOSDownloadWebViewResultHistogram, 0);
  histogram_tester.ExpectTotalCount(kIOSDownloadWebViewDurationHistogram, 0);
}

// Tests that calling `cancelDownload:` on a local `file://` download invokes
// the completion callback without recording `WKDownload` metrics.
TEST_F(CRWWebViewDownloadInttest, LocalDownloadCanceled) {
  base::FilePath source_path = temp_dir_.GetPath().Append(kSourceFileName);
  ASSERT_TRUE(base::WriteFile(source_path, kTestContent));

  base::HistogramTester histogram_tester;
  base::test::TestFuture<void> finish_future;
  base::test::TestFuture<NSError*> failure_future;
  FakeWebViewDownloadDelegate* delegate = [[FakeWebViewDownloadDelegate alloc]
      initWithFinishCallback:finish_future.GetCallback()
             failureCallback:failure_future.GetCallback()];

  NSURL* source_url = base::apple::FilePathToNSURL(source_path);
  NSURLRequest* request = [NSURLRequest requestWithURL:source_url];
  CRWWebViewDownload* download = [[CRWWebViewDownload alloc]
      initWithPath:base::apple::FilePathToNSString(GetDestinationPath())
           request:request
           webview:web_view_
          delegate:delegate];

  [download startDownload];

  base::test::TestFuture<void> cancel_future;
  [download cancelDownload:base::CallbackToBlock(cancel_future.GetCallback())];
  ASSERT_TRUE(cancel_future.Wait());
  ASSERT_TRUE(finish_future.Wait());

  histogram_tester.ExpectTotalCount(kIOSDownloadWebViewResultHistogram, 0);
  histogram_tester.ExpectTotalCount(kIOSDownloadWebViewDurationHistogram, 0);
}

}  // namespace web
