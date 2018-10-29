// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/download/download_task_impl.h"

#import <Foundation/Foundation.h>
#import <WebKit/WebKit.h>

#include <memory>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/web/net/cookies/wk_cookie_util.h"
#import "ios/web/public/download/download_task_observer.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "ios/web/public/test/web_test.h"
#import "ios/web/test/fakes/crw_fake_nsurl_session_task.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_fetcher_response_writer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::kWaitForCookiesTimeout;
using base::test::ios::kWaitForDownloadTimeout;
using base::test::ios::kWaitForFileOperationTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace web {

namespace {

const char kUrl[] = "chromium://download.test/";
const char kContentDisposition[] = "attachment; filename=file.test";
const char kMimeType[] = "application/pdf";

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

// Allows waiting for DownloadTaskObserver::OnDownloadUpdated callback.
class OnDownloadUpdatedWaiter : public DownloadTaskObserver {
 public:
  bool Wait() {
    return WaitUntilConditionOrTimeout(kWaitForDownloadTimeout, ^{
      base::RunLoop().RunUntilIdle();
      return download_updated_;
    });
  }

 private:
  void OnDownloadUpdated(DownloadTask* task) override {
    download_updated_ = true;
  }
  bool download_updated_ = false;
};

// Mocks DownloadTaskImpl::Delegate's OnTaskUpdated and OnTaskDestroyed methods
// and stubs DownloadTaskImpl::Delegate::CreateSession with session mock.
class FakeDownloadTaskImplDelegate : public DownloadTaskImpl::Delegate {
 public:
  FakeDownloadTaskImplDelegate()
      : configuration_([NSURLSessionConfiguration
            backgroundSessionConfigurationWithIdentifier:
                [NSUUID UUID].UUIDString]),
        session_(OCMStrictClassMock([NSURLSession class])) {
    OCMStub([session_ configuration]).andReturn(configuration_);
  }

  MOCK_METHOD1(OnTaskDestroyed, void(DownloadTaskImpl* task));

  // Returns mock, which can be accessed via session() method.
  NSURLSession* CreateSession(NSString* identifier,
                              id<NSURLSessionDataDelegate> delegate,
                              NSOperationQueue* delegate_queue) {
    // Make sure this method is called only once.
    EXPECT_FALSE(session_delegate_);
    session_delegate_ = delegate;
    return session_;
  }

  // These methods return session objects injected into DownloadTaskImpl.
  NSURLSessionConfiguration* configuration() { return configuration_; }
  id session() { return session_; }
  id<NSURLSessionDataDelegate> session_delegate() { return session_delegate_; }

 private:
  id<NSURLSessionDataDelegate> session_delegate_;
  id configuration_;
  id session_;
};

}  //  namespace

// Test fixture for testing DownloadTaskImplTest class.
class DownloadTaskImplTest : public PlatformTest {
 protected:
  DownloadTaskImplTest()
      : task_(std::make_unique<DownloadTaskImpl>(
            &web_state_,
            GURL(kUrl),
            kContentDisposition,
            /*total_bytes=*/-1,
            kMimeType,
            ui::PageTransition::PAGE_TRANSITION_TYPED,
            task_delegate_.configuration().identifier,
            &task_delegate_)),
        session_delegate_callbacks_queue_(
            dispatch_queue_create(nullptr, DISPATCH_QUEUE_SERIAL)) {
    browser_state_.SetOffTheRecord(true);
    web_state_.SetBrowserState(&browser_state_);
    task_->AddObserver(&task_observer_);
  }

  // Starts the download and return NSURLSessionDataTask fake for this task.
  CRWFakeNSURLSessionTask* Start(
      std::unique_ptr<net::URLFetcherResponseWriter> writer) {
    // Inject fake NSURLSessionDataTask into DownloadTaskImpl.
    NSURL* url = [NSURL URLWithString:@(kUrl)];
    CRWFakeNSURLSessionTask* session_task =
        [[CRWFakeNSURLSessionTask alloc] initWithURL:url];
    EXPECT_TRUE(task_delegate_.session());
    OCMExpect([task_delegate_.session() dataTaskWithURL:url])
        .andReturn(session_task);

    // Start the download.
    task_->Start(std::move(writer));
    bool success = WaitUntilConditionOrTimeout(kWaitForDownloadTimeout, ^{
      base::RunLoop().RunUntilIdle();
      return session_task.state == NSURLSessionTaskStateRunning;
    });
    return success ? session_task : nil;
  }

  // Starts the download and return NSURLSessionDataTask fake for this task.
  // Same as above, but uses URLFetcherStringWriter as response writer.
  CRWFakeNSURLSessionTask* Start() {
    return Start(std::make_unique<net::URLFetcherStringWriter>());
  }

  // Sets cookie for the test browser state.
  bool SetCookie(NSHTTPCookie* cookie) WARN_UNUSED_RESULT
      API_AVAILABLE(ios(11.0)) {
    auto store = web::WKCookieStoreForBrowserState(&browser_state_);
    __block bool cookie_was_set = false;
    [store setCookie:cookie
        completionHandler:^{
          cookie_was_set = true;
        }];
    return WaitUntilConditionOrTimeout(kWaitForCookiesTimeout, ^{
      return cookie_was_set;
    });
  }

  // Session and session delegate injected into DownloadTaskImpl for testing.
  NSURLSession* session() { return task_delegate_.session(); }
  id<NSURLSessionDataDelegate> session_delegate() {
    return task_delegate_.session_delegate();
  }

  // Updates NSURLSessionTask.countOfBytesReceived and calls
  // URLSession:dataTask:didReceiveData: callback. |data_str| is null terminated
  // C-string that represents the downloaded data.
  void SimulateDataDownload(CRWFakeNSURLSessionTask* session_task,
                            const char data_str[]) {
    OnDownloadUpdatedWaiter callback_waiter;
    task_->AddObserver(&callback_waiter);
    session_task.countOfBytesReceived += strlen(data_str);
    NSData* data = [NSData dataWithBytes:data_str length:strlen(data_str)];
    dispatch_async(session_delegate_callbacks_queue_, ^{
      [session_delegate() URLSession:session()
                            dataTask:session_task
                      didReceiveData:data];
    });
    EXPECT_TRUE(callback_waiter.Wait());
    task_->RemoveObserver(&callback_waiter);
  }

  // Sets NSURLSessionTask.state to NSURLSessionTaskStateCompleted and calls
  // URLSession:dataTask:didCompleteWithError: callback.
  void SimulateDownloadCompletion(CRWFakeNSURLSessionTask* session_task,
                                  NSError* error = nil) {
    OnDownloadUpdatedWaiter callback_waiter;
    task_->AddObserver(&callback_waiter);

    session_task.state = NSURLSessionTaskStateCompleted;
    dispatch_async(session_delegate_callbacks_queue_, ^{
      [session_delegate() URLSession:session()
                                task:session_task
                didCompleteWithError:error];
    });
    EXPECT_TRUE(callback_waiter.Wait());
    task_->RemoveObserver(&callback_waiter);
  }

  web::TestWebThreadBundle thread_bundle_;
  TestBrowserState browser_state_;
  TestWebState web_state_;
  testing::StrictMock<FakeDownloadTaskImplDelegate> task_delegate_;
  std::unique_ptr<DownloadTaskImpl> task_;
  MockDownloadTaskObserver task_observer_;
  // NSURLSessionDataDelegate callbacks are called on background serial queue.
  dispatch_queue_t session_delegate_callbacks_queue_ = 0;
};

// Tests DownloadTaskImpl default state after construction.
TEST_F(DownloadTaskImplTest, DefaultState) {
  EXPECT_EQ(DownloadTask::State::kNotStarted, task_->GetState());
  EXPECT_FALSE(task_->GetResponseWriter());
  EXPECT_NSEQ(task_delegate_.configuration().identifier,
              task_->GetIndentifier());
  EXPECT_EQ(kUrl, task_->GetOriginalUrl());
  EXPECT_FALSE(task_->IsDone());
  EXPECT_EQ(0, task_->GetErrorCode());
  EXPECT_EQ(-1, task_->GetHttpCode());
  EXPECT_EQ(-1, task_->GetTotalBytes());
  EXPECT_EQ(0, task_->GetReceivedBytes());
  EXPECT_EQ(-1, task_->GetPercentComplete());
  EXPECT_EQ(kContentDisposition, task_->GetContentDisposition());
  EXPECT_EQ(kMimeType, task_->GetMimeType());
  EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
      task_->GetTransitionType(), ui::PageTransition::PAGE_TRANSITION_TYPED));
  EXPECT_EQ("file.test", base::UTF16ToUTF8(task_->GetSuggestedFilename()));

  EXPECT_CALL(task_delegate_, OnTaskDestroyed(task_.get()));
}

