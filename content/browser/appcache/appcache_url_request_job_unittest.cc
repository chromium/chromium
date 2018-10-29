// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/appcache_url_request_job.h"

#include <stdint.h>
#include <string.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/containers/stack.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/pickle.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/appcache/appcache_response.h"
#include "content/browser/appcache/mock_appcache_service.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/request_priority.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_error_job.h"
#include "net/url_request/url_request_job_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using net::IOBuffer;
using net::WrappedIOBuffer;

namespace content {
namespace appcache_url_request_job_unittest {

const char kHttpBasicHeaders[] = "HTTP/1.0 200 OK\0Content-Length: 5\0\0";
const char kHttpBasicBody[] = "Hello";

const int kNumBlocks = 4;
const int kBlockSize = 1024;

// Used as an AppCacheURLRequestJob::OnPrepareToRestartCallback for requests
// that aren't expected to be restarted.
void ExpectNotRestarted() {
  ADD_FAILURE() << "Request unexpectedly restarted";
}

// Used as an AppCacheURLRequestJob::OnPrepareToRestartCallback for requests
// that are expected to be restarted. Allows tests to verify it was called by
// setting |*value| to true when invoked.
void SetIfCalled(bool* value) {
  // Expected to be called only once.
  EXPECT_FALSE(*value);
  *value = true;
}

class MockURLRequestJobFactory : public net::URLRequestJobFactory {
 public:
  MockURLRequestJobFactory() {}

  ~MockURLRequestJobFactory() override { DCHECK(!job_); }

  void SetJob(std::unique_ptr<net::URLRequestJob> job) {
    job_ = std::move(job);
  }

  bool has_job() const { return job_.get() != nullptr; }

  // net::URLRequestJobFactory implementation.
  net::URLRequestJob* MaybeCreateJobWithProtocolHandler(
      const std::string& scheme,
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    if (job_) {
      return job_.release();
    } else {
      return new net::URLRequestErrorJob(request,
                                         network_delegate,
                                         net::ERR_INTERNET_DISCONNECTED);
    }
  }

  net::URLRequestJob* MaybeInterceptRedirect(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate,
      const GURL& location) const override {
    return nullptr;
  }

  net::URLRequestJob* MaybeInterceptResponse(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    return nullptr;
  }

  bool IsHandledProtocol(const std::string& scheme) const override {
    return scheme == "http";
  };

  bool IsSafeRedirectTarget(const GURL& location) const override {
    return false;
  }

 private:
  // This is mutable because MaybeCreateJobWithProtocolHandler is const.
  mutable std::unique_ptr<net::URLRequestJob> job_;
};

class AppCacheURLRequestJobTest : public testing::Test {
 public:

  // Test Harness -------------------------------------------------------------
  // TODO(michaeln): share this test harness with AppCacheResponseTest

  class MockStorageDelegate : public AppCacheStorage::Delegate {
   public:
    explicit MockStorageDelegate(AppCacheURLRequestJobTest* test)
        : loaded_info_id_(0), test_(test) {
    }

    void OnResponseInfoLoaded(AppCacheResponseInfo* info,
                              int64_t response_id) override {
      loaded_info_ = info;
      loaded_info_id_ = response_id;
      test_->ScheduleNextTask();
    }

    scoped_refptr<AppCacheResponseInfo> loaded_info_;
    int64_t loaded_info_id_;
    AppCacheURLRequestJobTest* test_;
  };

  class MockURLRequestDelegate : public net::URLRequest::Delegate {
   public:
    explicit MockURLRequestDelegate(AppCacheURLRequestJobTest* test)
        : test_(test),
          received_data_(
              base::MakeRefCounted<net::IOBuffer>(kNumBlocks * kBlockSize)),
          did_receive_headers_(false),
          amount_received_(0),
          kill_after_amount_received_(0),
          kill_with_io_pending_(false),
          request_status_(net::ERR_IO_PENDING) {}

    void OnResponseStarted(net::URLRequest* request, int net_error) override {
      DCHECK_NE(net::ERR_IO_PENDING, net_error);
      amount_received_ = 0;
      did_receive_headers_ = false;
      request_status_ = net_error;
      if (net_error == net::OK) {
        EXPECT_TRUE(request->response_headers());
        did_receive_headers_ = true;
        received_info_ = request->response_info();
        ReadSome(request);
      } else {
        RequestComplete();
      }
    }

