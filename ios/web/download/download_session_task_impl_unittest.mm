// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/download/download_session_task_impl.h"

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
#include "ios/web/public/test/fakes/fake_cookie_store.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#include "ios/web/public/test/web_test.h"
#import "ios/web/test/fakes/crw_fake_nsurl_session_task.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_fetcher_response_writer.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
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
class FakeDownloadSessionTaskImplDelegate : public DownloadTaskImpl::Delegate {
 public:
  FakeDownloadSessionTaskImplDelegate()
      : configuration_([NSURLSessionConfiguration
            backgroundSessionConfigurationWithIdentifier:
                [NSUUID UUID].UUIDString]),
        session_(OCMStrictClassMock([NSURLSession class])) {
    OCMStub([session_ configuration]).andReturn(configuration_);
  }

  MOCK_METHOD1(OnTaskDestroyed, void(DownloadTaskImpl* task));

  // Returns mock, which can be accessed via session() method.
  NSURLSession* CreateSession(NSString* identifier,
                              NSArray<NSHTTPCookie*>* cookies,
                              id<NSURLSessionDataDelegate> delegate,
                              NSOperationQueue* delegate_queue) {
    // Make sure this method is called only once.
    EXPECT_FALSE(session_delegate_);
    session_delegate_ = delegate;
    cookies_ = [cookies copy];
    return session_;
  }

  // These methods return session objects injected into DownloadTaskImpl.
  NSURLSessionConfiguration* configuration() { return configuration_; }
  id session() { return session_; }
  id<NSURLSessionDataDelegate> session_delegate() { return session_delegate_; }

  // Returns the cookies passed to Create session method.
  NSArray<NSHTTPCookie*>* cookies() { return cookies_; }

 private:
  id<NSURLSessionDataDelegate> session_delegate_;
  id configuration_;
  NSArray<NSHTTPCookie*>* cookies_ = nil;
  id session_;
};

}  //  namespace

// Test fixture for testing DownloadTaskImplTest class.
class DownloadSessionTaskImplTest : public PlatformTest {
 protected:
  DownloadSessionTaskImplTest()
      : task_(std::make_unique<DownloadSessionTaskImpl>(
            &web_state_,
            GURL(kUrl),
            kHttpMethod,
            kContentDisposition,
            /*total_bytes=*/-1,
            kMimeType,
            task_delegate_.configuration().identifier,
            &task_delegate_)),
        session_delegate_callbacks_queue_(
            dispatch_queue_create(nullptr, DISPATCH_QUEUE_SERIAL)) {
    browser_state_.SetOffTheRecord(true);
    browser_state_.SetCookieStore(std::make_unique<FakeCookieStore>());
    web_state_.SetBrowserState(&browser_state_);
    task_->AddObserver(&task_observer_);
  }

  // Starts the download and return NSURLSessionDataTask fake for this task.
  CRWFakeNSURLSessionTask* Start(const base::FilePath& path,
                                 DownloadTask::Destination destination_hint) {
    // Inject fake NSURLSessionDataTask into DownloadTaskImpl.
    NSURL* url = [NSURL URLWithString:@(kUrl)];
    CRWFakeNSURLSessionTask* session_task =
        [[CRWFakeNSURLSessionTask alloc] initWithURL:url];
    EXPECT_TRUE(task_delegate_.session());
    NSMutableURLRequest* request = [NSMutableURLRequest requestWithURL:url];
    request.HTTPMethod = kHttpMethod;
    OCMExpect([task_delegate_.session() dataTaskWithRequest:request])
        .andReturn(session_task);

    // Start the download.
    task_->Start(path, destination_hint);
    bool success = WaitUntilConditionOrTimeout(kWaitForDownloadTimeout, ^{
      base::RunLoop().RunUntilIdle();
      return session_task.state == NSURLSessionTaskStateRunning;
    });
    return success ? session_task : nil;
  }