// Tests sucessfull download of response without content.
// (No URLSession:dataTask:didReceiveData: callback).
TEST_F(DownloadTaskImplTest, EmptyContentDownload) {
  EXPECT_CALL(task_observer_, OnDownloadUpdated(task_.get()));
  CRWFakeNSURLSessionTask* session_task = Start();
  ASSERT_TRUE(session_task);
  testing::Mock::VerifyAndClearExpectations(&task_observer_);

  // Download has finished.
  EXPECT_CALL(task_observer_, OnDownloadUpdated(task_.get()));
  SimulateDownloadCompletion(session_task);
  testing::Mock::VerifyAndClearExpectations(&task_observer_);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForDownloadTimeout, ^{
    return task_->IsDone();
  }));
  EXPECT_EQ(DownloadTask::State::kComplete, task_->GetState());
  EXPECT_EQ(0, task_->GetErrorCode());
  EXPECT_EQ(0, task_->GetTotalBytes());
  EXPECT_EQ(0, task_->GetReceivedBytes());
  EXPECT_EQ(100, task_->GetPercentComplete());

  EXPECT_CALL(task_delegate_, OnTaskDestroyed(task_.get()));
}

// Tests sucessfull download of response when content length is unknown until
// the download completes.
TEST_F(DownloadTaskImplTest, UnknownLengthContentDownload) {
  EXPECT_CALL(task_observer_, OnDownloadUpdated(task_.get()));
  CRWFakeNSURLSessionTask* session_task = Start();
  ASSERT_TRUE(session_task);
  testing::Mock::VerifyAndClearExpectations(&task_observer_);

  // The response has arrived.
  EXPECT_CALL(task_observer_, OnDownloadUpdated(task_.get()));
  const char kData[] = "foo";
  session_task.countOfBytesExpectedToReceive = -1;
  SimulateDataDownload(session_task, kData);
  testing::Mock::VerifyAndClearExpectations(&task_observer_);
  EXPECT_EQ(DownloadTask::State::kInProgress, task_->GetState());
  EXPECT_FALSE(task_->IsDone());
  EXPECT_EQ(0, task_->GetErrorCode());
  EXPECT_EQ(-1, task_->GetTotalBytes());
  EXPECT_EQ(-1, task_->GetPercentComplete());
  EXPECT_EQ(kData, task_->GetResponseWriter()->AsStringWriter()->data());

  // Download has finished.
  EXPECT_CALL(task_observer_, OnDownloadUpdated(task_.get()));
  int64_t kDataSize = strlen(kData);
  session_task.countOfBytesExpectedToReceive = kDataSize;
  SimulateDownloadCompletion(session_task);
  testing::Mock::VerifyAndClearExpectations(&task_observer_);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForDownloadTimeout, ^{
    return task_->IsDone();
  }));
  EXPECT_EQ(DownloadTask::State::kComplete, task_->GetState());
  EXPECT_EQ(0, task_->GetErrorCode());
  EXPECT_EQ(kDataSize, task_->GetTotalBytes());
  EXPECT_EQ(100, task_->GetPercentComplete());
  EXPECT_EQ(kData, task_->GetResponseWriter()->AsStringWriter()->data());

  EXPECT_CALL(task_delegate_, OnTaskDestroyed(task_.get()));
}