    void OnReadCompleted(net::URLRequest* request, int bytes_read) override {
      if (bytes_read > 0) {
        amount_received_ += bytes_read;
        request_status_ = net::OK;

        if (kill_after_amount_received_ && !kill_with_io_pending_) {
          if (amount_received_ >= kill_after_amount_received_) {
            request->Cancel();
            return;
          }
        }

        ReadSome(request);

        if (kill_after_amount_received_ && kill_with_io_pending_) {
          if (amount_received_ >= kill_after_amount_received_) {
            request->Cancel();
            return;
          }
        }
      } else {
        request_status_ = bytes_read;
        RequestComplete();
      }
    }

    void ReadSome(net::URLRequest* request) {
      DCHECK(amount_received_ + kBlockSize <= kNumBlocks * kBlockSize);
      scoped_refptr<IOBuffer> wrapped_buffer =
          base::MakeRefCounted<net::WrappedIOBuffer>(received_data_->data() +
                                                     amount_received_);
      EXPECT_EQ(net::ERR_IO_PENDING,
                request->Read(wrapped_buffer.get(), kBlockSize));
    }

    void RequestComplete() {
      test_->ScheduleNextTask();
    }

    int request_status() { return request_status_; }

    AppCacheURLRequestJobTest* test_;
    net::HttpResponseInfo received_info_;
    scoped_refptr<net::IOBuffer> received_data_;
    bool did_receive_headers_;
    int amount_received_;
    int kill_after_amount_received_;
    bool kill_with_io_pending_;
    int request_status_;
  };

  // Helper callback to run a test on our io_thread. The io_thread is spun up
  // once and reused for all tests.
  template <class Method>
  void MethodWrapper(Method method) {
    SetUpTest();
    (this->*method)();
  }

  static void SetUpTestCase() {
    scoped_task_environment_.reset(new base::test::ScopedTaskEnvironment());
    io_thread_.reset(new base::Thread("AppCacheURLRequestJobTest Thread"));
    base::Thread::Options options(base::MessageLoop::TYPE_IO, 0);
    io_thread_->StartWithOptions(options);
  }

  static void TearDownTestCase() {
    io_thread_.reset();
    scoped_task_environment_.reset();
  }

  AppCacheURLRequestJobTest() {}

  template <class Method>
  void RunTestOnIOThread(Method method) {
    test_finished_event_.reset(new base::WaitableEvent(
        base::WaitableEvent::ResetPolicy::AUTOMATIC,
        base::WaitableEvent::InitialState::NOT_SIGNALED));
    io_thread_->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&AppCacheURLRequestJobTest::MethodWrapper<Method>,
                       base::Unretained(this), method));
    test_finished_event_->Wait();
  }

  void SetUpTest() {
    DCHECK(io_thread_->task_runner()->BelongsToCurrentThread());
    DCHECK(task_stack_.empty());

    storage_delegate_.reset(new MockStorageDelegate(this));
    service_.reset(new MockAppCacheService());
    expected_read_result_ = 0;
    expected_write_result_ = 0;
    written_response_id_ = 0;
    reader_deletion_count_down_ = 0;
    writer_deletion_count_down_ = 0;

    restart_callback_invoked_ = false;

    url_request_delegate_.reset(new MockURLRequestDelegate(this));
    job_factory_.reset(new MockURLRequestJobFactory());
    empty_context_.reset(new net::URLRequestContext());
    empty_context_->set_job_factory(job_factory_.get());
  }

  void TearDownTest() {
    DCHECK(io_thread_->task_runner()->BelongsToCurrentThread());
    request_.reset();

    while (!task_stack_.empty())
      task_stack_.pop();

    reader_.reset();
    read_buffer_ = nullptr;
    read_info_buffer_ = nullptr;
    writer_.reset();
    write_buffer_ = nullptr;
    write_info_buffer_ = nullptr;
    storage_delegate_.reset();
    service_.reset();

    DCHECK(!job_factory_->has_job());
    empty_context_.reset();
    job_factory_.reset();
    url_request_delegate_.reset();
  }