  FakeCookieStore* cookie_store() {
    auto* context = browser_state_.GetRequestContext()->GetURLRequestContext();
    // This cast is safe because we set a FakeCookieStore in the constructor.
    return static_cast<FakeCookieStore*>(context->cookie_store());
  }

  // Starts the download and return NSURLSessionDataTask fake for this task.
  // Same as above, but uses URLFetcherStringWriter as response writer.
  CRWFakeNSURLSessionTask* Start() {
    return Start(base::FilePath(), DownloadTask::Destination::kToMemory);
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

  web::WebTaskEnvironment task_environment_;
  FakeBrowserState browser_state_;
  FakeWebState web_state_;
  testing::StrictMock<FakeDownloadSessionTaskImplDelegate> task_delegate_;
  std::unique_ptr<DownloadSessionTaskImpl> task_;
  MockDownloadTaskObserver task_observer_;
  // NSURLSessionDataDelegate callbacks are called on background serial queue.
  dispatch_queue_t session_delegate_callbacks_queue_ = 0;
};

// Tests DownloadSessionTaskImpl default state after construction.
TEST_F(DownloadSessionTaskImplTest, DefaultState) {
  EXPECT_EQ(&web_state_, task_->GetWebState());
  EXPECT_EQ(DownloadTask::State::kNotStarted, task_->GetState());
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
  EXPECT_EQ(kMimeType, task_->GetOriginalMimeType());
  EXPECT_EQ("file.test", base::UTF16ToUTF8(task_->GetSuggestedFilename()));

  EXPECT_CALL(task_delegate_, OnTaskDestroyed(task_.get()));
}

// Tests sucessfull download of response without content.
// (No URLSession:dataTask:didReceiveData: callback).
TEST_F(DownloadSessionTaskImplTest, EmptyContentDownload) {
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
TEST_F(DownloadSessionTaskImplTest, UnknownLengthContentDownload) {
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
  EXPECT_NSEQ(@(kData), [[NSString alloc] initWithData:task_->GetResponseData()
                                              encoding:NSUTF8StringEncoding]);

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
  EXPECT_NSEQ(@(kData), [[NSString alloc] initWithData:task_->GetResponseData()
                                              encoding:NSUTF8StringEncoding]);

  EXPECT_CALL(task_delegate_, OnTaskDestroyed(task_.get()));
}

// Tests cancelling the download task.
TEST_F(DownloadSessionTaskImplTest, Cancelling) {
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
TEST_F(DownloadSessionTaskImplTest, Restarting) {
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
TEST_F(DownloadSessionTaskImplTest, SmallResponseDownload) {
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
  EXPECT_NSEQ(@(kData), [[NSString alloc] initWithData:task_->GetResponseData()
                                              encoding:NSUTF8StringEncoding]);

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
  EXPECT_NSEQ(@(kData), [[NSString alloc] initWithData:task_->GetResponseData()
                                              encoding:NSUTF8StringEncoding]);

  EXPECT_CALL(task_delegate_, OnTaskDestroyed(task_.get()));
}

// Tests sucessfull download of response with multiple
// URLSession:dataTask:didReceiveData: callbacks.
TEST_F(DownloadSessionTaskImplTest, LargeResponseDownload) {
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
  EXPECT_NSEQ(@(kData1), [[NSString alloc] initWithData:task_->GetResponseData()
                                               encoding:NSUTF8StringEncoding]);

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
  EXPECT_NSEQ([@(kData1) stringByAppendingString:@(kData2)],
              [[NSString alloc] initWithData:task_->GetResponseData()
                                    encoding:NSUTF8StringEncoding]);

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
  EXPECT_NSEQ([@(kData1) stringByAppendingString:@(kData2)],
              [[NSString alloc] initWithData:task_->GetResponseData()
                                    encoding:NSUTF8StringEncoding]);

  EXPECT_CALL(task_delegate_, OnTaskDestroyed(task_.get()));
}

// Tests failed download when URLSession:dataTask:didReceiveData: callback was
// not even called.
TEST_F(DownloadSessionTaskImplTest, FailureInTheBeginning) {
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
  EXPECT_EQ(DownloadTask::State::kFailed, task_->GetState());
  EXPECT_TRUE(task_->GetErrorCode() == net::ERR_INTERNET_DISCONNECTED);
  EXPECT_EQ(0, task_->GetTotalBytes());
  EXPECT_EQ(0, task_->GetReceivedBytes());
  EXPECT_EQ(100, task_->GetPercentComplete());

  EXPECT_CALL(task_delegate_, OnTaskDestroyed(task_.get()));
}

// Tests failed download when URLSession:dataTask:didReceiveData: callback was
// called once.
TEST_F(DownloadSessionTaskImplTest, FailureInTheMiddle) {
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
  EXPECT_NSEQ(@(kReceivedData),
              [[NSString alloc] initWithData:task_->GetResponseData()
                                    encoding:NSUTF8StringEncoding]);

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
  EXPECT_EQ(DownloadTask::State::kFailed, task_->GetState());
  EXPECT_TRUE(task_->GetErrorCode() == net::ERR_INTERNET_DISCONNECTED);
  EXPECT_EQ(kExpectedDataSize, task_->GetTotalBytes());
  EXPECT_EQ(kReceivedDataSize, task_->GetReceivedBytes());
  EXPECT_EQ(100, task_->GetPercentComplete());
  EXPECT_NSEQ(@(kReceivedData),
              [[NSString alloc] initWithData:task_->GetResponseData()
                                    encoding:NSUTF8StringEncoding]);

  EXPECT_CALL(task_delegate_, OnTaskDestroyed(task_.get()));
}

// Tests that CreateSession is called with the correct cookies from the cookie
// store.
TEST_F(DownloadSessionTaskImplTest, Cookie) {
  GURL cookie_url(kUrl);
  base::Time now = base::Time::Now();
  std::unique_ptr<net::CanonicalCookie> expected_cookie =
      net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "name", "value", cookie_url.host(), cookie_url.path(),
          /*creation=*/now,
          /*expire_date=*/now + base::Hours(2),
          /*last_access=*/now,
          /*secure=*/false,
          /*httponly=*/false, net::CookieSameSite::UNSPECIFIED,
          net::COOKIE_PRIORITY_DEFAULT, /*same_party=*/false);
  cookie_store()->SetAllCookies({*expected_cookie});

  // Start the download and make sure that all cookie from BrowserState were
  // picked up.
  EXPECT_CALL(task_observer_, OnDownloadUpdated(task_.get()));
  ASSERT_TRUE(Start());

  EXPECT_EQ(1U, task_delegate_.cookies().count);
  NSHTTPCookie* actual_cookie = task_delegate_.cookies().firstObject;
  EXPECT_NSEQ(@"name", actual_cookie.name);
  EXPECT_NSEQ(@"value", actual_cookie.value);

  EXPECT_CALL(task_delegate_, OnTaskDestroyed(task_.get()));
}

// Tests that URLFetcherFileWriter deletes the file if download has failed with
// error.
TEST_F(DownloadSessionTaskImplTest, FileDeletion) {
  // Create URLFetcherFileWriter.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath temp_file = temp_dir.GetPath().AppendASCII("DownloadTaskImpl");
  base::DeleteFile(temp_file);
  ASSERT_FALSE(base::PathExists(temp_file));

  // Start the download.
  EXPECT_CALL(task_observer_, OnDownloadUpdated(task_.get()));
  CRWFakeNSURLSessionTask* session_task =
      Start(temp_file, web::DownloadTask::Destination::kToDisk);
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
TEST_F(DownloadSessionTaskImplTest, MimeTypeChange) {
  EXPECT_CALL(task_observer_, OnDownloadUpdated(task_.get()));
  CRWFakeNSURLSessionTask* session_task = Start();
  ASSERT_TRUE(session_task);
  testing::Mock::VerifyAndClearExpectations(&task_observer_);

  ASSERT_EQ(kMimeType, task_->GetOriginalMimeType());
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
  EXPECT_EQ(kMimeType, task_->GetOriginalMimeType());
  EXPECT_EQ(kOtherMimeType, task_->GetMimeType());

  EXPECT_CALL(task_delegate_, OnTaskDestroyed(task_.get()));
}

// Tests updating HTTP response code.
TEST_F(DownloadSessionTaskImplTest, HttpResponseCode) {
  EXPECT_CALL(task_observer_, OnDownloadUpdated(task_.get()));
  CRWFakeNSURLSessionTask* session_task = Start();
  ASSERT_TRUE(session_task);
  testing::Mock::VerifyAndClearExpectations(&task_observer_);

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
TEST_F(DownloadSessionTaskImplTest, DownloadTaskDestruction) {
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
TEST_F(DownloadSessionTaskImplTest, DownloadTaskShutdown) {
  EXPECT_CALL(task_observer_, OnDownloadUpdated(task_.get()));
  CRWFakeNSURLSessionTask* session_task = Start();
  ASSERT_TRUE(session_task);
  testing::Mock::VerifyAndClearExpectations(&task_observer_);

  task_->ShutDown();
  EXPECT_TRUE(session_task.state = NSURLSessionTaskStateCanceling);
}

// Tests valid data:// url downloads.
TEST_F(DownloadSessionTaskImplTest, ValidDataUrl) {
  // Create data:// url download task.
  char kDataUrl[] = "data:text/plain;base64,Q2hyb21pdW0=";
  auto task = std::make_unique<DownloadSessionTaskImpl>(
      &web_state_, GURL(kDataUrl), @"GET", kContentDisposition,
      /*total_bytes=*/-1, kMimeType, task_delegate_.configuration().identifier,
      &task_delegate_);

  // Start and wait until the download is complete.
  task->Start(base::FilePath(), web::DownloadTask::Destination::kToMemory);
  DownloadSessionTaskImpl* task_ptr = task.get();
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
  EXPECT_NSEQ(@(kTestData),
              [[NSString alloc] initWithData:task->GetResponseData()
                                    encoding:NSUTF8StringEncoding]);

  // One OnTaskDestroyed for |task_| and one for |task|.
  EXPECT_CALL(task_delegate_, OnTaskDestroyed(task_.get()));
  EXPECT_CALL(task_delegate_, OnTaskDestroyed(task.get()));
}

// Tests empty data:// url downloads.
TEST_F(DownloadSessionTaskImplTest, EmptyDataUrl) {
  // Create data:// url download task.
  char kDataUrl[] = "data://";
  auto task = std::make_unique<DownloadSessionTaskImpl>(
      &web_state_, GURL(kDataUrl), @"GET", kContentDisposition,
      /*total_bytes=*/-1, kMimeType, task_delegate_.configuration().identifier,
      &task_delegate_);

  // Start and wait until the download is complete.
  task->Start(base::FilePath(), DownloadTask::Destination::kToMemory);
  DownloadSessionTaskImpl* task_ptr = task.get();
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForDownloadTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return task_ptr->IsDone();
  }));

  // Verify the state of downloaded task.
  EXPECT_EQ(DownloadTask::State::kFailed, task->GetState());
  EXPECT_EQ(net::ERR_INVALID_URL, task->GetErrorCode());
  EXPECT_EQ(-1, task->GetTotalBytes());
  EXPECT_EQ(0, task->GetReceivedBytes());
  EXPECT_EQ(0, task->GetPercentComplete());
  EXPECT_EQ("", task->GetMimeType());
  EXPECT_NSEQ(@"", [[NSString alloc] initWithData:task->GetResponseData()
                                         encoding:NSUTF8StringEncoding]);

  // One OnTaskDestroyed for |task_| and one for |task|.
  EXPECT_CALL(task_delegate_, OnTaskDestroyed(task_.get()));
  EXPECT_CALL(task_delegate_, OnTaskDestroyed(task.get()));
}

}  // namespace web