// Tests cancelling the download task.
TEST_F(DownloadTaskImplTest, Cancelling) {
  EXPECT_CALL(task_observer_, OnDownloadUpdated(task_.get()));
  CRWFakeNSURLSessionTask* session_task = Start();
  ASSERT_TRUE(session_task);
  testing::Mock::VerifyAndClearExpectations(&task_observer_);

  // Cancel the download.
  EXPECT_CALL(task_observer_, OnDownloadUpdated(task_.get()));
  task_->Cancel();
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForDownloadTimeout, ^{
    return task_->IsDone();
  }));
  testing::Mock::VerifyAndClearExpectations(&task_observer_);
  EXPECT_EQ(DownloadTask::State::kCancelled, task_->GetState());

  EXPECT_CALL(task_delegate_, OnTaskDestroyed(task_.get()));
}

// Tests restarting failed download task.
TEST_F(DownloadTaskImplTest, Restarting) {
  EXPECT_CALL(task_observer_, OnDownloadUpdated(task_.get()));
  CRWFakeNSURLSessionTask* session_task = Start();
  ASSERT_TRUE(session_task);
  testing::Mock::VerifyAndClearExpectations(&task_observer_);

  // Download has failed.
  EXPECT_CALL(task_observer_, OnDownloadUpdated(task_.get()));
  NSError* error = [NSError errorWithDomain:NSURLErrorDomain
                                       code:NSURLErrorNotConnectedToInternet
                                   userInfo:nil];
  SimulateDownloadCompletion(session_task, error);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForDownloadTimeout, ^{
    return task_->IsDone();
  }));
  EXPECT_EQ(100, task_->GetPercentComplete());

  // Restart the task.
  EXPECT_CALL(task_observer_, OnDownloadUpdated(task_.get()));
  session_task = Start();
  EXPECT_EQ(0, task_->GetPercentComplete());
  ASSERT_TRUE(session_task);
  testing::Mock::VerifyAndClearExpectations(&task_observer_);

  // Download has finished.
  EXPECT_CALL(task_observer_, OnDownloadUpdated(task_.get()));
  SimulateDownloadCompletion(session_task);
  testing::Mock::VerifyAndClearExpectations(&task_observer_);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForDownloadTimeout, ^{
    return task_->IsDone();
  }));
  EXPECT_EQ(DownloadTask::State::kComplete, task_->GetState());
  EXPECT_EQ(0, task_->GetErrorCode());
  EXPECT_EQ(100, task_->GetPercentComplete());

  EXPECT_CALL(task_delegate_, OnTaskDestroyed(task_.get()));
}