  void TestFinished() {
    // We unwind the stack prior to finishing up to let stack
    // based objects get deleted.
    DCHECK(io_thread_->task_runner()->BelongsToCurrentThread());
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&AppCacheURLRequestJobTest::TestFinishedUnwound,
                       base::Unretained(this)));
  }

  void TestFinishedUnwound() {
    TearDownTest();
    test_finished_event_->Signal();
  }

  void PushNextTask(base::OnceClosure task) {
    task_stack_.push(
        std::pair<base::OnceClosure, bool>(std::move(task), false));
  }

  void PushNextTaskAsImmediate(base::OnceClosure task) {
    task_stack_.push(std::pair<base::OnceClosure, bool>(std::move(task), true));
  }

  void ScheduleNextTask() {
    DCHECK(io_thread_->task_runner()->BelongsToCurrentThread());
    if (task_stack_.empty()) {
      TestFinished();
      return;
    }
    base::OnceClosure task = std::move(task_stack_.top().first);
    bool immediate = task_stack_.top().second;
    task_stack_.pop();
    if (immediate)
      std::move(task).Run();
    else
      base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, std::move(task));
  }

  // Wrappers to call AppCacheResponseReader/Writer Read and Write methods

  void WriteBasicResponse() {
    scoped_refptr<IOBuffer> body =
        base::MakeRefCounted<WrappedIOBuffer>(kHttpBasicBody);
    std::string raw_headers(kHttpBasicHeaders, arraysize(kHttpBasicHeaders));
    WriteResponse(
        MakeHttpResponseInfo(raw_headers), body.get(), strlen(kHttpBasicBody));
  }

  void WriteResponse(std::unique_ptr<net::HttpResponseInfo> head,
                     IOBuffer* body,
                     int body_len) {
    DCHECK(body);
    scoped_refptr<IOBuffer> body_ref(body);
    PushNextTask(base::BindOnce(&AppCacheURLRequestJobTest::WriteResponseBody,
                                base::Unretained(this), body_ref, body_len));
    WriteResponseHead(std::move(head));
  }

  void WriteResponseHead(std::unique_ptr<net::HttpResponseInfo> head) {
    EXPECT_FALSE(writer_->IsWritePending());
    expected_write_result_ = GetHttpResponseInfoSize(*head);
    write_info_buffer_ =
        base::MakeRefCounted<HttpResponseInfoIOBuffer>(std::move(head));
    writer_->WriteInfo(
        write_info_buffer_.get(),
        base::BindOnce(&AppCacheURLRequestJobTest::OnWriteInfoComplete,
                       base::Unretained(this)));
  }

  void WriteResponseBody(scoped_refptr<IOBuffer> io_buffer, int buf_len) {
    EXPECT_FALSE(writer_->IsWritePending());
    write_buffer_ = io_buffer;
    expected_write_result_ = buf_len;
    writer_->WriteData(
        write_buffer_.get(), buf_len,
        base::BindOnce(&AppCacheURLRequestJobTest::OnWriteComplete,
                       base::Unretained(this)));
  }

  void ReadResponseBody(scoped_refptr<IOBuffer> io_buffer, int buf_len) {
    EXPECT_FALSE(reader_->IsReadPending());
    read_buffer_ = io_buffer;
    expected_read_result_ = buf_len;
    reader_->ReadData(read_buffer_.get(), buf_len,
                      base::BindOnce(&AppCacheURLRequestJobTest::OnReadComplete,
                                     base::Unretained(this)));
  }

  // AppCacheResponseReader / Writer completion callbacks

  void OnWriteInfoComplete(int result) {
    EXPECT_FALSE(writer_->IsWritePending());
    EXPECT_EQ(expected_write_result_, result);
    ScheduleNextTask();
  }

  void OnWriteComplete(int result) {
    EXPECT_FALSE(writer_->IsWritePending());
    EXPECT_EQ(expected_write_result_, result);
    ScheduleNextTask();
  }

  void OnReadInfoComplete(int result) {
    EXPECT_FALSE(reader_->IsReadPending());
    EXPECT_EQ(expected_read_result_, result);
    ScheduleNextTask();
  }

  void OnReadComplete(int result) {
    EXPECT_FALSE(reader_->IsReadPending());
    EXPECT_EQ(expected_read_result_, result);
    ScheduleNextTask();
  }

  // Helpers to work with HttpResponseInfo objects

  std::unique_ptr<net::HttpResponseInfo> MakeHttpResponseInfo(
      const std::string& raw_headers) {
    std::unique_ptr<net::HttpResponseInfo> info =
        std::make_unique<net::HttpResponseInfo>();
    info->request_time = base::Time::Now();
    info->response_time = base::Time::Now();
    info->was_cached = false;
    info->headers = base::MakeRefCounted<net::HttpResponseHeaders>(raw_headers);
    return info;
  }

  int GetHttpResponseInfoSize(const net::HttpResponseInfo& info) {
    base::Pickle pickle = PickleHttpResonseInfo(info);
    return pickle.size();
  }

  bool CompareHttpResponseInfos(const net::HttpResponseInfo& info1,
                                const net::HttpResponseInfo& info2) {
    base::Pickle pickle1 = PickleHttpResonseInfo(info1);
    base::Pickle pickle2 = PickleHttpResonseInfo(info2);
    return (pickle1.size() == pickle2.size()) &&
           (0 == memcmp(pickle1.data(), pickle2.data(), pickle1.size()));
  }

  base::Pickle PickleHttpResonseInfo(const net::HttpResponseInfo& info) {
    const bool kSkipTransientHeaders = true;
    const bool kTruncated = false;
    base::Pickle pickle;
    info.Persist(&pickle, kSkipTransientHeaders, kTruncated);
    return pickle;
  }

  // Helpers to fill and verify blocks of memory with a value

  void FillData(char value, char* data, int data_len) {
    memset(data, value, data_len);
  }

  bool CheckData(char value, const char* data, int data_len) {
    for (int i = 0; i < data_len; ++i, ++data) {
      if (*data != value)
        return false;
    }
    return true;
  }

  // Individual Tests ---------------------------------------------------------
  // Some of the individual tests involve multiple async steps. Each test
  // is delineated with a section header.

  // Basic -------------------------------------------------------------------
  void Basic() {
    AppCacheStorage* storage = service_->storage();
    request_ = empty_context_->CreateRequest(GURL("http://blah/"),
                                             net::DEFAULT_PRIORITY, nullptr,
                                             TRAFFIC_ANNOTATION_FOR_TESTS);

    // Create an instance and see that it looks as expected.

    std::unique_ptr<AppCacheURLRequestJob> job(
        new AppCacheURLRequestJob(request_.get(), nullptr, storage, nullptr,
                                  false, base::BindOnce(&ExpectNotRestarted)));
    EXPECT_TRUE(job->IsWaiting());
    EXPECT_FALSE(job->IsDeliveringAppCacheResponse());
    EXPECT_FALSE(job->IsDeliveringNetworkResponse());
    EXPECT_FALSE(job->IsDeliveringErrorResponse());
    EXPECT_FALSE(job->IsStarted());
    EXPECT_FALSE(job->has_been_killed());
    EXPECT_EQ(GURL(), job->manifest_url());
    EXPECT_EQ(kAppCacheNoCacheId, job->cache_id());
    EXPECT_FALSE(job->entry().has_response_id());

    TestFinished();
  }

  // DeliveryOrders -----------------------------------------------------
  void DeliveryOrders() {
    AppCacheStorage* storage = service_->storage();
    std::unique_ptr<net::URLRequest> request(empty_context_->CreateRequest(
        GURL("http://blah/"), net::DEFAULT_PRIORITY, nullptr,
        TRAFFIC_ANNOTATION_FOR_TESTS));

    // Create an instance, give it a delivery order and see that
    // it looks as expected.

    std::unique_ptr<AppCacheURLRequestJob> job(
        new AppCacheURLRequestJob(request.get(), nullptr, storage, nullptr,
                                  false, base::BindOnce(&ExpectNotRestarted)));
    job->DeliverErrorResponse();
    EXPECT_TRUE(job->IsDeliveringErrorResponse());
    EXPECT_FALSE(job->IsStarted());

    job.reset(new AppCacheURLRequestJob(request.get(), nullptr, storage,
                                        nullptr, false,
                                        base::BindOnce(&ExpectNotRestarted)));
    job->DeliverNetworkResponse();
    EXPECT_TRUE(job->IsDeliveringNetworkResponse());
    EXPECT_FALSE(job->IsStarted());

    job.reset(new AppCacheURLRequestJob(request.get(), nullptr, storage,
                                        nullptr, false,
                                        base::BindOnce(&ExpectNotRestarted)));
    const GURL kManifestUrl("http://blah/");
    const int64_t kCacheId(1);
    const AppCacheEntry kEntry(AppCacheEntry::EXPLICIT, 1);
    job->DeliverAppCachedResponse(kManifestUrl, kCacheId, kEntry, false);
    EXPECT_FALSE(job->IsWaiting());
    EXPECT_TRUE(job->IsDeliveringAppCacheResponse());
    EXPECT_FALSE(job->IsStarted());
    EXPECT_EQ(kManifestUrl, job->manifest_url());
    EXPECT_EQ(kCacheId, job->cache_id());
    EXPECT_EQ(kEntry.types(), job->entry().types());
    EXPECT_EQ(kEntry.response_id(), job->entry().response_id());

    TestFinished();
  }

  // DeliverNetworkResponse --------------------------------------------------

  void DeliverNetworkResponse() {
    // This test has async steps.
    PushNextTask(
        base::BindOnce(&AppCacheURLRequestJobTest::VerifyDeliverNetworkResponse,
                       base::Unretained(this)));

    AppCacheStorage* storage = service_->storage();
    request_ = empty_context_->CreateRequest(
        GURL("http://blah/"), net::DEFAULT_PRIORITY,
        url_request_delegate_.get(), TRAFFIC_ANNOTATION_FOR_TESTS);

    // Set up to create an AppCacheURLRequestJob with orders to deliver
    // a network response.
    std::unique_ptr<AppCacheURLRequestJob> mock_job(new AppCacheURLRequestJob(
        request_.get(), nullptr, storage, nullptr, false,
        base::BindOnce(&SetIfCalled, &restart_callback_invoked_)));
    mock_job->DeliverNetworkResponse();
    EXPECT_TRUE(mock_job->IsDeliveringNetworkResponse());
    EXPECT_FALSE(mock_job->IsStarted());
    job_factory_->SetJob(std::move(mock_job));

    // Start the request.
    request_->Start();

    // The job should have been picked up.
    EXPECT_FALSE(job_factory_->has_job());
    // Completion is async.
  }

  void VerifyDeliverNetworkResponse() {
    EXPECT_EQ(net::ERR_INTERNET_DISCONNECTED,
              url_request_delegate_->request_status());
    EXPECT_TRUE(restart_callback_invoked_);
    TestFinished();
  }

  // DeliverErrorResponse --------------------------------------------------

  void DeliverErrorResponse() {
    // This test has async steps.
    PushNextTask(
        base::BindOnce(&AppCacheURLRequestJobTest::VerifyDeliverErrorResponse,
                       base::Unretained(this)));

    AppCacheStorage* storage = service_->storage();
    request_ = empty_context_->CreateRequest(
        GURL("http://blah/"), net::DEFAULT_PRIORITY,
        url_request_delegate_.get(), TRAFFIC_ANNOTATION_FOR_TESTS);

    // Setup to create an AppCacheURLRequestJob with orders to deliver
    // a network response.
    std::unique_ptr<AppCacheURLRequestJob> mock_job(
        new AppCacheURLRequestJob(request_.get(), nullptr, storage, nullptr,
                                  false, base::BindOnce(&ExpectNotRestarted)));
    mock_job->DeliverErrorResponse();
    EXPECT_TRUE(mock_job->IsDeliveringErrorResponse());
    EXPECT_FALSE(mock_job->IsStarted());
    job_factory_->SetJob(std::move(mock_job));

    // Start the request.
    request_->Start();

    // The job should have been picked up.
    EXPECT_FALSE(job_factory_->has_job());
    // Completion is async.
  }

  void VerifyDeliverErrorResponse() {
    EXPECT_EQ(net::ERR_FAILED, url_request_delegate_->request_status());
    TestFinished();
  }

  // DeliverSmallAppCachedResponse --------------------------------------
  // "Small" being small enough to read completely in a single
  // request->Read call.

  void DeliverSmallAppCachedResponse() {
    // This test has several async steps.
    // 1. Write a small response to response storage.
    // 2. Use net::URLRequest to retrieve it.
    // 3. Verify we received what we expected to receive.

    PushNextTask(base::BindOnce(
        &AppCacheURLRequestJobTest::VerifyDeliverSmallAppCachedResponse,
        base::Unretained(this)));
    PushNextTask(
        base::BindOnce(&AppCacheURLRequestJobTest::RequestAppCachedResource,
                       base::Unretained(this), false));

    writer_ = service_->storage()->CreateResponseWriter(GURL());
    written_response_id_ = writer_->response_id();
    WriteBasicResponse();
    // Continues async
  }

  void RequestAppCachedResource(bool start_after_delivery_orders) {
    AppCacheStorage* storage = service_->storage();
    request_ = empty_context_->CreateRequest(
        GURL("http://blah/"), net::DEFAULT_PRIORITY,
        url_request_delegate_.get(), TRAFFIC_ANNOTATION_FOR_TESTS);

    // Setup to create an AppCacheURLRequestJob with orders to deliver
    // a network response.
    std::unique_ptr<AppCacheURLRequestJob> job(
        new AppCacheURLRequestJob(request_.get(), nullptr, storage, nullptr,
                                  false, base::BindOnce(&ExpectNotRestarted)));

    if (start_after_delivery_orders) {
      job->DeliverAppCachedResponse(
          GURL(), 111,
          AppCacheEntry(AppCacheEntry::EXPLICIT, written_response_id_), false);
      EXPECT_TRUE(job->IsDeliveringAppCacheResponse());
    }

    // Start the request.
    EXPECT_FALSE(job->IsStarted());
    base::WeakPtr<AppCacheJob> weak_job = job->GetWeakPtr();
    job_factory_->SetJob(std::move(job));
    request_->Start();
    EXPECT_FALSE(job_factory_->has_job());
    ASSERT_TRUE(weak_job);
    EXPECT_TRUE(weak_job->IsStarted());

    if (!start_after_delivery_orders) {
      weak_job->DeliverAppCachedResponse(
          GURL(), 111,
          AppCacheEntry(AppCacheEntry::EXPLICIT, written_response_id_), false);
      ASSERT_TRUE(weak_job);
      EXPECT_TRUE(weak_job->IsDeliveringAppCacheResponse());
    }

    // Completion is async.
  }

  void VerifyDeliverSmallAppCachedResponse() {
    EXPECT_EQ(net::OK, url_request_delegate_->request_status());
    EXPECT_TRUE(CompareHttpResponseInfos(
        *write_info_buffer_->http_info, url_request_delegate_->received_info_));
    EXPECT_EQ(5, url_request_delegate_->amount_received_);
    EXPECT_EQ(0, memcmp(kHttpBasicBody,
                        url_request_delegate_->received_data_->data(),
                        strlen(kHttpBasicBody)));
    TestFinished();
  }

  // DeliverLargeAppCachedResponse --------------------------------------
  // "Large" enough to require multiple calls to request->Read to complete.

  void DeliverLargeAppCachedResponse() {
    // This test has several async steps.
    // 1. Write a large response to response storage.
    // 2. Use net::URLRequest to retrieve it.
    // 3. Verify we received what we expected to receive.

    PushNextTask(base::BindOnce(
        &AppCacheURLRequestJobTest::VerifyDeliverLargeAppCachedResponse,
        base::Unretained(this)));
    PushNextTask(
        base::BindOnce(&AppCacheURLRequestJobTest::RequestAppCachedResource,
                       base::Unretained(this), true));

    writer_ = service_->storage()->CreateResponseWriter(GURL());
    written_response_id_ = writer_->response_id();
    WriteLargeResponse();
    // Continues async
  }

  void WriteLargeResponse() {
    // 3, 1k blocks
    static const char kHttpHeaders[] =
        "HTTP/1.0 200 OK\0Content-Length: 3072\0\0";
    scoped_refptr<IOBuffer> body =
        base::MakeRefCounted<IOBuffer>(kBlockSize * 3);
    char* p = body->data();
    for (int i = 0; i < 3; ++i, p += kBlockSize)
      FillData(i + 1, p, kBlockSize);
    std::string raw_headers(kHttpHeaders, arraysize(kHttpHeaders));
    WriteResponse(
        MakeHttpResponseInfo(raw_headers), body.get(), kBlockSize * 3);
  }

  void VerifyDeliverLargeAppCachedResponse() {
    EXPECT_EQ(net::OK, url_request_delegate_->request_status());
    EXPECT_TRUE(CompareHttpResponseInfos(
        *write_info_buffer_->http_info, url_request_delegate_->received_info_));
    EXPECT_EQ(3072, url_request_delegate_->amount_received_);
    char* p = url_request_delegate_->received_data_->data();
    for (int i = 0; i < 3; ++i, p += kBlockSize)
      EXPECT_TRUE(CheckData(i + 1, p, kBlockSize));
    TestFinished();
  }

  // DeliverPartialResponse --------------------------------------

  void DeliverPartialResponse() {
    // This test has several async steps.
    // 1. Write a small response to response storage.
    // 2. Use net::URLRequest to retrieve it a subset using a range request
    // 3. Verify we received what we expected to receive.
    PushNextTask(
        base::BindOnce(&AppCacheURLRequestJobTest::VerifyDeliverPartialResponse,
                       base::Unretained(this)));
    PushNextTask(base::BindOnce(&AppCacheURLRequestJobTest::MakeRangeRequest,
                                base::Unretained(this)));
    writer_ = service_->storage()->CreateResponseWriter(GURL());
    written_response_id_ = writer_->response_id();
    WriteBasicResponse();
    // Continues async
  }

  void MakeRangeRequest() {
    AppCacheStorage* storage = service_->storage();
    request_ = empty_context_->CreateRequest(
        GURL("http://blah/"), net::DEFAULT_PRIORITY,
        url_request_delegate_.get(), TRAFFIC_ANNOTATION_FOR_TESTS);

    // Request a range, the 3 middle chars out of 'Hello'
    net::HttpRequestHeaders extra_headers;
    extra_headers.SetHeader("Range", "bytes= 1-3");
    request_->SetExtraRequestHeaders(extra_headers);

    // Create job with orders to deliver an appcached entry.
    std::unique_ptr<AppCacheURLRequestJob> job(
        new AppCacheURLRequestJob(request_.get(), nullptr, storage, nullptr,
                                  false, base::BindOnce(&ExpectNotRestarted)));
    job->DeliverAppCachedResponse(
        GURL(), 111,
        AppCacheEntry(AppCacheEntry::EXPLICIT, written_response_id_), false);
    EXPECT_TRUE(job->IsDeliveringAppCacheResponse());

    // Start the request.
    EXPECT_FALSE(job->IsStarted());
    job_factory_->SetJob(std::move(job));
    request_->Start();
    EXPECT_FALSE(job_factory_->has_job());
    // Completion is async.
  }

  void VerifyDeliverPartialResponse() {
    EXPECT_EQ(net::OK, url_request_delegate_->request_status());
    EXPECT_EQ(3, url_request_delegate_->amount_received_);
    EXPECT_EQ(0, memcmp(kHttpBasicBody + 1,
                        url_request_delegate_->received_data_->data(),
                        3));
    net::HttpResponseHeaders* headers =
        url_request_delegate_->received_info_.headers.get();
    EXPECT_EQ(206, headers->response_code());
    EXPECT_EQ(3, headers->GetContentLength());
    int64_t range_start, range_end, object_size;
    EXPECT_TRUE(
        headers->GetContentRangeFor206(&range_start, &range_end, &object_size));
    EXPECT_EQ(1, range_start);
    EXPECT_EQ(3, range_end);
    EXPECT_EQ(5, object_size);
    TestFinished();
  }

  // CancelRequest --------------------------------------

  void CancelRequest() {
    // This test has several async steps.
    // 1. Write a large response to response storage.
    // 2. Use net::URLRequest to retrieve it.
    // 3. Cancel the request after data starts coming in.

    PushNextTask(base::BindOnce(&AppCacheURLRequestJobTest::VerifyCancel,
                                base::Unretained(this)));
    PushNextTask(
        base::BindOnce(&AppCacheURLRequestJobTest::RequestAppCachedResource,
                       base::Unretained(this), true));

    writer_ = service_->storage()->CreateResponseWriter(GURL());
    written_response_id_ = writer_->response_id();
    WriteLargeResponse();

    url_request_delegate_->kill_after_amount_received_ = kBlockSize;
    url_request_delegate_->kill_with_io_pending_ = false;
    // Continues async
  }

  void VerifyCancel() {
    EXPECT_EQ(net::ERR_ABORTED, url_request_delegate_->request_status());
    TestFinished();
  }

  // CancelRequestWithIOPending --------------------------------------

  void CancelRequestWithIOPending() {
    // This test has several async steps.
    // 1. Write a large response to response storage.
    // 2. Use net::URLRequest to retrieve it.
    // 3. Cancel the request after data starts coming in.

    PushNextTask(base::BindOnce(&AppCacheURLRequestJobTest::VerifyCancel,
                                base::Unretained(this)));
    PushNextTask(
        base::BindOnce(&AppCacheURLRequestJobTest::RequestAppCachedResource,
                       base::Unretained(this), true));

    writer_ = service_->storage()->CreateResponseWriter(GURL());
    written_response_id_ = writer_->response_id();
    WriteLargeResponse();

    url_request_delegate_->kill_after_amount_received_ = kBlockSize;
    url_request_delegate_->kill_with_io_pending_ = true;
    // Continues async
  }


  // Data members --------------------------------------------------------

  std::unique_ptr<base::WaitableEvent> test_finished_event_;
  std::unique_ptr<MockStorageDelegate> storage_delegate_;
  std::unique_ptr<MockAppCacheService> service_;
  base::stack<std::pair<base::OnceClosure, bool>> task_stack_;

  std::unique_ptr<AppCacheResponseReader> reader_;
  scoped_refptr<HttpResponseInfoIOBuffer> read_info_buffer_;
  scoped_refptr<IOBuffer> read_buffer_;
  int expected_read_result_;
  int reader_deletion_count_down_;

  int64_t written_response_id_;
  std::unique_ptr<AppCacheResponseWriter> writer_;
  scoped_refptr<HttpResponseInfoIOBuffer> write_info_buffer_;
  scoped_refptr<IOBuffer> write_buffer_;
  int expected_write_result_;
  int writer_deletion_count_down_;

  bool restart_callback_invoked_;

  std::unique_ptr<MockURLRequestJobFactory> job_factory_;
  std::unique_ptr<net::URLRequestContext> empty_context_;
  std::unique_ptr<net::URLRequest> request_;
  std::unique_ptr<MockURLRequestDelegate> url_request_delegate_;

  static std::unique_ptr<base::Thread> io_thread_;
  static std::unique_ptr<base::test::ScopedTaskEnvironment>
      scoped_task_environment_;
};

