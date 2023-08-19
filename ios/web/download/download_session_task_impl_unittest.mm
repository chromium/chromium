// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/download/download_session_task_impl.h"

#import <Foundation/Foundation.h>
#import <WebKit/WebKit.h>

#import <memory>

#import "base/files/file_util.h"
#import "base/files/scoped_temp_dir.h"
#import "base/functional/bind.h"
#import "base/task/task_traits.h"
#import "base/task/thread_pool.h"
#import "ios/web/net/cookies/wk_cookie_util.h"
#import "ios/web/public/test/download_task_test_util.h"
#import "ios/web/public/test/fakes/fake_browser_state.h"
#import "ios/web/public/test/fakes/fake_cookie_store.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/test/fakes/crw_fake_nsurl_session_task.h"
#import "net/base/net_errors.h"
#import "net/url_request/url_request_context.h"
#import "net/url_request/url_request_context_getter.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "url/gurl.h"

namespace web {

namespace {

const char kUrl[] = "chromium://download.test/";
const char kContentDisposition[] = "attachment; filename=file.test";
const char kMimeType[] = "application/pdf";
const base::FilePath::CharType kTestFileName[] = FILE_PATH_LITERAL("file.test");
NSString* const kHttpMethod = @"POST";

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
            [[NSUUID UUID] UUIDString],
            base::ThreadPool::CreateSequencedTaskRunner(
                {base::MayBlock(), base::TaskPriority::USER_BLOCKING}),
            base::BindRepeating(&DownloadSessionTaskImplTest::CreateSession,
                                base::Unretained(this)))),
        session_delegate_callbacks_queue_(
            dispatch_queue_create(nullptr, DISPATCH_QUEUE_SERIAL)) {
    browser_state_.SetOffTheRecord(true);
    browser_state_.SetCookieStore(std::make_unique<FakeCookieStore>());
    web_state_.SetBrowserState(&browser_state_);
  }

  // Starts the download and return NSURLSessionDataTask fake for this task.
  CRWFakeNSURLSessionTask* Start(const base::FilePath& path) {
    web::test::WaitDownloadTaskUpdated observer(task_.get());
    task_->Start(path);
    observer.Wait();

    DCHECK(session_task_);
    return session_task_;
  }

  FakeCookieStore* cookie_store() {
    auto* context = browser_state_.GetRequestContext()->GetURLRequestContext();
    // This cast is safe because we set a FakeCookieStore in the constructor.
    return static_cast<FakeCookieStore*>(context->cookie_store());
  }

  // Starts the download and return NSURLSessionDataTask fake for this task.
  // Same as above, but uses URLFetcherStringWriter as response writer.
  CRWFakeNSURLSessionTask* Start() { return Start(base::FilePath()); }

  // Session and session delegate injected into DownloadTaskImpl for testing.
  NSURLSession* session() { return session_; }
  id<NSURLSessionDataDelegate> session_delegate() { return session_delegate_; }
  NSURLSessionConfiguration* session_configuration() {
    return session_configuration_;
  }

  // Updates NSURLSessionTask.countOfBytesReceived and calls
  // URLSession:dataTask:didReceiveData: callback. `data_str` is null terminated
  // C-string that represents the downloaded data.
  void SimulateDataDownload(CRWFakeNSURLSessionTask* session_task,
                            const char data_str[]) {
    web::test::WaitDownloadTaskUpdated observer(task_.get());

    session_task.countOfBytesReceived += strlen(data_str);
    NSData* data = [NSData dataWithBytes:data_str length:strlen(data_str)];
    dispatch_async(session_delegate_callbacks_queue_, ^{
      [session_delegate() URLSession:session()
                            dataTask:session_task
                      didReceiveData:data];
    });

    observer.Wait();
  }

  // Sets NSURLSessionTask.state to NSURLSessionTaskStateCompleted and calls
  // URLSession:dataTask:didCompleteWithError: callback.
  void SimulateDownloadCompletion(CRWFakeNSURLSessionTask* session_task,
                                  NSError* error = nil) {
    web::test::WaitDownloadTaskUpdated observer(task_.get());

    session_task.state = NSURLSessionTaskStateCompleted;
    dispatch_async(session_delegate_callbacks_queue_, ^{
      [session_delegate() URLSession:session()
                                task:session_task
                didCompleteWithError:error];
    });

    observer.Wait();
  }

  NSURLSession* CreateSession(NSURLSessionConfiguration* configuration,
                              id<NSURLSessionDataDelegate> delegate) {
    session_ = OCMStrictClassMock([NSURLSession class]);

    // Inject fake NSURLSessionDataTask into DownloadTaskImpl.
    NSURL* url = [NSURL URLWithString:@(kUrl)];
    session_task_ = [[CRWFakeNSURLSessionTask alloc] initWithURL:url];
    NSMutableURLRequest* request = [NSMutableURLRequest requestWithURL:url];
    request.HTTPMethod = kHttpMethod;
    OCMExpect([session_ dataTaskWithRequest:request]).andReturn(session_task_);

    session_configuration_ = configuration;
    session_delegate_ = delegate;

    OCMStub([session_ configuration]).andReturn(session_configuration_);
    OCMStub([session_ invalidateAndCancel]);

    return session_;
  }

  web::WebTaskEnvironment task_environment_;
  FakeBrowserState browser_state_;
  FakeWebState web_state_;
  std::unique_ptr<DownloadSessionTaskImpl> task_;
  // NSURLSessionDataDelegate callbacks are called on background serial queue.
  dispatch_queue_t session_delegate_callbacks_queue_ = 0;
  __strong id session_ = nil;
  __strong CRWFakeNSURLSessionTask* session_task_ = nil;
  __strong id<NSURLSessionDataDelegate> session_delegate_ = nil;
  __strong NSURLSessionConfiguration* session_configuration_ = nil;
};