// Tests sucessfull download of response with only one
// URLSession:dataTask:didReceiveData: callback.
TEST_F(DownloadTaskImplTest, SmallResponseDownload) {
  EXPECT_CALL(task_observer_, OnDownloadUpdated(task_.get()));
  CRWFakeNSURLSessionTask* session_task = Start();
  ASSERT_TRUE(session_task);
  testing::Mock::VerifyAndClearExpectations(&task_observer_);

  // The response has arrived.
  EXPECT_CALL(task_observer_, OnDownloadUpdated(task_.get()));
  const char kData[] = "foo";
  int64_t kDataSize = strlen(kData);
  session_task.countOfBytesExpectedToReceive = kDataSize;
  SimulateDataDownload(session_task, kData);
  testing::Mock::VerifyAndClearExpectations(&task_observer_);
  EXPECT_EQ(DownloadTask::State::kInProgress, task_->GetState());
  EXPECT_FALSE(task_->IsDone());
  EXPECT_EQ(0, task_->GetErrorCode());
  EXPECT_EQ(kDataSize, task_->GetTotalBytes());
  EXPECT_EQ(kDataSize, task_->GetReceivedBytes());
  EXPECT_EQ(100, task_->GetPercentComplete());
  EXPECT_EQ(kData, task_->GetResponseWriter()->AsStringWriter()->data());

  // Download has finished.
  EXPECT_CALL(task_observer_, OnDownloadUpdated(task_.get()));
  SimulateDownloadCompletion(session_task);
  testing::Mock::VerifyAndClearExpectations(&task_observer_);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForDownloadTimeout, ^{
    return task_->IsDone();
  }));
  EXPECT_EQ(DownloadTask::State::kComplete, task_->GetState());
  EXPECT_EQ(0, task_->GetErrorCode());
  EXPECT_EQ(kDataSize, task_->GetTotalBytes());
  EXPECT_EQ(kDataSize, task_->GetReceivedBytes());
  EXPECT_EQ(100, task_->GetPercentComplete());
  EXPECT_EQ(kData, task_->GetResponseWriter()->AsStringWriter()->data());

  EXPECT_CALL(task_delegate_, OnTaskDestroyed(task_.get()));
}