// static
std::unique_ptr<base::Thread> AppCacheURLRequestJobTest::io_thread_;
std::unique_ptr<base::test::ScopedTaskEnvironment>
    AppCacheURLRequestJobTest::scoped_task_environment_;

TEST_F(AppCacheURLRequestJobTest, Basic) {
  RunTestOnIOThread(&AppCacheURLRequestJobTest::Basic);
}

TEST_F(AppCacheURLRequestJobTest, DeliveryOrders) {
  RunTestOnIOThread(&AppCacheURLRequestJobTest::DeliveryOrders);
}

TEST_F(AppCacheURLRequestJobTest, DeliverNetworkResponse) {
  RunTestOnIOThread(&AppCacheURLRequestJobTest::DeliverNetworkResponse);
}

TEST_F(AppCacheURLRequestJobTest, DeliverErrorResponse) {
  RunTestOnIOThread(&AppCacheURLRequestJobTest::DeliverErrorResponse);
}

TEST_F(AppCacheURLRequestJobTest, DeliverSmallAppCachedResponse) {
  RunTestOnIOThread(&AppCacheURLRequestJobTest::DeliverSmallAppCachedResponse);
}

TEST_F(AppCacheURLRequestJobTest, DeliverLargeAppCachedResponse) {
  RunTestOnIOThread(&AppCacheURLRequestJobTest::DeliverLargeAppCachedResponse);
}

TEST_F(AppCacheURLRequestJobTest, DeliverPartialResponse) {
  RunTestOnIOThread(&AppCacheURLRequestJobTest::DeliverPartialResponse);
}

TEST_F(AppCacheURLRequestJobTest, CancelRequest) {
  RunTestOnIOThread(&AppCacheURLRequestJobTest::CancelRequest);
}

TEST_F(AppCacheURLRequestJobTest, CancelRequestWithIOPending) {
  RunTestOnIOThread(&AppCacheURLRequestJobTest::CancelRequestWithIOPending);
}

}  // namespace appcache_url_request_job_unittest
}  // namespace content