// Tests DownloadSessionTaskImpl default state after construction.
TEST_F(DownloadSessionTaskImplTest, DefaultState) {
  EXPECT_EQ(&web_state_, task_->GetWebState());
  EXPECT_EQ(DownloadTask::State::kNotStarted, task_->GetState());
  EXPECT_NE(@"", task_->GetIdentifier());
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

// Tests sucessfull download of response without content.
// (No URLSession:dataTask:didReceiveData: callback).
TEST_F(DownloadSessionTaskImplTest, EmptyContentDownload) {
  CRWFakeNSURLSessionTask* session_task = Start();
  ASSERT_TRUE(session_task);

  // Download has finished.
  SimulateDownloadCompletion(session_task);

  EXPECT_EQ(DownloadTask::State::kComplete, task_->GetState());
  EXPECT_EQ(0, task_->GetErrorCode());
  EXPECT_EQ(0, task_->GetTotalBytes());
  EXPECT_EQ(0, task_->GetReceivedBytes());
  EXPECT_EQ(100, task_->GetPercentComplete());
}

// Tests sucessfull download of response when content length is unknown until
// the download completes.
TEST_F(DownloadSessionTaskImplTest, UnknownLengthContentDownload) {
  CRWFakeNSURLSessionTask* session_task = Start();
  ASSERT_TRUE(session_task);

  // The response has arrived.
  const char kData[] = "foo";
  session_task.countOfBytesExpectedToReceive = -1;
  SimulateDataDownload(session_task, kData);
  EXPECT_EQ(DownloadTask::State::kInProgress, task_->GetState());
  EXPECT_FALSE(task_->IsDone());
  EXPECT_EQ(0, task_->GetErrorCode());
  EXPECT_EQ(-1, task_->GetTotalBytes());
  EXPECT_EQ(-1, task_->GetPercentComplete());

  // Download has finished.
  int64_t kDataSize = strlen(kData);
  session_task.countOfBytesExpectedToReceive = kDataSize;
  SimulateDownloadCompletion(session_task);

  EXPECT_EQ(DownloadTask::State::kComplete, task_->GetState());
  EXPECT_EQ(0, task_->GetErrorCode());
  EXPECT_EQ(kDataSize, task_->GetTotalBytes());
  EXPECT_EQ(100, task_->GetPercentComplete());
  EXPECT_NSEQ(
      @(kData),
      [[NSString alloc]
          initWithData:web::test::GetDownloadTaskResponseData(task_.get())
              encoding:NSUTF8StringEncoding]);
}

// Tests cancelling the download task.
TEST_F(DownloadSessionTaskImplTest, Cancelling) {
  CRWFakeNSURLSessionTask* session_task = Start();
  ASSERT_TRUE(session_task);

  // Cancel the download.
  {
    web::test::WaitDownloadTaskDone observer(task_.get());
    task_->Cancel();
    observer.Wait();
  }

  EXPECT_EQ(DownloadTask::State::kCancelled, task_->GetState());
}

// Tests restarting failed download task.
TEST_F(DownloadSessionTaskImplTest, Restarting) {
  CRWFakeNSURLSessionTask* session_task = Start();
  ASSERT_TRUE(session_task);

  // Download has failed.
  NSError* error = [NSError errorWithDomain:NSURLErrorDomain
                                       code:NSURLErrorNotConnectedToInternet
                                   userInfo:nil];
  SimulateDownloadCompletion(session_task, error);
  EXPECT_EQ(100, task_->GetPercentComplete());

  // Restart the task.
  session_task = Start();
  EXPECT_EQ(0, task_->GetPercentComplete());
  ASSERT_TRUE(session_task);

  // Download has finished.
  SimulateDownloadCompletion(session_task);
  EXPECT_EQ(DownloadTask::State::kComplete, task_->GetState());
  EXPECT_EQ(0, task_->GetErrorCode());
  EXPECT_EQ(100, task_->GetPercentComplete());
}

// Tests sucessfull download of response with only one
// URLSession:dataTask:didReceiveData: callback.
TEST_F(DownloadSessionTaskImplTest, SmallResponseDownload) {
  CRWFakeNSURLSessionTask* session_task = Start();
  ASSERT_TRUE(session_task);

  // The response has arrived.
  const char kData[] = "foo";
  int64_t kDataSize = strlen(kData);
  session_task.countOfBytesExpectedToReceive = kDataSize;
  SimulateDataDownload(session_task, kData);
  EXPECT_EQ(DownloadTask::State::kInProgress, task_->GetState());
  EXPECT_FALSE(task_->IsDone());
  EXPECT_EQ(0, task_->GetErrorCode());
  EXPECT_EQ(kDataSize, task_->GetTotalBytes());
  EXPECT_EQ(kDataSize, task_->GetReceivedBytes());
  EXPECT_EQ(100, task_->GetPercentComplete());

  // Download has finished.
  SimulateDownloadCompletion(session_task);
  EXPECT_EQ(DownloadTask::State::kComplete, task_->GetState());
  EXPECT_EQ(0, task_->GetErrorCode());
  EXPECT_EQ(kDataSize, task_->GetTotalBytes());
  EXPECT_EQ(kDataSize, task_->GetReceivedBytes());
  EXPECT_EQ(100, task_->GetPercentComplete());
  EXPECT_NSEQ(
      @(kData),
      [[NSString alloc]
          initWithData:web::test::GetDownloadTaskResponseData(task_.get())
              encoding:NSUTF8StringEncoding]);
}

// Tests sucessfull download of response with multiple
// URLSession:dataTask:didReceiveData: callbacks.
TEST_F(DownloadSessionTaskImplTest, LargeResponseDownload) {
  CRWFakeNSURLSessionTask* session_task = Start();
  ASSERT_TRUE(session_task);

  // The first part of the response has arrived.
  const char kData1[] = "foo";
  const char kData2[] = "buzz";
  int64_t kData1Size = strlen(kData1);
  int64_t kData2Size = strlen(kData2);
  session_task.countOfBytesExpectedToReceive = kData1Size + kData2Size;
  SimulateDataDownload(session_task, kData1);
  EXPECT_EQ(DownloadTask::State::kInProgress, task_->GetState());
  EXPECT_FALSE(task_->IsDone());
  EXPECT_EQ(0, task_->GetErrorCode());
  EXPECT_EQ(kData1Size + kData2Size, task_->GetTotalBytes());
  EXPECT_EQ(kData1Size, task_->GetReceivedBytes());
  EXPECT_EQ(42, task_->GetPercentComplete());

  // The second part of the response has arrived.
  SimulateDataDownload(session_task, kData2);
  EXPECT_EQ(DownloadTask::State::kInProgress, task_->GetState());
  EXPECT_FALSE(task_->IsDone());
  EXPECT_EQ(0, task_->GetErrorCode());
  EXPECT_EQ(kData1Size + kData2Size, task_->GetTotalBytes());
  EXPECT_EQ(kData1Size + kData2Size, task_->GetReceivedBytes());
  EXPECT_EQ(100, task_->GetPercentComplete());

  // Download has finished.
  SimulateDownloadCompletion(session_task);
  EXPECT_EQ(DownloadTask::State::kComplete, task_->GetState());
  EXPECT_EQ(0, task_->GetErrorCode());
  EXPECT_EQ(kData1Size + kData2Size, task_->GetTotalBytes());
  EXPECT_EQ(kData1Size + kData2Size, task_->GetReceivedBytes());
  EXPECT_EQ(100, task_->GetPercentComplete());
  EXPECT_NSEQ(
      [@(kData1) stringByAppendingString:@(kData2)],
      [[NSString alloc]
          initWithData:web::test::GetDownloadTaskResponseData(task_.get())
              encoding:NSUTF8StringEncoding]);
}

// Tests failed download when URLSession:dataTask:didReceiveData: callback was
// not even called.
TEST_F(DownloadSessionTaskImplTest, FailureInTheBeginning) {
  CRWFakeNSURLSessionTask* session_task = Start();
  ASSERT_TRUE(session_task);

  // Download has failed.
  NSError* error = [NSError errorWithDomain:NSURLErrorDomain
                                       code:NSURLErrorNotConnectedToInternet
                                   userInfo:nil];
  SimulateDownloadCompletion(session_task, error);
  EXPECT_EQ(DownloadTask::State::kFailed, task_->GetState());
  EXPECT_TRUE(task_->GetErrorCode() == net::ERR_INTERNET_DISCONNECTED);
  EXPECT_EQ(0, task_->GetTotalBytes());
  EXPECT_EQ(0, task_->GetReceivedBytes());
  EXPECT_EQ(100, task_->GetPercentComplete());
}

// Tests failed download when URLSession:dataTask:didReceiveData: callback was
// called once.
TEST_F(DownloadSessionTaskImplTest, FailureInTheMiddle) {
  CRWFakeNSURLSessionTask* session_task = Start();
  ASSERT_TRUE(session_task);

  // A part of the response has arrived.
  const char kReceivedData[] = "foo";
  int64_t kReceivedDataSize = strlen(kReceivedData);
  int64_t kExpectedDataSize = kReceivedDataSize + 10;
  session_task.countOfBytesExpectedToReceive = kExpectedDataSize;
  SimulateDataDownload(session_task, kReceivedData);
  EXPECT_EQ(DownloadTask::State::kInProgress, task_->GetState());
  EXPECT_FALSE(task_->IsDone());
  EXPECT_EQ(0, task_->GetErrorCode());
  EXPECT_EQ(kExpectedDataSize, task_->GetTotalBytes());
  EXPECT_EQ(kReceivedDataSize, task_->GetReceivedBytes());
  EXPECT_EQ(23, task_->GetPercentComplete());

  // Download has failed.
  NSError* error = [NSError errorWithDomain:NSURLErrorDomain
                                       code:NSURLErrorNotConnectedToInternet
                                   userInfo:nil];
  session_task.countOfBytesExpectedToReceive = 0;  // This is 0 when offline.
  SimulateDownloadCompletion(session_task, error);
  EXPECT_EQ(DownloadTask::State::kFailed, task_->GetState());
  EXPECT_EQ(task_->GetErrorCode(), net::ERR_INTERNET_DISCONNECTED);
  EXPECT_EQ(kExpectedDataSize, task_->GetTotalBytes());
  EXPECT_EQ(kReceivedDataSize, task_->GetReceivedBytes());
  EXPECT_EQ(23, task_->GetPercentComplete());
  EXPECT_NSEQ(
      @(kReceivedData),
      [[NSString alloc]
          initWithData:web::test::GetDownloadTaskResponseData(task_.get())
              encoding:NSUTF8StringEncoding]);
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
          /*last_update=*/now,
          /*secure=*/false,
          /*httponly=*/false, net::CookieSameSite::UNSPECIFIED,
          net::COOKIE_PRIORITY_DEFAULT, /*same_party=*/false);
  ASSERT_TRUE(expected_cookie);
  cookie_store()->SetAllCookies({*expected_cookie});

  // Start the download and make sure that all cookie from BrowserState were
  // picked up.
  ASSERT_TRUE(Start());

  NSArray<NSHTTPCookie*>* cookies =
      session_configuration().HTTPCookieStorage.cookies;
  EXPECT_EQ(1U, cookies.count);
  NSHTTPCookie* actual_cookie = cookies.firstObject;
  EXPECT_NSEQ(@"name", actual_cookie.name);
  EXPECT_NSEQ(@"value", actual_cookie.value);
}