// Tests sucessfull download of response with multiple
// URLSession:dataTask:didReceiveData: callbacks.
TEST_F(DownloadTaskImplTest, LargeResponseDownload) {
  EXPECT_CALL(task_observer_, OnDownloadUpdated(task_.get()));
  CRWFakeNSURLSessionTask* session_task = Start();
  ASSERT_TRUE(session_task);
  testing::Mock::VerifyAndClearExpectations(&task_observer_);

  // The first part of the response has arrived.
  EXPECT_CALL(task_observer_, OnDownloadUpdated(task_.get()));
  const char kData1[] = "foo";
  const char kData2[] = "buzz";
  int64_t kData1Size = strlen(kData1);
  int64_t kData2Size = strlen(kData2);
  session_task.countOfBytesExpectedToReceive = kData1Size + kData2Size;
  SimulateDataDownload(session_task, kData1);
  testing::Mock::VerifyAndClearExpectations(&task_observer_);
  EXPECT_EQ(DownloadTask::State::kInProgress, task_->GetState());
  EXPECT_FALSE(task_->IsDone());
  EXPECT_EQ(0, task_->GetErrorCode());
  EXPECT_EQ(kData1Size + kData2Size, task_->GetTotalBytes());
  EXPECT_EQ(kData1Size, task_->GetReceivedBytes());
  EXPECT_EQ(42, task_->GetPercentComplete());
  net::URLFetcherStringWriter* writer =
      task_->GetResponseWriter()->AsStringWriter();
  EXPECT_EQ(kData1, writer->data());

  // The second part of the response has arrived.
  EXPECT_CALL(task_observer_, OnDownloadUpdated(task_.get()));
  SimulateDataDownload(session_task, kData2);
  testing::Mock::VerifyAndClearExpectations(&task_observer_);
  EXPECT_EQ(DownloadTask::State::kInProgress, task_->GetState());
  EXPECT_FALSE(task_->IsDone());
  EXPECT_EQ(0, task_->GetErrorCode());
  EXPECT_EQ(kData1Size + kData2Size, task_->GetTotalBytes());
  EXPECT_EQ(kData1Size + kData2Size, task_->GetReceivedBytes());
  EXPECT_EQ(100, task_->GetPercentComplete());
  EXPECT_EQ(std::string(kData1) + kData2, writer->data());

  // Download has finished.
  EXPECT_CALL(task_observer_, OnDownloadUpdated(task_.get()));
  SimulateDownloadCompletion(session_task);
  testing::Mock::VerifyAndClearExpectations(&task_observer_);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForDownloadTimeout, ^{
    return task_->IsDone();
  }));
  EXPECT_EQ(DownloadTask::State::kComplete, task_->GetState());
  EXPECT_EQ(0, task_->GetErrorCode());
  EXPECT_EQ(kData1Size + kData2Size, task_->GetTotalBytes());
  EXPECT_EQ(kData1Size + kData2Size, task_->GetReceivedBytes());
  EXPECT_EQ(100, task_->GetPercentComplete());
  EXPECT_EQ(std::string(kData1) + kData2, writer->data());

  EXPECT_CALL(task_delegate_, OnTaskDestroyed(task_.get()));
}

// Tests failed download when URLSession:dataTask:didReceiveData: callback was
// not even called.
TEST_F(DownloadTaskImplTest, FailureInTheBeginning) {
  EXPECT_CALL(task_observer_, OnDownloadUpdated(task_.get()));
  CRWFakeNSURLSessionTask* session_task = Start();
  ASSERT_TRUE(session_task);
  testing::Mock::VerifyAndClearExpectations(&task_observer_);

  // Download has failed.
  EXPECT_CALL(task_observer_, OnDownloadUpdated(task_.get()));
  NSError* error = [NSError errorWithDomain:NSURLErrorDomain
                                       code:NSURLErrorNotConnectedToInternet
                                   userInfo:nil];
  SimulateDownloadCompletion(session_task, error);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForDownloadTimeout, ^{
    return task_->IsDone();
  }));
  EXPECT_EQ(DownloadTask::State::kComplete, task_->GetState());
  EXPECT_TRUE(task_->GetErrorCode() == net::ERR_INTERNET_DISCONNECTED);
  EXPECT_EQ(0, task_->GetTotalBytes());
  EXPECT_EQ(0, task_->GetReceivedBytes());
  EXPECT_EQ(100, task_->GetPercentComplete());

  EXPECT_CALL(task_delegate_, OnTaskDestroyed(task_.get()));
}