// Tests that URLFetcherFileWriter deletes the file if download has failed with
// error.
TEST_F(DownloadSessionTaskImplTest, FileDeletion) {
  // Create URLFetcherFileWriter.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath temp_file =
      temp_dir.GetPath().Append(FILE_PATH_LITERAL("DownloadTaskImpl"));
  base::DeleteFile(temp_file);
  ASSERT_FALSE(base::PathExists(temp_file));

  // Start the download.
  CRWFakeNSURLSessionTask* session_task = Start(temp_file);
  ASSERT_TRUE(session_task);

  // Deliver the response and verify that download file exists.
  const char kReceivedData[] = "foo";
  SimulateDataDownload(session_task, kReceivedData);
  ASSERT_TRUE(base::PathExists(temp_file));

  // Fail the download and verify that the file was deleted.
  NSError* error = [NSError errorWithDomain:NSURLErrorDomain
                                       code:NSURLErrorNotConnectedToInternet
                                   userInfo:nil];
  SimulateDownloadCompletion(session_task, error);
}

// Tests changing MIME type during the download.
TEST_F(DownloadSessionTaskImplTest, MimeTypeChange) {
  CRWFakeNSURLSessionTask* session_task = Start();
  ASSERT_TRUE(session_task);

  ASSERT_EQ(kMimeType, task_->GetOriginalMimeType());
  ASSERT_EQ(kMimeType, task_->GetMimeType());
  const char kOtherMimeType[] = "application/foo";
  session_task.response =
      [[NSURLResponse alloc] initWithURL:[NSURL URLWithString:@(kUrl)]
                                MIMEType:@(kOtherMimeType)
                   expectedContentLength:0
                        textEncodingName:nil];
  SimulateDownloadCompletion(session_task);
  EXPECT_EQ(kMimeType, task_->GetOriginalMimeType());
  EXPECT_EQ(kOtherMimeType, task_->GetMimeType());
}

// Tests updating HTTP response code.
TEST_F(DownloadSessionTaskImplTest, HttpResponseCode) {
  CRWFakeNSURLSessionTask* session_task = Start();
  ASSERT_TRUE(session_task);

  int kHttpCode = 303;
  session_task.response =
      [[NSHTTPURLResponse alloc] initWithURL:[NSURL URLWithString:@(kUrl)]
                                  statusCode:303
                                 HTTPVersion:nil
                                headerFields:nil];
  SimulateDownloadCompletion(session_task);
  EXPECT_EQ(kHttpCode, task_->GetHttpCode());
}

// Tests that destructing DownloadTaskImpl calls -[NSURLSessionDataTask cancel]
// and OnTaskDestroyed().
TEST_F(DownloadSessionTaskImplTest, DownloadTaskDestruction) {
  CRWFakeNSURLSessionTask* session_task = Start();
  ASSERT_TRUE(session_task);
  task_.reset();  // Destruct DownloadTaskImpl.
  EXPECT_TRUE(session_task.state = NSURLSessionTaskStateCanceling);
}

}  // namespace web