// Tests failed download when URLSession:dataTask:didReceiveData: callback was
// called once.
TEST_F(DownloadTaskImplTest, FailureInTheMiddle) {
  EXPECT_CALL(task_observer_, OnDownloadUpdated(task_.get()));
  CRWFakeNSURLSessionTask* session_task = Start();
  ASSERT_TRUE(session_task);
  testing::Mock::VerifyAndClearExpectations(&task_observer_);

  // A part of the response has arrived.
  EXPECT_CALL(task_observer_, OnDownloadUpdated(task_.get()));
  const char kReceivedData[] = "foo";
  int64_t kReceivedDataSize = strlen(kReceivedData);
  int64_t kExpectedDataSize = kReceivedDataSize + 10;
  session_task.countOfBytesExpectedToReceive = kExpectedDataSize;
  SimulateDataDownload(session_task, kReceivedData);
  testing::Mock::VerifyAndClearExpectations(&task_observer_);
  EXPECT_EQ(DownloadTask::State::kInProgress, task_->GetState());
  EXPECT_FALSE(task_->IsDone());
  EXPECT_EQ(0, task_->GetErrorCode());
  EXPECT_EQ(kExpectedDataSize, task_->GetTotalBytes());
  EXPECT_EQ(kReceivedDataSize, task_->GetReceivedBytes());
  EXPECT_EQ(23, task_->GetPercentComplete());
  net::URLFetcherStringWriter* writer =
      task_->GetResponseWriter()->AsStringWriter();
  EXPECT_EQ(kReceivedData, writer->data());

  // Download has failed.
  EXPECT_CALL(task_observer_, OnDownloadUpdated(task_.get()));
  NSError* error = [NSError errorWithDomain:NSURLErrorDomain
                                       code:NSURLErrorNotConnectedToInternet
                                   userInfo:nil];
  session_task.countOfBytesExpectedToReceive = 0;  // This is 0 when offline.
  SimulateDownloadCompletion(session_task, error);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForDownloadTimeout, ^{
    return task_->IsDone();
  }));
  EXPECT_EQ(DownloadTask::State::kComplete, task_->GetState());
  EXPECT_TRUE(task_->GetErrorCode() == net::ERR_INTERNET_DISCONNECTED);
  EXPECT_EQ(kExpectedDataSize, task_->GetTotalBytes());
  EXPECT_EQ(kReceivedDataSize, task_->GetReceivedBytes());
  EXPECT_EQ(100, task_->GetPercentComplete());
  EXPECT_EQ(kReceivedData, writer->data());

  EXPECT_CALL(task_delegate_, OnTaskDestroyed(task_.get()));
}

// Tests that NSURLSessionConfiguration contains up to date cookie from browser
// state before the download started.
TEST_F(DownloadTaskImplTest, Cookie) {
  if (@available(iOS 11, *)) {
    // Remove all cookies from the session configuration.
    auto storage = task_delegate_.configuration().HTTPCookieStorage;
    for (NSHTTPCookie* cookie in storage.cookies)
      [storage deleteCookie:cookie];

    // Add a cookie to BrowserState.
    NSURL* cookie_url = [NSURL URLWithString:@(kUrl)];
    NSHTTPCookie* cookie = [NSHTTPCookie cookieWithProperties:@{
      NSHTTPCookieName : @"name",
      NSHTTPCookieValue : @"value",
      NSHTTPCookiePath : cookie_url.path,
      NSHTTPCookieDomain : cookie_url.host,
      NSHTTPCookieVersion : @1,
    }];
    ASSERT_TRUE(SetCookie(cookie));

    // Start the download and make sure that all cookie from BrowserState were
    // picked up.
    EXPECT_CALL(task_observer_, OnDownloadUpdated(task_.get()));
    ASSERT_TRUE(Start());
    EXPECT_EQ(1U, storage.cookies.count);
    EXPECT_NSEQ(cookie, storage.cookies.firstObject);
  }

  EXPECT_CALL(task_delegate_, OnTaskDestroyed(task_.get()));
}

// Tests that URLFetcherFileWriter deletes the file if download has failed with
// error.
TEST_F(DownloadTaskImplTest, FileDeletion) {
  // Create URLFetcherFileWriter.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath temp_file = temp_dir.GetPath().AppendASCII("DownloadTaskImpl");
  base::DeleteFile(temp_file, false);
  ASSERT_FALSE(base::PathExists(temp_file));
  std::unique_ptr<net::URLFetcherResponseWriter> writer =
      std::make_unique<net::URLFetcherFileWriter>(
          base::ThreadTaskRunnerHandle::Get(), temp_file);
  __block bool initialized_file_writer = false;
  ASSERT_EQ(net::ERR_IO_PENDING,
            writer->Initialize(base::BindRepeating(^(int error) {
              ASSERT_FALSE(error);
              initialized_file_writer = true;
            })));
  ASSERT_TRUE(WaitUntilConditionOrTimeout(1.0, ^{
    base::RunLoop().RunUntilIdle();
    return initialized_file_writer;
  }));

  // Start the download.
  EXPECT_CALL(task_observer_, OnDownloadUpdated(task_.get()));
  CRWFakeNSURLSessionTask* session_task = Start(std::move(writer));
  ASSERT_TRUE(session_task);

  // Deliver the response and verify that download file exists.
  EXPECT_CALL(task_observer_, OnDownloadUpdated(task_.get()));
  const char kReceivedData[] = "foo";
  SimulateDataDownload(session_task, kReceivedData);
  ASSERT_TRUE(base::PathExists(temp_file));

  // Fail the download and verify that the file was deleted.
  EXPECT_CALL(task_observer_, OnDownloadUpdated(task_.get()));
  NSError* error = [NSError errorWithDomain:NSURLErrorDomain
                                       code:NSURLErrorNotConnectedToInternet
                                   userInfo:nil];
  SimulateDownloadCompletion(session_task, error);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForDownloadTimeout, ^{
    return task_->IsDone();
  }));
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForFileOperationTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return !base::PathExists(temp_file);
  }));

  EXPECT_CALL(task_delegate_, OnTaskDestroyed(task_.get()));
}

// Tests changing MIME type during the download.
TEST_F(DownloadTaskImplTest, MimeTypeChange) {
  EXPECT_CALL(task_observer_, OnDownloadUpdated(task_.get()));
  CRWFakeNSURLSessionTask* session_task = Start();
  ASSERT_TRUE(session_task);
  testing::Mock::VerifyAndClearExpectations(&task_observer_);

  // Download has finished with a different MIME type.
  ASSERT_EQ(kMimeType, task_->GetMimeType());
  EXPECT_CALL(task_observer_, OnDownloadUpdated(task_.get()));
  const char kOtherMimeType[] = "application/foo";
  session_task.response =
      [[NSURLResponse alloc] initWithURL:[NSURL URLWithString:@(kUrl)]
                                MIMEType:@(kOtherMimeType)
                   expectedContentLength:0
                        textEncodingName:nil];
  SimulateDownloadCompletion(session_task);
  testing::Mock::VerifyAndClearExpectations(&task_observer_);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForDownloadTimeout, ^{
    return task_->IsDone();
  }));
  EXPECT_EQ(kOtherMimeType, task_->GetMimeType());

  EXPECT_CALL(task_delegate_, OnTaskDestroyed(task_.get()));
}

// Tests updating HTTP response code.
TEST_F(DownloadTaskImplTest, HttpResponseCode) {
  EXPECT_CALL(task_observer_, OnDownloadUpdated(task_.get()));
  CRWFakeNSURLSessionTask* session_task = Start();
  ASSERT_TRUE(session_task);
  testing::Mock::VerifyAndClearExpectations(&task_observer_);

  // Download has finished with a different MIME type.
  ASSERT_EQ(kMimeType, task_->GetMimeType());
  EXPECT_CALL(task_observer_, OnDownloadUpdated(task_.get()));
  int kHttpCode = 303;
  session_task.response =
      [[NSHTTPURLResponse alloc] initWithURL:[NSURL URLWithString:@(kUrl)]
                                  statusCode:303
                                 HTTPVersion:nil
                                headerFields:nil];
  SimulateDownloadCompletion(session_task);
  testing::Mock::VerifyAndClearExpectations(&task_observer_);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForDownloadTimeout, ^{
    return task_->IsDone();
  }));
  EXPECT_EQ(kHttpCode, task_->GetHttpCode());

  EXPECT_CALL(task_delegate_, OnTaskDestroyed(task_.get()));
}

// Tests that destructing DownloadTaskImpl calls -[NSURLSessionDataTask cancel]
// and OnTaskDestroyed().
TEST_F(DownloadTaskImplTest, DownloadTaskDestruction) {
  EXPECT_CALL(task_observer_, OnDownloadUpdated(task_.get()));
  CRWFakeNSURLSessionTask* session_task = Start();
  ASSERT_TRUE(session_task);
  testing::Mock::VerifyAndClearExpectations(&task_observer_);
  EXPECT_CALL(task_delegate_, OnTaskDestroyed(task_.get()));
  task_ = nullptr;  // Destruct DownloadTaskImpl.
  EXPECT_TRUE(session_task.state = NSURLSessionTaskStateCanceling);
}

// Tests that shutting down DownloadTaskImpl calls
// -[NSURLSessionDataTask cancel], but does not call OnTaskDestroyed().
TEST_F(DownloadTaskImplTest, DownloadTaskShutdown) {
  EXPECT_CALL(task_observer_, OnDownloadUpdated(task_.get()));
  CRWFakeNSURLSessionTask* session_task = Start();
  ASSERT_TRUE(session_task);
  testing::Mock::VerifyAndClearExpectations(&task_observer_);

  task_->ShutDown();
  EXPECT_TRUE(session_task.state = NSURLSessionTaskStateCanceling);
}

// Tests valid data:// url downloads.
TEST_F(DownloadTaskImplTest, ValidDataUrl) {
  // Create data:// url download task.
  char kDataUrl[] = "data:text/plain;base64,Q2hyb21pdW0=";
  auto task = std::make_unique<DownloadTaskImpl>(
      &web_state_, GURL(kDataUrl), kContentDisposition,
      /*total_bytes=*/-1, kMimeType, ui::PageTransition::PAGE_TRANSITION_TYPED,
      task_delegate_.configuration().identifier, &task_delegate_);

  // Start and wait until the download is complete.
  task->Start(std::make_unique<net::URLFetcherStringWriter>());
  DownloadTaskImpl* task_ptr = task.get();
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForDownloadTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return task_ptr->IsDone();
  }));

  // Verify the state of downloaded task.
  const char kTestData[] = "Chromium";
  EXPECT_EQ(DownloadTask::State::kComplete, task->GetState());
  EXPECT_EQ(0, task->GetErrorCode());
  EXPECT_EQ(strlen(kTestData), static_cast<size_t>(task->GetTotalBytes()));
  EXPECT_EQ(strlen(kTestData), static_cast<size_t>(task->GetReceivedBytes()));
  EXPECT_EQ(100, task->GetPercentComplete());
  EXPECT_EQ("text/plain", task->GetMimeType());
  EXPECT_EQ(kTestData, task->GetResponseWriter()->AsStringWriter()->data());

  // One OnTaskDestroyed for |task_| and one for |task|.
  EXPECT_CALL(task_delegate_, OnTaskDestroyed(task_.get()));
  EXPECT_CALL(task_delegate_, OnTaskDestroyed(task.get()));
}

// Tests empty data:// url downloads.
TEST_F(DownloadTaskImplTest, EmptyDataUrl) {
  // Create data:// url download task.
  char kDataUrl[] = "data://";
  auto task = std::make_unique<DownloadTaskImpl>(
      &web_state_, GURL(kDataUrl), kContentDisposition,
      /*total_bytes=*/-1, kMimeType, ui::PageTransition::PAGE_TRANSITION_TYPED,
      task_delegate_.configuration().identifier, &task_delegate_);

  // Start and wait until the download is complete.
  task->Start(std::make_unique<net::URLFetcherStringWriter>());
  DownloadTaskImpl* task_ptr = task.get();
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForDownloadTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return task_ptr->IsDone();
  }));

  // Verify the state of downloaded task.
  EXPECT_EQ(DownloadTask::State::kComplete, task->GetState());
  EXPECT_EQ(net::ERR_INVALID_URL, task->GetErrorCode());
  EXPECT_EQ(-1, task->GetTotalBytes());
  EXPECT_EQ(0, task->GetReceivedBytes());
  EXPECT_EQ(0, task->GetPercentComplete());
  EXPECT_EQ("", task->GetMimeType());
  EXPECT_EQ("", task->GetResponseWriter()->AsStringWriter()->data());

  // One OnTaskDestroyed for |task_| and one for |task|.
  EXPECT_CALL(task_delegate_, OnTaskDestroyed(task_.get()));
  EXPECT_CALL(task_delegate_, OnTaskDestroyed(task.get()));
}

}  // namespace web
