// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/mojo_async_resource_handler.h"

#include <string.h>

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/gtest_util.h"
#include "base/test/test_simple_task_runner.h"
#include "base/timer/mock_timer.h"
#include "content/browser/loader/mock_resource_loader.h"
#include "content/browser/loader/resource_controller.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/loader/resource_request_info_impl.h"
#include "content/public/browser/appcache_service.h"
#include "content/public/browser/navigation_data.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/resource_dispatcher_host_delegate.h"
#include "content/public/browser/resource_throttle.h"
#include "content/public/browser/stream_info.h"
#include "content/public/common/previews_state.h"
#include "content/public/common/resource_type.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "mojo/public/c/system/data_pipe.h"
#include "mojo/public/c/system/types.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/auth.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_status_flags.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/ssl/ssl_info.h"
#include "net/test/cert_test_util.h"
#include "net/test/url_request/url_request_mock_data_job.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "net/url_request/url_request_status.h"
#include "net/url_request/url_request_test_job.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/public/cpp/resource_response.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/resource_scheduler.h"
#include "services/network/test/test_url_loader_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"

namespace content {
namespace {

constexpr int kSizeMimeSnifferRequiresForFirstOnWillRead = 2048;

class DummyUploadDataStream : public net::UploadDataStream {
 public:
  DummyUploadDataStream() : UploadDataStream(false, 0) {}

  int InitInternal(const net::NetLogWithSource& net_log) override {
    NOTREACHED();
    return 0;
  }
  int ReadInternal(net::IOBuffer* buf, int buf_len) override {
    NOTREACHED();
    return 0;
  }
  void ResetInternal() override { NOTREACHED(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(DummyUploadDataStream);
};

class FakeUploadProgressTracker : public network::UploadProgressTracker {
 public:
  using network::UploadProgressTracker::UploadProgressTracker;

  net::UploadProgress GetUploadProgress() const override {
    return upload_progress_;
  }
  base::TimeTicks GetCurrentTime() const override { return current_time_; }

  net::UploadProgress upload_progress_;
  base::TimeTicks current_time_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeUploadProgressTracker);
};

class TestResourceDispatcherHostDelegate final
    : public ResourceDispatcherHostDelegate {
 public:
  TestResourceDispatcherHostDelegate() = default;
  ~TestResourceDispatcherHostDelegate() override = default;

  void RequestBeginning(
      net::URLRequest* request,
      ResourceContext* resource_context,
      AppCacheService* appcache_service,
      ResourceType resource_type,
      std::vector<std::unique_ptr<ResourceThrottle>>* throttles) override {
    ADD_FAILURE() << "RequestBeginning should not be called.";
  }

  void DownloadStarting(
      net::URLRequest* request,
      ResourceContext* resource_context,
      bool is_content_initiated,
      bool must_download,
      bool is_new_request,
      std::vector<std::unique_ptr<ResourceThrottle>>* throttles) override {
    ADD_FAILURE() << "DownloadStarting should not be called.";
  }

  bool ShouldInterceptResourceAsStream(net::URLRequest* request,
                                       const std::string& mime_type,
                                       GURL* origin,
                                       std::string* payload) override {
    ADD_FAILURE() << "ShouldInterceptResourceAsStream should not be called.";
    return false;
  }

  void OnStreamCreated(net::URLRequest* request,
                       std::unique_ptr<content::StreamInfo> stream) override {
    ADD_FAILURE() << "OnStreamCreated should not be called.";
  }

  void OnResponseStarted(net::URLRequest* request,
                         ResourceContext* resource_context,
                         network::ResourceResponse* response) override {}

  void OnRequestRedirected(const GURL& redirect_url,
                           net::URLRequest* request,
                           ResourceContext* resource_context,
                           network::ResourceResponse* response) override {
    ADD_FAILURE() << "OnRequestRedirected should not be called.";
  }

  void RequestComplete(net::URLRequest* url_request) override {
    ADD_FAILURE() << "RequestComplete should not be called.";
  }

  NavigationData* GetNavigationData(net::URLRequest* request) const override {
    ADD_FAILURE() << "GetNavigationData should not be called.";
    return nullptr;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestResourceDispatcherHostDelegate);
};

class MojoAsyncResourceHandlerWithStubOperations
    : public MojoAsyncResourceHandler {
 public:
  MojoAsyncResourceHandlerWithStubOperations(
      net::URLRequest* request,
      ResourceDispatcherHostImpl* rdh,
      network::mojom::URLLoaderRequest mojo_request,
      network::mojom::URLLoaderClientPtr url_loader_client,
      uint32_t options)
      : MojoAsyncResourceHandler(request,
                                 rdh,
                                 std::move(mojo_request),
                                 std::move(url_loader_client),
                                 RESOURCE_TYPE_MAIN_FRAME,
                                 options),
        task_runner_(new base::TestSimpleTaskRunner) {}
  ~MojoAsyncResourceHandlerWithStubOperations() override {}

  void ResetBeginWriteExpectation() { is_begin_write_expectation_set_ = false; }

  void set_begin_write_expectation(MojoResult begin_write_expectation) {
    is_begin_write_expectation_set_ = true;
    begin_write_expectation_ = begin_write_expectation;
  }
  void set_end_write_expectation(MojoResult end_write_expectation) {
    is_end_write_expectation_set_ = true;
    end_write_expectation_ = end_write_expectation;
  }
  bool has_received_bad_message() const { return has_received_bad_message_; }
  void SetMetadata(scoped_refptr<net::IOBufferWithSize> metadata) {
    metadata_ = std::move(metadata);
  }

  FakeUploadProgressTracker* upload_progress_tracker() const {
    return upload_progress_tracker_;
  }

  void PollUploadProgress() {
    task_runner_->RunPendingTasks();
    base::RunLoop().RunUntilIdle();
  }

 private:
  MojoResult BeginWrite(void** data, uint32_t* available) override {
    if (is_begin_write_expectation_set_)
      return begin_write_expectation_;
    return MojoAsyncResourceHandler::BeginWrite(data, available);
  }
  MojoResult EndWrite(uint32_t written) override {
    if (is_end_write_expectation_set_)
      return end_write_expectation_;
    return MojoAsyncResourceHandler::EndWrite(written);
  }
  net::IOBufferWithSize* GetResponseMetadata(
      net::URLRequest* request) override {
    return metadata_.get();
  }

  void ReportBadMessage(const std::string& error) override {
    has_received_bad_message_ = true;
  }

  std::unique_ptr<network::UploadProgressTracker> CreateUploadProgressTracker(
      const base::Location& from_here,
      network::UploadProgressTracker::UploadProgressReportCallback callback)
      override {
    DCHECK(!upload_progress_tracker_);

    auto upload_progress_tracker = std::make_unique<FakeUploadProgressTracker>(
        from_here, std::move(callback), request(), task_runner_);
    upload_progress_tracker_ = upload_progress_tracker.get();
    return std::move(upload_progress_tracker);
  }

  bool is_begin_write_expectation_set_ = false;
  bool is_end_write_expectation_set_ = false;
  bool has_received_bad_message_ = false;
  MojoResult begin_write_expectation_ = MOJO_RESULT_UNKNOWN;
  MojoResult end_write_expectation_ = MOJO_RESULT_UNKNOWN;
  scoped_refptr<net::IOBufferWithSize> metadata_;

  FakeUploadProgressTracker* upload_progress_tracker_ = nullptr;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(MojoAsyncResourceHandlerWithStubOperations);
};

class TestURLLoaderFactory final : public network::mojom::URLLoaderFactory {
 public:
  TestURLLoaderFactory() {}
  ~TestURLLoaderFactory() override {}

  void CreateLoaderAndStart(network::mojom::URLLoaderRequest request,
                            int32_t routing_id,
                            int32_t request_id,
                            uint32_t options,
                            const network::ResourceRequest& url_request,
                            network::mojom::URLLoaderClientPtr client_ptr,
                            const net::MutableNetworkTrafficAnnotationTag&
                                traffic_annotation) override {
    loader_request_ = std::move(request);
    client_ptr_ = std::move(client_ptr);
  }

  network::mojom::URLLoaderRequest PassLoaderRequest() {
    return std::move(loader_request_);
  }

  network::mojom::URLLoaderClientPtr PassClientPtr() {
    return std::move(client_ptr_);
  }

  void Clone(network::mojom::URLLoaderFactoryRequest request) override {
    NOTREACHED();
  }

 private:
  network::mojom::URLLoaderRequest loader_request_;
  network::mojom::URLLoaderClientPtr client_ptr_;

  DISALLOW_COPY_AND_ASSIGN(TestURLLoaderFactory);
};

class MojoAsyncResourceHandlerTestBase {
 public:
  MojoAsyncResourceHandlerTestBase(
      std::unique_ptr<net::UploadDataStream> upload_stream,
      uint32_t options)
      : thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP),
        browser_context_(new TestBrowserContext()) {
    MojoAsyncResourceHandler::SetAllocationSizeForTesting(32 * 1024);
    rdh_.SetDelegate(&rdh_delegate_);

    // Create and initialize |request_|.  None of this matters, for these tests,
    // just need something non-NULL.
    request_context_ =
        browser_context_->GetResourceContext()->GetRequestContext();
    request_ = request_context_->CreateRequest(
        GURL("http://foo/"), net::DEFAULT_PRIORITY, &url_request_delegate_,
        TRAFFIC_ANNOTATION_FOR_TESTS);
    request_->set_upload(std::move(upload_stream));
    ResourceRequestInfo::AllocateForTesting(
        request_.get(),                          // request
        RESOURCE_TYPE_XHR,                       // resource_type
        browser_context_->GetResourceContext(),  // context
        kChildId,                                // render_process_id
        kRouteId,                                // render_view_id
        0,                                       // render_frame_id
        true,                                    // is_main_frame
        false,                                   // allow_download
        true,                                    // is_async
        PREVIEWS_OFF,                            // previews_state
        nullptr);                                // navigation_ui_data

    network::ResourceRequest request;
    base::WeakPtr<mojo::StrongBinding<network::mojom::URLLoaderFactory>>
        weak_binding =
            mojo::MakeStrongBinding(std::make_unique<TestURLLoaderFactory>(),
                                    mojo::MakeRequest(&url_loader_factory_));

    url_loader_factory_->CreateLoaderAndStart(
        mojo::MakeRequest(&url_loader_proxy_), kRouteId, kRequestId,
        network::mojom::kURLLoadOptionNone, request,
        url_loader_client_.CreateInterfacePtr(),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

    url_loader_factory_.FlushForTesting();
    DCHECK(weak_binding);
    TestURLLoaderFactory* factory_impl =
        static_cast<TestURLLoaderFactory*>(weak_binding->impl());

    handler_.reset(new MojoAsyncResourceHandlerWithStubOperations(
        request_.get(), &rdh_, factory_impl->PassLoaderRequest(),
        factory_impl->PassClientPtr(), options));
    mock_loader_.reset(new MockResourceLoader(handler_.get()));
  }

  virtual ~MojoAsyncResourceHandlerTestBase() {
    MojoAsyncResourceHandler::SetAllocationSizeForTesting(
        MojoAsyncResourceHandler::kDefaultAllocationSize);
    base::RunLoop().RunUntilIdle();
  }

  // Returns false if something bad happens.
  bool CallOnWillStart() {
    MockResourceLoader::Status result =
        mock_loader_->OnWillStart(request_->url());
    EXPECT_EQ(MockResourceLoader::Status::IDLE, result);
    return result == MockResourceLoader::Status::IDLE;
  }

  // Returns false if something bad happens.
  bool CallOnResponseStarted() {
    MockResourceLoader::Status result = mock_loader_->OnResponseStarted(
        base::MakeRefCounted<network::ResourceResponse>());
    EXPECT_EQ(MockResourceLoader::Status::IDLE, result);
    if (result != MockResourceLoader::Status::IDLE)
      return false;

    if (url_loader_client_.has_received_response()) {
      ADD_FAILURE() << "URLLoaderClient unexpectedly gets a response.";
      return false;
    }
    url_loader_client_.RunUntilResponseReceived();
    return true;
  }

  // Returns false if something bad happens.
  bool CallOnWillStartAndOnResponseStarted() {
    return CallOnWillStart() && CallOnResponseStarted();
  }

  void set_upload_progress(const net::UploadProgress& upload_progress) {
    handler_->upload_progress_tracker()->upload_progress_ = upload_progress;
  }
  void AdvanceCurrentTime(const base::TimeDelta& delta) {
    handler_->upload_progress_tracker()->current_time_ += delta;
  }

  void SetupRequestSSLInfo() {
    net::CertificateList certs;
    ASSERT_TRUE(net::LoadCertificateFiles({"multi-root-B-by-C.pem"}, &certs));
    ASSERT_EQ(1U, certs.size());
    const_cast<net::SSLInfo&>(request_->ssl_info()).cert = certs[0];
  }

  TestBrowserThreadBundle thread_bundle_;
  TestResourceDispatcherHostDelegate rdh_delegate_;
  ResourceDispatcherHostImpl rdh_;
  network::mojom::URLLoaderFactoryPtr url_loader_factory_;
  network::mojom::URLLoaderPtr url_loader_proxy_;
  network::TestURLLoaderClient url_loader_client_;
  std::unique_ptr<TestBrowserContext> browser_context_;
  net::TestDelegate url_request_delegate_;
  net::URLRequestContext* request_context_;
  std::unique_ptr<net::URLRequest> request_;
  std::unique_ptr<MojoAsyncResourceHandlerWithStubOperations> handler_;
  std::unique_ptr<MockResourceLoader> mock_loader_;

  static constexpr int kChildId = 25;
  static constexpr int kRouteId = 12;
  static constexpr int kRequestId = 41;

  DISALLOW_COPY_AND_ASSIGN(MojoAsyncResourceHandlerTestBase);
};

class MojoAsyncResourceHandlerTest : public MojoAsyncResourceHandlerTestBase,
                                     public ::testing::Test {
 protected:
  MojoAsyncResourceHandlerTest()
      : MojoAsyncResourceHandlerTestBase(nullptr,
                                         network::mojom::kURLLoadOptionNone) {}
};

class MojoAsyncResourceHandlerDeferOnResponseStartedTest
    : public MojoAsyncResourceHandlerTestBase,
      public ::testing::Test {
 protected:
  MojoAsyncResourceHandlerDeferOnResponseStartedTest()
      : MojoAsyncResourceHandlerTestBase(
            nullptr,
            network::mojom::kURLLoadOptionPauseOnResponseStarted) {}
};

class MojoAsyncResourceHandlerSendSSLInfoWithResponseTest
    : public MojoAsyncResourceHandlerTestBase,
      public ::testing::Test {
 protected:
  MojoAsyncResourceHandlerSendSSLInfoWithResponseTest()
      : MojoAsyncResourceHandlerTestBase(
            nullptr,
            network::mojom::kURLLoadOptionSendSSLInfoWithResponse) {}
};

class MojoAsyncResourceHandlerSendSSLInfoForCertificateError
    : public MojoAsyncResourceHandlerTestBase,
      public ::testing::Test {
 protected:
  MojoAsyncResourceHandlerSendSSLInfoForCertificateError()
      : MojoAsyncResourceHandlerTestBase(
            nullptr,
            network::mojom::kURLLoadOptionSendSSLInfoForCertificateError) {}
};

// This test class is parameterized with MojoAsyncResourceHandler's allocation
// size.
class MojoAsyncResourceHandlerWithAllocationSizeTest
    : public MojoAsyncResourceHandlerTestBase,
      public ::testing::TestWithParam<size_t> {
 protected:
  MojoAsyncResourceHandlerWithAllocationSizeTest()
      : MojoAsyncResourceHandlerTestBase(nullptr,
                                         network::mojom::kURLLoadOptionNone) {
    MojoAsyncResourceHandler::SetAllocationSizeForTesting(GetParam());
  }
};

class MojoAsyncResourceHandlerUploadTest
    : public MojoAsyncResourceHandlerTestBase,
      public ::testing::Test {
 protected:
  MojoAsyncResourceHandlerUploadTest()
      : MojoAsyncResourceHandlerTestBase(
            std::make_unique<DummyUploadDataStream>(),
            network::mojom::kURLLoadOptionNone) {}
};

TEST_F(MojoAsyncResourceHandlerTest, InFlightRequests) {
  EXPECT_EQ(0, rdh_.num_in_flight_requests_for_testing());
  handler_ = nullptr;
  EXPECT_EQ(0, rdh_.num_in_flight_requests_for_testing());
}

TEST_F(MojoAsyncResourceHandlerTest, OnWillStart) {
  EXPECT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnWillStart(request_->url()));
}

TEST_F(MojoAsyncResourceHandlerTest, OnResponseStarted) {
  scoped_refptr<net::IOBufferWithSize> metadata =
      base::MakeRefCounted<net::IOBufferWithSize>(5);
  memcpy(metadata->data(), "hello", 5);
  handler_->SetMetadata(metadata);

  ASSERT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnWillStart(request_->url()));

  scoped_refptr<network::ResourceResponse> response =
      new network::ResourceResponse();
  response->head.content_length = 99;
  response->head.request_start =
      base::TimeTicks::UnixEpoch() + base::TimeDelta::FromDays(14);
  response->head.response_start =
      base::TimeTicks::UnixEpoch() + base::TimeDelta::FromDays(28);

  base::TimeTicks now1 = base::TimeTicks::Now();
  ASSERT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnResponseStarted(response));
  base::TimeTicks now2 = base::TimeTicks::Now();

  EXPECT_EQ(request_->creation_time(), response->head.request_start);
  EXPECT_LE(now1, response->head.response_start);
  EXPECT_LE(response->head.response_start, now2);

  url_loader_client_.RunUntilResponseReceived();
  EXPECT_EQ(response->head.request_start,
            url_loader_client_.response_head().request_start);
  EXPECT_EQ(response->head.response_start,
            url_loader_client_.response_head().response_start);
  EXPECT_EQ(99, url_loader_client_.response_head().content_length);

  url_loader_client_.RunUntilCachedMetadataReceived();
  EXPECT_EQ("hello", url_loader_client_.cached_metadata());

  EXPECT_FALSE(url_loader_client_.has_received_upload_progress());
}

TEST_F(MojoAsyncResourceHandlerTest, OnWillReadAndInFlightRequests) {
  ASSERT_TRUE(CallOnWillStartAndOnResponseStarted());
  EXPECT_EQ(0, rdh_.num_in_flight_requests_for_testing());
  ASSERT_EQ(MockResourceLoader::Status::IDLE, mock_loader_->OnWillRead());
  EXPECT_EQ(1, rdh_.num_in_flight_requests_for_testing());
  handler_ = nullptr;
  EXPECT_EQ(0, rdh_.num_in_flight_requests_for_testing());
}

TEST_F(MojoAsyncResourceHandlerTest, OnWillReadWithInsufficientResource) {
  rdh_.set_max_num_in_flight_requests_per_process(0);
  ASSERT_TRUE(CallOnWillStartAndOnResponseStarted());

  ASSERT_EQ(MockResourceLoader::Status::CANCELED, mock_loader_->OnWillRead());
  EXPECT_EQ(net::ERR_INSUFFICIENT_RESOURCES, mock_loader_->error_code());
  EXPECT_EQ(1, rdh_.num_in_flight_requests_for_testing());
  handler_ = nullptr;
  EXPECT_EQ(0, rdh_.num_in_flight_requests_for_testing());
}

TEST_F(MojoAsyncResourceHandlerTest, OnWillReadAndOnReadCompleted) {
  ASSERT_TRUE(CallOnWillStartAndOnResponseStarted());
  ASSERT_EQ(MockResourceLoader::Status::IDLE, mock_loader_->OnWillRead());
  // The buffer size that the mime sniffer requires implicitly.
  ASSERT_GE(mock_loader_->io_buffer_size(),
            kSizeMimeSnifferRequiresForFirstOnWillRead);

  ASSERT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnReadCompleted("AB"));

  url_loader_client_.RunUntilResponseBodyArrived();
  ASSERT_TRUE(url_loader_client_.response_body().is_valid());

  std::string contents;
  while (contents.size() < 2) {
    char buffer[16];
    uint32_t read_size = sizeof(buffer);
    MojoResult result = url_loader_client_.response_body().ReadData(
        buffer, &read_size, MOJO_READ_DATA_FLAG_NONE);
    if (result == MOJO_RESULT_SHOULD_WAIT) {
      base::RunLoop().RunUntilIdle();
      continue;
    }
    contents.append(buffer, read_size);
  }
  EXPECT_EQ("AB", contents);
}

TEST_F(MojoAsyncResourceHandlerTest,
       OnWillReadAndOnReadCompletedWithInsufficientInitialCapacity) {
  MojoAsyncResourceHandler::SetAllocationSizeForTesting(2);

  ASSERT_TRUE(CallOnWillStartAndOnResponseStarted());
  ASSERT_EQ(MockResourceLoader::Status::IDLE, mock_loader_->OnWillRead());
  // The buffer size that the mime sniffer requires implicitly.
  ASSERT_GE(mock_loader_->io_buffer_size(),
            kSizeMimeSnifferRequiresForFirstOnWillRead);

  const std::string data("abcdefgh");
  ASSERT_EQ(MockResourceLoader::Status::CALLBACK_PENDING,
            mock_loader_->OnReadCompleted(data));

  url_loader_client_.RunUntilResponseBodyArrived();
  ASSERT_TRUE(url_loader_client_.response_body().is_valid());

  std::string contents;
  while (contents.size() < data.size()) {
    // This is needed for Resume to be called.
    base::RunLoop().RunUntilIdle();
    char buffer[16];
    uint32_t read_size = sizeof(buffer);
    MojoResult result = url_loader_client_.response_body().ReadData(
        buffer, &read_size, MOJO_READ_DATA_FLAG_NONE);
    if (result == MOJO_RESULT_SHOULD_WAIT)
      continue;
    ASSERT_EQ(MOJO_RESULT_OK, result);
    contents.append(buffer, read_size);
  }
  EXPECT_EQ(data, contents);
  EXPECT_EQ(MockResourceLoader::Status::IDLE, mock_loader_->status());
}

TEST_F(MojoAsyncResourceHandlerTest,
       IOBufferFromOnWillReadShouldRemainValidEvenIfHandlerIsGone) {
  ASSERT_TRUE(CallOnWillStartAndOnResponseStarted());
  ASSERT_EQ(MockResourceLoader::Status::IDLE, mock_loader_->OnWillRead());
  // The io_buffer size that the mime sniffer requires implicitly.
  ASSERT_GE(mock_loader_->io_buffer_size(),
            kSizeMimeSnifferRequiresForFirstOnWillRead);

  handler_ = nullptr;
  url_loader_client_.Unbind();
  base::RunLoop().RunUntilIdle();

  // Hopefully ASAN checks this operation's validity.
  mock_loader_->io_buffer()->data()[0] = 'A';
}

TEST_F(MojoAsyncResourceHandlerTest, OnResponseCompleted) {
  ASSERT_TRUE(CallOnWillStartAndOnResponseStarted());

  net::URLRequestStatus status(net::URLRequestStatus::SUCCESS, net::OK);

  base::TimeTicks now1 = base::TimeTicks::Now();
  ASSERT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnResponseCompleted(status));
  base::TimeTicks now2 = base::TimeTicks::Now();

  url_loader_client_.RunUntilComplete();
  EXPECT_TRUE(url_loader_client_.has_received_completion());
  EXPECT_EQ(net::OK, url_loader_client_.completion_status().error_code);
  EXPECT_LE(now1, url_loader_client_.completion_status().completion_time);
  EXPECT_LE(url_loader_client_.completion_status().completion_time, now2);
  EXPECT_EQ(request_->GetTotalReceivedBytes(),
            url_loader_client_.completion_status().encoded_data_length);
}

// This test case sets different status values from OnResponseCompleted.
TEST_F(MojoAsyncResourceHandlerTest, OnResponseCompleted2) {
  rdh_.SetDelegate(nullptr);
  // Don't use CallOnWillStartAndOnResponseStarted as this test case manually
  // sets the null delegate.
  ASSERT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnWillStart(request_->url()));
  ASSERT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnResponseStarted(
                base::MakeRefCounted<network::ResourceResponse>()));
  ASSERT_FALSE(url_loader_client_.has_received_response());
  url_loader_client_.RunUntilResponseReceived();

  net::URLRequestStatus status(net::URLRequestStatus::CANCELED,
                               net::ERR_ABORTED);

  base::TimeTicks now1 = base::TimeTicks::Now();
  ASSERT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnResponseCompleted(status));
  base::TimeTicks now2 = base::TimeTicks::Now();

  url_loader_client_.RunUntilComplete();
  EXPECT_TRUE(url_loader_client_.has_received_completion());
  EXPECT_EQ(net::ERR_ABORTED,
            url_loader_client_.completion_status().error_code);
  EXPECT_LE(now1, url_loader_client_.completion_status().completion_time);
  EXPECT_LE(url_loader_client_.completion_status().completion_time, now2);
  EXPECT_EQ(request_->GetTotalReceivedBytes(),
            url_loader_client_.completion_status().encoded_data_length);
}

TEST_F(MojoAsyncResourceHandlerTest, OnResponseCompletedWithCanceledTimedOut) {
  net::URLRequestStatus status(net::URLRequestStatus::CANCELED,
                               net::ERR_TIMED_OUT);

  ASSERT_TRUE(CallOnWillStartAndOnResponseStarted());
  ASSERT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnResponseCompleted(status));

  url_loader_client_.RunUntilComplete();
  EXPECT_TRUE(url_loader_client_.has_received_completion());
  EXPECT_EQ(net::ERR_TIMED_OUT,
            url_loader_client_.completion_status().error_code);
}

TEST_F(MojoAsyncResourceHandlerTest, OnResponseCompletedWithFailedTimedOut) {
  net::URLRequestStatus status(net::URLRequestStatus::FAILED,
                               net::ERR_TIMED_OUT);

  ASSERT_TRUE(CallOnWillStartAndOnResponseStarted());
  ASSERT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnResponseCompleted(status));

  url_loader_client_.RunUntilComplete();
  EXPECT_TRUE(url_loader_client_.has_received_completion());
  EXPECT_EQ(net::ERR_TIMED_OUT,
            url_loader_client_.completion_status().error_code);
}

TEST_F(MojoAsyncResourceHandlerTest, ResponseCompletionShouldCloseDataPipe) {
  ASSERT_TRUE(CallOnWillStartAndOnResponseStarted());

  ASSERT_EQ(MockResourceLoader::Status::IDLE, mock_loader_->OnWillRead());
  ASSERT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnReadCompleted("AB"));
  url_loader_client_.RunUntilResponseBodyArrived();
  ASSERT_TRUE(url_loader_client_.response_body().is_valid());

  net::URLRequestStatus status(net::URLRequestStatus::SUCCESS, net::OK);
  ASSERT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnResponseCompleted(status));

  url_loader_client_.RunUntilComplete();
  EXPECT_TRUE(url_loader_client_.has_received_completion());
  EXPECT_EQ(net::OK, url_loader_client_.completion_status().error_code);

  while (true) {
    char buffer[16];
    uint32_t read_size = sizeof(buffer);
    MojoResult result = url_loader_client_.response_body().ReadData(
        buffer, &read_size, MOJO_READ_DATA_FLAG_NONE);
    if (result == MOJO_RESULT_FAILED_PRECONDITION)
      break;
    ASSERT_TRUE(result == MOJO_RESULT_SHOULD_WAIT || result == MOJO_RESULT_OK);
  }
}

TEST_F(MojoAsyncResourceHandlerTest, OutOfBandCancelDuringBodyTransmission) {
  ASSERT_TRUE(CallOnWillStartAndOnResponseStarted());

  ASSERT_EQ(MockResourceLoader::Status::IDLE, mock_loader_->OnWillRead());
  std::string data(mock_loader_->io_buffer_size(), 'a');
  ASSERT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnReadCompleted(data));
  ASSERT_EQ(MockResourceLoader::Status::CALLBACK_PENDING,
            mock_loader_->OnWillRead());
  url_loader_client_.RunUntilResponseBodyArrived();
  ASSERT_TRUE(url_loader_client_.response_body().is_valid());

  net::URLRequestStatus status(net::URLRequestStatus::FAILED, net::ERR_FAILED);
  ASSERT_EQ(
      MockResourceLoader::Status::IDLE,
      mock_loader_->OnResponseCompletedFromExternalOutOfBandCancel(status));

  url_loader_client_.RunUntilComplete();
  EXPECT_TRUE(url_loader_client_.has_received_completion());
  EXPECT_EQ(net::ERR_FAILED, url_loader_client_.completion_status().error_code);

  std::string actual;
  while (true) {
    char buf[16];
    uint32_t read_size = sizeof(buf);
    MojoResult result = url_loader_client_.response_body().ReadData(
        buf, &read_size, MOJO_READ_DATA_FLAG_NONE);
    if (result == MOJO_RESULT_FAILED_PRECONDITION)
      break;
    if (result == MOJO_RESULT_SHOULD_WAIT) {
      base::RunLoop().RunUntilIdle();
      continue;
    }
    EXPECT_EQ(MOJO_RESULT_OK, result);
    actual.append(buf, read_size);
  }
  EXPECT_EQ(data, actual);
}

TEST_F(MojoAsyncResourceHandlerTest, BeginWriteFailsOnWillRead) {
  handler_->set_begin_write_expectation(MOJO_RESULT_UNKNOWN);
  ASSERT_TRUE(CallOnWillStartAndOnResponseStarted());
  ASSERT_EQ(MockResourceLoader::Status::CANCELED, mock_loader_->OnWillRead());
}

TEST_F(MojoAsyncResourceHandlerTest, BeginWriteReturnsShouldWaitOnWillRead) {
  handler_->set_begin_write_expectation(MOJO_RESULT_SHOULD_WAIT);

  ASSERT_TRUE(CallOnWillStartAndOnResponseStarted());
  EXPECT_EQ(0, rdh_.num_in_flight_requests_for_testing());

  // Bytes are read one-at-a-time, and each OnWillRead() call completes
  // asynchronously. Note that this loop runs 4 times (once for the terminal
  // '\0').
  const char kReadData[] = "ABC";
  for (const char read_char : kReadData) {
    handler_->set_begin_write_expectation(MOJO_RESULT_SHOULD_WAIT);
    ASSERT_EQ(MockResourceLoader::Status::CALLBACK_PENDING,
              mock_loader_->OnWillRead());
    EXPECT_EQ(1, rdh_.num_in_flight_requests_for_testing());

    handler_->ResetBeginWriteExpectation();
    handler_->OnWritableForTesting();
    EXPECT_EQ(MockResourceLoader::Status::IDLE, mock_loader_->status());

    ASSERT_EQ(MockResourceLoader::Status::IDLE,
              mock_loader_->OnReadCompleted(std::string(1, read_char)));
    url_loader_client_.RunUntilResponseBodyArrived();

    // Keep on trying to read the data until it succeeds.
    while (true) {
      char buffer[16];
      uint32_t read_size = sizeof(buffer);
      MojoResult result = url_loader_client_.response_body().ReadData(
          buffer, &read_size, MOJO_READ_DATA_FLAG_NONE);
      if (result != MOJO_RESULT_SHOULD_WAIT) {
        ASSERT_EQ(MOJO_RESULT_OK, result);
        ASSERT_EQ(1u, read_size);
        EXPECT_EQ(read_char, buffer[0]);
        break;
      }

      base::RunLoop().RunUntilIdle();
    }
  }

  // Should only count as one in-flight request.
  EXPECT_EQ(1, rdh_.num_in_flight_requests_for_testing());

  ASSERT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnResponseCompleted(
                net::URLRequestStatus::FromError(net::OK)));

  url_loader_client_.RunUntilComplete();
  EXPECT_TRUE(url_loader_client_.has_received_completion());

  handler_.reset();
  EXPECT_EQ(0, rdh_.num_in_flight_requests_for_testing());
}

// Same as above, but after the first OnWriteable() call, BeginWrite() indicates
// should wait again. Unclear if this can happen in practice, but seems best to
// support it.
TEST_F(MojoAsyncResourceHandlerTest,
       BeginWriteReturnsShouldWaitTwiceOnWillRead) {
  handler_->set_begin_write_expectation(MOJO_RESULT_SHOULD_WAIT);

  ASSERT_TRUE(CallOnWillStartAndOnResponseStarted());
  EXPECT_EQ(0, rdh_.num_in_flight_requests_for_testing());

  // Bytes are read one-at-a-time, and each OnWillRead() call completes
  // asynchronously. Note that this loop runs 4 times (once for the terminal
  // '\0').
  const char kReadData[] = "ABC";
  for (const char read_char : kReadData) {
    handler_->set_begin_write_expectation(MOJO_RESULT_SHOULD_WAIT);
    ASSERT_EQ(MockResourceLoader::Status::CALLBACK_PENDING,
              mock_loader_->OnWillRead());
    EXPECT_EQ(1, rdh_.num_in_flight_requests_for_testing());

    handler_->OnWritableForTesting();
    EXPECT_EQ(MockResourceLoader::Status::CALLBACK_PENDING,
              mock_loader_->status());

    handler_->ResetBeginWriteExpectation();
    handler_->OnWritableForTesting();
    EXPECT_EQ(MockResourceLoader::Status::IDLE, mock_loader_->status());

    ASSERT_EQ(MockResourceLoader::Status::IDLE,
              mock_loader_->OnReadCompleted(std::string(1, read_char)));
    url_loader_client_.RunUntilResponseBodyArrived();

    // Keep on trying to read the data until it succeeds.
    while (true) {
      char buffer[16];
      uint32_t read_size = sizeof(buffer);
      MojoResult result = url_loader_client_.response_body().ReadData(
          buffer, &read_size, MOJO_READ_DATA_FLAG_NONE);
      if (result != MOJO_RESULT_SHOULD_WAIT) {
        ASSERT_EQ(MOJO_RESULT_OK, result);
        ASSERT_EQ(1u, read_size);
        EXPECT_EQ(read_char, buffer[0]);
        break;
      }

      base::RunLoop().RunUntilIdle();
    }
  }

  // Should only count as one in-flight request.
  EXPECT_EQ(1, rdh_.num_in_flight_requests_for_testing());

  ASSERT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnResponseCompleted(
                net::URLRequestStatus::FromError(net::OK)));

  url_loader_client_.RunUntilComplete();
  EXPECT_TRUE(url_loader_client_.has_received_completion());

  handler_.reset();
  EXPECT_EQ(0, rdh_.num_in_flight_requests_for_testing());
}

TEST_F(MojoAsyncResourceHandlerTest,
       EndWriteFailsOnWillReadWithInsufficientInitialCapacity) {
  MojoAsyncResourceHandler::SetAllocationSizeForTesting(2);
  ASSERT_TRUE(CallOnWillStartAndOnResponseStarted());
  handler_->set_end_write_expectation(MOJO_RESULT_UNKNOWN);
  ASSERT_EQ(MockResourceLoader::Status::CANCELED, mock_loader_->OnWillRead());
}

TEST_F(MojoAsyncResourceHandlerTest, EndWriteFailsOnReadCompleted) {
  ASSERT_TRUE(CallOnWillStartAndOnResponseStarted());
  ASSERT_EQ(MockResourceLoader::Status::IDLE, mock_loader_->OnWillRead());

  handler_->set_end_write_expectation(MOJO_RESULT_SHOULD_WAIT);
  ASSERT_EQ(MockResourceLoader::Status::CANCELED,
            mock_loader_->OnReadCompleted(
                std::string(mock_loader_->io_buffer_size(), 'w')));
}

TEST_F(MojoAsyncResourceHandlerTest,
       EndWriteFailsOnReadCompletedWithInsufficientInitialCapacity) {
  MojoAsyncResourceHandler::SetAllocationSizeForTesting(2);
  ASSERT_TRUE(CallOnWillStartAndOnResponseStarted());
  ASSERT_EQ(MockResourceLoader::Status::IDLE, mock_loader_->OnWillRead());

  handler_->set_end_write_expectation(MOJO_RESULT_SHOULD_WAIT);
  ASSERT_EQ(MockResourceLoader::Status::CANCELED,
            mock_loader_->OnReadCompleted(
                std::string(mock_loader_->io_buffer_size(), 'w')));
}

TEST_F(MojoAsyncResourceHandlerTest,
       EndWriteFailsOnResumeWithInsufficientInitialCapacity) {
  MojoAsyncResourceHandler::SetAllocationSizeForTesting(8);
  ASSERT_TRUE(CallOnWillStartAndOnResponseStarted());
  ASSERT_EQ(MockResourceLoader::Status::IDLE, mock_loader_->OnWillRead());

  while (true) {
    MockResourceLoader::Status result = mock_loader_->OnReadCompleted(
        std::string(mock_loader_->io_buffer_size(), 'A'));
    if (result == MockResourceLoader::Status::CALLBACK_PENDING)
      break;
    ASSERT_EQ(MockResourceLoader::Status::IDLE, result);

    ASSERT_EQ(MockResourceLoader::Status::IDLE, mock_loader_->OnWillRead());
  }

  url_loader_client_.RunUntilResponseBodyArrived();
  ASSERT_TRUE(url_loader_client_.response_body().is_valid());

  while (true) {
    char buf[16];
    uint32_t read_size = sizeof(buf);
    MojoResult result = url_loader_client_.response_body().ReadData(
        buf, &read_size, MOJO_READ_DATA_FLAG_NONE);
    if (result == MOJO_RESULT_SHOULD_WAIT)
      break;
    ASSERT_EQ(MOJO_RESULT_OK, result);
  }

  handler_->set_end_write_expectation(MOJO_RESULT_SHOULD_WAIT);
  mock_loader_->WaitUntilIdleOrCanceled();
  EXPECT_FALSE(url_loader_client_.has_received_completion());
  EXPECT_EQ(MockResourceLoader::Status::CANCELED, mock_loader_->status());
  EXPECT_EQ(net::ERR_INSUFFICIENT_RESOURCES, mock_loader_->error_code());
}

TEST_F(MojoAsyncResourceHandlerUploadTest, UploadProgressHandling) {
  ASSERT_TRUE(CallOnWillStart());

  // Expect no report for no progress.
  set_upload_progress(net::UploadProgress(0, 1000));
  handler_->PollUploadProgress();
  EXPECT_FALSE(url_loader_client_.has_received_upload_progress());
  EXPECT_EQ(0, url_loader_client_.current_upload_position());
  EXPECT_EQ(0, url_loader_client_.total_upload_size());

  // Expect a upload progress report for a good amount of progress.
  url_loader_client_.reset_has_received_upload_progress();
  set_upload_progress(net::UploadProgress(100, 1000));
  handler_->PollUploadProgress();
  EXPECT_TRUE(url_loader_client_.has_received_upload_progress());
  EXPECT_EQ(100, url_loader_client_.current_upload_position());
  EXPECT_EQ(1000, url_loader_client_.total_upload_size());

  // Expect a upload progress report for the passed time.
  url_loader_client_.reset_has_received_upload_progress();
  set_upload_progress(net::UploadProgress(101, 1000));
  AdvanceCurrentTime(base::TimeDelta::FromSeconds(5));
  handler_->PollUploadProgress();
  EXPECT_TRUE(url_loader_client_.has_received_upload_progress());
  EXPECT_EQ(101, url_loader_client_.current_upload_position());
  EXPECT_EQ(1000, url_loader_client_.total_upload_size());

  // A redirect rewinds the upload progress. Expect no report for the rewound
  // progress.
  url_loader_client_.reset_has_received_upload_progress();
  set_upload_progress(net::UploadProgress(0, 1000));
  AdvanceCurrentTime(base::TimeDelta::FromSeconds(5));
  handler_->PollUploadProgress();
  EXPECT_FALSE(url_loader_client_.has_received_upload_progress());

  // Set the progress to almost-finished state to prepare for the completion
  // report below.
  url_loader_client_.reset_has_received_upload_progress();
  set_upload_progress(net::UploadProgress(999, 1000));
  handler_->PollUploadProgress();
  EXPECT_TRUE(url_loader_client_.has_received_upload_progress());
  EXPECT_EQ(999, url_loader_client_.current_upload_position());
  EXPECT_EQ(1000, url_loader_client_.total_upload_size());

  // Expect a upload progress report for the upload completion.
  url_loader_client_.reset_has_received_upload_progress();
  set_upload_progress(net::UploadProgress(1000, 1000));
  ASSERT_TRUE(CallOnResponseStarted());
  EXPECT_TRUE(url_loader_client_.has_received_upload_progress());
  EXPECT_EQ(1000, url_loader_client_.current_upload_position());
  EXPECT_EQ(1000, url_loader_client_.total_upload_size());
}

TEST_F(MojoAsyncResourceHandlerTest, SetPriority) {
  constexpr int kIntraPriority = 5;
  ASSERT_TRUE(CallOnWillStartAndOnResponseStarted());
  auto throttle =
      ResourceDispatcherHostImpl::Get()->scheduler()->ScheduleRequest(
          kChildId, kRouteId, false, request_.get());

  EXPECT_EQ(net::LOWEST, request_->priority());

  handler_->SetPriority(net::RequestPriority::HIGHEST, kIntraPriority);

  EXPECT_EQ(net::HIGHEST, request_->priority());
}

TEST_P(MojoAsyncResourceHandlerWithAllocationSizeTest,
       OnWillReadWithLongContents) {
  ASSERT_TRUE(CallOnWillStartAndOnResponseStarted());

  std::string expected;
  for (int i = 0; i < 3 * mock_loader_->io_buffer_size() + 2; ++i)
    expected += ('A' + i % 26);

  size_t written = 0;
  std::string actual;
  while (actual.size() < expected.size()) {
    while (written < expected.size() &&
           mock_loader_->status() == MockResourceLoader::Status::IDLE) {
      ASSERT_EQ(MockResourceLoader::Status::IDLE, mock_loader_->OnWillRead());
      size_t to_be_written =
          std::min(static_cast<size_t>(mock_loader_->io_buffer_size()),
                   expected.size() - written);

      // Request should be resumed or paused.
      ASSERT_NE(MockResourceLoader::Status::CANCELED,
                mock_loader_->OnReadCompleted(
                    expected.substr(written, to_be_written)));

      written += to_be_written;
    }
    if (!url_loader_client_.response_body().is_valid()) {
      url_loader_client_.RunUntilResponseBodyArrived();
      ASSERT_TRUE(url_loader_client_.response_body().is_valid());
    }

    char buf[16];
    uint32_t read_size = sizeof(buf);
    MojoResult result = url_loader_client_.response_body().ReadData(
        buf, &read_size, MOJO_READ_DATA_FLAG_NONE);
    if (result != MOJO_RESULT_SHOULD_WAIT) {
      ASSERT_EQ(MOJO_RESULT_OK, result);
      actual.append(buf, read_size);
    }

    // Give mojo a chance pass data back and forth, and to request more data
    // from the handler.
    base::RunLoop().RunUntilIdle();
  }
  EXPECT_EQ(expected, actual);
}

TEST_P(MojoAsyncResourceHandlerWithAllocationSizeTest,
       BeginWriteFailsOnReadCompleted) {
  ASSERT_TRUE(CallOnWillStartAndOnResponseStarted());
  ASSERT_EQ(MockResourceLoader::Status::IDLE, mock_loader_->OnWillRead());

  // Whether the next OnReadCompleted call or OnWillRead returns the error
  // depends on whether or not an intermediary buffer is being used by the
  // MojoAsyncResourceHandler.
  handler_->set_begin_write_expectation(MOJO_RESULT_UNKNOWN);
  MockResourceLoader::Status result = mock_loader_->OnReadCompleted(
      std::string(mock_loader_->io_buffer_size(), 'A'));
  if (result == MockResourceLoader::Status::CANCELED)
    return;
  ASSERT_EQ(MockResourceLoader::Status::IDLE, result);
  ASSERT_EQ(MockResourceLoader::Status::CANCELED, mock_loader_->OnWillRead());
}

TEST_P(MojoAsyncResourceHandlerWithAllocationSizeTest,
       BeginWriteReturnsShouldWaitOnReadCompleted) {
  ASSERT_TRUE(CallOnWillStartAndOnResponseStarted());
  ASSERT_EQ(MockResourceLoader::Status::IDLE, mock_loader_->OnWillRead());

  // Whether the next OnReadCompleted call or OnWillRead call completes
  // asynchronously depends on whether or not an intermediary buffer is being
  // used by the MojoAsyncResourceHandler.
  handler_->set_begin_write_expectation(MOJO_RESULT_SHOULD_WAIT);
  MockResourceLoader::Status result = mock_loader_->OnReadCompleted(
      std::string(mock_loader_->io_buffer_size() - 1, 'A'));
  if (result == MockResourceLoader::Status::CALLBACK_PENDING)
    return;

  ASSERT_EQ(MockResourceLoader::Status::CALLBACK_PENDING,
            mock_loader_->OnWillRead());
}

TEST_P(MojoAsyncResourceHandlerWithAllocationSizeTest,
       BeginWriteFailsOnResume) {
  ASSERT_TRUE(CallOnWillStartAndOnResponseStarted());

  while (true) {
    // Whether the next OnReadCompleted call or OnWillRead call completes
    // asynchronously depends on whether or not an intermediary buffer is being
    // used by the MojoAsyncResourceHandler.
    MockResourceLoader::Status result = mock_loader_->OnWillRead();
    if (result == MockResourceLoader::Status::CALLBACK_PENDING)
      break;
    ASSERT_EQ(MockResourceLoader::Status::IDLE, result);
    result = mock_loader_->OnReadCompleted(
        std::string(mock_loader_->io_buffer_size(), 'A'));
    if (result == MockResourceLoader::Status::CALLBACK_PENDING)
      break;
    ASSERT_EQ(MockResourceLoader::Status::IDLE, result);
  }
  url_loader_client_.RunUntilResponseBodyArrived();
  ASSERT_TRUE(url_loader_client_.response_body().is_valid());

  handler_->set_begin_write_expectation(MOJO_RESULT_UNKNOWN);

  while (mock_loader_->status() != MockResourceLoader::Status::CANCELED) {
    char buf[256];
    uint32_t read_size = sizeof(buf);
    MojoResult result = url_loader_client_.response_body().ReadData(
        buf, &read_size, MOJO_READ_DATA_FLAG_NONE);
    ASSERT_TRUE(result == MOJO_RESULT_OK || result == MOJO_RESULT_SHOULD_WAIT);
    base::RunLoop().RunUntilIdle();
  }

  if (mock_loader_->status() == MockResourceLoader::Status::IDLE)
    EXPECT_EQ(MockResourceLoader::Status::CANCELED, mock_loader_->OnWillRead());

  EXPECT_FALSE(url_loader_client_.has_received_completion());
  EXPECT_EQ(net::ERR_INSUFFICIENT_RESOURCES, mock_loader_->error_code());
}

TEST_P(MojoAsyncResourceHandlerWithAllocationSizeTest, CancelWhileWaiting) {
  ASSERT_TRUE(CallOnWillStartAndOnResponseStarted());

  while (true) {
    // Whether the next OnReadCompleted call or OnWillRead call completes
    // asynchronously depends on whether or not an intermediary buffer is being
    // used by the MojoAsyncResourceHandler.
    MockResourceLoader::Status result = mock_loader_->OnWillRead();
    if (result == MockResourceLoader::Status::CALLBACK_PENDING)
      break;
    ASSERT_EQ(MockResourceLoader::Status::IDLE, result);
    result = mock_loader_->OnReadCompleted(
        std::string(mock_loader_->io_buffer_size(), 'A'));
    if (result == MockResourceLoader::Status::CALLBACK_PENDING)
      break;
    ASSERT_EQ(MockResourceLoader::Status::IDLE, result);
  }

  url_loader_client_.RunUntilResponseBodyArrived();
  ASSERT_TRUE(url_loader_client_.response_body().is_valid());

  net::URLRequestStatus status(net::URLRequestStatus::CANCELED,
                               net::ERR_ABORTED);
  ASSERT_EQ(
      MockResourceLoader::Status::IDLE,
      mock_loader_->OnResponseCompletedFromExternalOutOfBandCancel(status));

  ASSERT_FALSE(url_loader_client_.has_received_completion());
  url_loader_client_.RunUntilComplete();
  EXPECT_EQ(net::ERR_ABORTED,
            url_loader_client_.completion_status().error_code);

  while (true) {
    char buffer[16];
    uint32_t read_size = sizeof(buffer);
    MojoResult result = url_loader_client_.response_body().ReadData(
        buffer, &read_size, MOJO_READ_DATA_FLAG_NONE);
    if (result == MOJO_RESULT_FAILED_PRECONDITION)
      break;
    base::RunLoop().RunUntilIdle();
    DCHECK(result == MOJO_RESULT_SHOULD_WAIT || result == MOJO_RESULT_OK);
  }

  base::RunLoop().RunUntilIdle();
}

TEST_P(MojoAsyncResourceHandlerWithAllocationSizeTest, RedirectHandling) {
  ASSERT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnWillStart(request_->url()));

  net::RedirectInfo redirect_info;
  redirect_info.status_code = 301;
  ASSERT_EQ(
      MockResourceLoader::Status::CALLBACK_PENDING,
      mock_loader_->OnRequestRedirected(
          redirect_info, base::MakeRefCounted<network::ResourceResponse>()));

  ASSERT_FALSE(url_loader_client_.has_received_response());
  ASSERT_FALSE(url_loader_client_.has_received_redirect());
  url_loader_client_.RunUntilRedirectReceived();

  ASSERT_FALSE(url_loader_client_.has_received_response());
  ASSERT_TRUE(url_loader_client_.has_received_redirect());
  EXPECT_EQ(301, url_loader_client_.redirect_info().status_code);

  ASSERT_EQ(MockResourceLoader::Status::CALLBACK_PENDING,
            mock_loader_->status());
  handler_->FollowRedirect(base::nullopt, base::nullopt);
  ASSERT_EQ(MockResourceLoader::Status::IDLE, mock_loader_->status());

  url_loader_client_.ClearHasReceivedRedirect();
  // Redirect once more.
  redirect_info.status_code = 302;
  ASSERT_EQ(
      MockResourceLoader::Status::CALLBACK_PENDING,
      mock_loader_->OnRequestRedirected(
          redirect_info, base::MakeRefCounted<network::ResourceResponse>()));

  ASSERT_FALSE(url_loader_client_.has_received_response());
  ASSERT_FALSE(url_loader_client_.has_received_redirect());
  url_loader_client_.RunUntilRedirectReceived();

  ASSERT_FALSE(url_loader_client_.has_received_response());
  ASSERT_TRUE(url_loader_client_.has_received_redirect());
  EXPECT_EQ(302, url_loader_client_.redirect_info().status_code);

  ASSERT_EQ(MockResourceLoader::Status::CALLBACK_PENDING,
            mock_loader_->status());
  handler_->FollowRedirect(base::nullopt, base::nullopt);
  ASSERT_EQ(MockResourceLoader::Status::IDLE, mock_loader_->status());

  // Give the final response.
  ASSERT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnResponseStarted(
                base::MakeRefCounted<network::ResourceResponse>()));

  net::URLRequestStatus status(net::URLRequestStatus::SUCCESS, net::OK);
  ASSERT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnResponseCompleted(status));

  ASSERT_FALSE(url_loader_client_.has_received_completion());
  url_loader_client_.RunUntilComplete();

  ASSERT_TRUE(url_loader_client_.has_received_response());
  ASSERT_TRUE(url_loader_client_.has_received_completion());
  EXPECT_EQ(net::OK, url_loader_client_.completion_status().error_code);
}

// Test the case where th other process tells the ResourceHandler to follow a
// redirect, despite the fact that no redirect has been received yet.
TEST_P(MojoAsyncResourceHandlerWithAllocationSizeTest,
       MalformedFollowRedirectRequest) {
  handler_->FollowRedirect(base::nullopt, base::nullopt);

  EXPECT_TRUE(handler_->has_received_bad_message());
}

// Typically ResourceHandler methods are called in this order.
TEST_P(
    MojoAsyncResourceHandlerWithAllocationSizeTest,
    OnWillStartThenOnResponseStartedThenOnWillReadThenOnReadCompletedThenOnResponseCompleted) {

  ASSERT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnWillStart(request_->url()));
  ASSERT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnResponseStarted(
                base::MakeRefCounted<network::ResourceResponse>()));

  ASSERT_FALSE(url_loader_client_.has_received_response());
  url_loader_client_.RunUntilResponseReceived();

  ASSERT_EQ(MockResourceLoader::Status::IDLE, mock_loader_->OnWillRead());

  ASSERT_FALSE(url_loader_client_.response_body().is_valid());

  ASSERT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnReadCompleted("A"));
  url_loader_client_.RunUntilResponseBodyArrived();
  ASSERT_TRUE(url_loader_client_.response_body().is_valid());

  net::URLRequestStatus status(net::URLRequestStatus::SUCCESS, net::OK);
  ASSERT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnResponseCompleted(status));

  ASSERT_FALSE(url_loader_client_.has_received_completion());
  url_loader_client_.RunUntilComplete();
  EXPECT_EQ(net::OK, url_loader_client_.completion_status().error_code);

  std::string body;
  while (true) {
    char buffer[16];
    uint32_t read_size = sizeof(buffer);
    MojoResult result = url_loader_client_.response_body().ReadData(
        buffer, &read_size, MOJO_READ_DATA_FLAG_NONE);
    if (result == MOJO_RESULT_FAILED_PRECONDITION)
      break;
    if (result == MOJO_RESULT_SHOULD_WAIT) {
      base::RunLoop().RunUntilIdle();
    } else {
      ASSERT_EQ(result, MOJO_RESULT_OK);
      body.append(buffer, read_size);
    }
  }
  EXPECT_EQ("A", body);
}

// MimeResourceHandler calls delegated ResourceHandler's methods in this order.
TEST_P(
    MojoAsyncResourceHandlerWithAllocationSizeTest,
    OnWillStartThenOnWillReadThenOnResponseStartedThenOnReadCompletedThenOnResponseCompleted) {

  ASSERT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnWillStart(request_->url()));

  ASSERT_EQ(MockResourceLoader::Status::IDLE, mock_loader_->OnWillRead());

  ASSERT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnResponseStarted(
                base::MakeRefCounted<network::ResourceResponse>()));

  ASSERT_FALSE(url_loader_client_.has_received_response());
  url_loader_client_.RunUntilResponseReceived();

  ASSERT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnReadCompleted("B"));

  ASSERT_FALSE(url_loader_client_.response_body().is_valid());
  url_loader_client_.RunUntilResponseBodyArrived();
  ASSERT_TRUE(url_loader_client_.response_body().is_valid());

  net::URLRequestStatus status(net::URLRequestStatus::SUCCESS, net::OK);
  ASSERT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnResponseCompleted(status));

  ASSERT_FALSE(url_loader_client_.has_received_completion());
  url_loader_client_.RunUntilComplete();
  EXPECT_EQ(net::OK, url_loader_client_.completion_status().error_code);

  std::string body;
  while (true) {
    char buffer[16];
    uint32_t read_size = sizeof(buffer);
    MojoResult result = url_loader_client_.response_body().ReadData(
        buffer, &read_size, MOJO_READ_DATA_FLAG_NONE);
    if (result == MOJO_RESULT_FAILED_PRECONDITION)
      break;
    if (result == MOJO_RESULT_SHOULD_WAIT) {
      base::RunLoop().RunUntilIdle();
    } else {
      ASSERT_EQ(result, MOJO_RESULT_OK);
      body.append(buffer, read_size);
    }
  }
  EXPECT_EQ("B", body);
}

TEST_F(MojoAsyncResourceHandlerDeferOnResponseStartedTest,
       ProceedWithResponse) {
  EXPECT_TRUE(CallOnWillStart());

  // On response started, the MojoAsyncResourceHandler should stop loading,
  // since |defer_on_response_started| is true.
  {
    MockResourceLoader::Status result = mock_loader_->OnResponseStarted(
        base::MakeRefCounted<network::ResourceResponse>());
    EXPECT_EQ(MockResourceLoader::Status::CALLBACK_PENDING, result);
    std::unique_ptr<base::Value> request_state = request_->GetStateAsValue();
    base::Value* delegate_blocked_by =
        request_state->FindKey("delegate_blocked_by");
    EXPECT_TRUE(delegate_blocked_by);
    EXPECT_EQ("MojoAsyncResourceHandler", delegate_blocked_by->GetString());
  }

  // When ProceedWithResponse() is called, the MojoAsyncResourceHandler should
  // resume its controller.
  {
    handler_->ProceedWithResponse();
    mock_loader_->WaitUntilIdleOrCanceled();
    EXPECT_EQ(MockResourceLoader::Status::IDLE, mock_loader_->status());
    std::unique_ptr<base::Value> request_state = request_->GetStateAsValue();
    base::Value* delegate_blocked_by =
        request_state->FindKey("delegate_blocked_by");
    EXPECT_FALSE(delegate_blocked_by);
  }
}

// Test that SSLInfo is not attached to OnResponseStarted when there is no
// kURLLoadOptionsSendSSLInfoWithResponse option.
TEST_F(MojoAsyncResourceHandlerTest, SSLInfoOnResponseStarted) {
  SetupRequestSSLInfo();
  EXPECT_TRUE(CallOnWillStartAndOnResponseStarted());
  EXPECT_FALSE(url_loader_client_.ssl_info());
}

// Test that SSLInfo is attached to OnResponseStarted when there is a
// kURLLoadOptionsSendSSLInfoWithResponse option.
TEST_F(MojoAsyncResourceHandlerSendSSLInfoWithResponseTest,
       SSLInfoOnResponseStarted) {
  SetupRequestSSLInfo();
  EXPECT_TRUE(CallOnWillStartAndOnResponseStarted());
  EXPECT_TRUE(url_loader_client_.ssl_info());
}

// Test that SSLInfo is not attached to OnResponseComplete when there is no
// kURLLoadOptionsSendSSLInfoForCertificateError option.
TEST_F(MojoAsyncResourceHandlerTest, SSLInfoOnComplete) {
  EXPECT_TRUE(CallOnWillStart());

  // Simulates the request getting a major SSL error.
  const_cast<net::SSLInfo&>(request_->ssl_info()).cert_status =
      net::CERT_STATUS_AUTHORITY_INVALID;
  ASSERT_EQ(
      MockResourceLoader::Status::IDLE,
      mock_loader_->OnResponseCompleted(net::URLRequestStatus(
          net::URLRequestStatus::CANCELED, net::ERR_CERT_AUTHORITY_INVALID)));

  url_loader_client_.RunUntilComplete();
  EXPECT_FALSE(url_loader_client_.completion_status().ssl_info);
};

// Test that SSLInfo is attached to OnResponseComplete when there is the
// kURLLoadOptionsSendSSLInfoForCertificateError option.
TEST_F(MojoAsyncResourceHandlerSendSSLInfoForCertificateError,
       SSLInfoOnCompleteMajorError) {
  EXPECT_TRUE(CallOnWillStart());

  // Simulates the request getting a major SSL error.
  const_cast<net::SSLInfo&>(request_->ssl_info()).cert_status =
      net::CERT_STATUS_AUTHORITY_INVALID;
  ASSERT_EQ(
      MockResourceLoader::Status::IDLE,
      mock_loader_->OnResponseCompleted(net::URLRequestStatus(
          net::URLRequestStatus::CANCELED, net::ERR_CERT_AUTHORITY_INVALID)));

  url_loader_client_.RunUntilComplete();
  EXPECT_TRUE(url_loader_client_.completion_status().ssl_info);
};

// Test that SSLInfo is not attached to OnResponseComplete when there is the
// kURLLoadOptionsSendSSLInfoForCertificateError option and a minor SSL error.
TEST_F(MojoAsyncResourceHandlerSendSSLInfoForCertificateError,
       SSLInfoOnCompleteMinorError) {
  EXPECT_TRUE(CallOnWillStart());

  // Simulates the request getting a minor SSL error.
  const_cast<net::SSLInfo&>(request_->ssl_info()).cert_status =
      net::CERT_STATUS_UNABLE_TO_CHECK_REVOCATION;

  EXPECT_TRUE(CallOnResponseStarted());
  ASSERT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnResponseCompleted(net::URLRequestStatus()));
  url_loader_client_.RunUntilComplete();
  EXPECT_FALSE(url_loader_client_.completion_status().ssl_info);
};

TEST_F(MojoAsyncResourceHandlerTest,
       TransferSizeUpdateCalledForNonBlockedResponse) {
  net::URLRequestJobFactoryImpl test_job_factory_;
  auto test_job = std::make_unique<net::URLRequestTestJob>(
      request_.get(), request_context_->network_delegate(), "response headers",
      "response body", true);
  auto test_job_interceptor = std::make_unique<net::TestJobInterceptor>();
  net::TestJobInterceptor* raw_test_job_interceptor =
      test_job_interceptor.get();
  EXPECT_TRUE(test_job_factory_.SetProtocolHandler(
      url::kHttpScheme, std::move(test_job_interceptor)));

  request_context_->set_job_factory(&test_job_factory_);
  raw_test_job_interceptor->set_main_intercept_job(std::move(test_job));
  request_->Start();

  ASSERT_TRUE(CallOnWillStartAndOnResponseStarted());
  url_request_delegate_.RunUntilComplete();

  net::URLRequestStatus status(net::URLRequestStatus::SUCCESS, net::OK);
  ASSERT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnResponseCompleted(status));
  url_loader_client_.RunUntilComplete();
  EXPECT_LT(0, url_loader_client_.body_transfer_size());
  EXPECT_EQ(request_->GetTotalReceivedBytes(),
            url_loader_client_.body_transfer_size());
}

TEST_F(MojoAsyncResourceHandlerTest,
       TransferSizeUpdateNotCalledForBlockedResponse) {
  net::URLRequestJobFactoryImpl test_job_factory_;
  auto test_job = std::make_unique<net::URLRequestTestJob>(
      request_.get(), request_context_->network_delegate(), "response headers",
      "response body", true);
  auto test_job_interceptor = std::make_unique<net::TestJobInterceptor>();
  net::TestJobInterceptor* raw_test_job_interceptor =
      test_job_interceptor.get();
  EXPECT_TRUE(test_job_factory_.SetProtocolHandler(
      url::kHttpScheme, std::move(test_job_interceptor)));

  request_context_->set_job_factory(&test_job_factory_);
  raw_test_job_interceptor->set_main_intercept_job(std::move(test_job));
  request_->Start();

  // Block the response to reach renderer.
  ResourceRequestInfoImpl::ForRequest(request_.get())
      ->set_blocked_response_from_reaching_renderer(true);

  ASSERT_TRUE(CallOnWillStartAndOnResponseStarted());

  net::URLRequestStatus status(net::URLRequestStatus::SUCCESS, net::OK);
  ASSERT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnResponseCompleted(status));
  url_request_delegate_.RunUntilComplete();
  EXPECT_TRUE(ResourceRequestInfoImpl::ForRequest(request_.get())
                  ->blocked_response_from_reaching_renderer());
  EXPECT_EQ(0, url_loader_client_.body_transfer_size());
  EXPECT_LT(0, request_->GetTotalReceivedBytes());
}

TEST_F(MojoAsyncResourceHandlerTest,
       TransferSizeUpdateCalledWithoutResponseComplete) {
  const char kResponseHeaders[] = "response headers";
  const char kResponseData[] = "response data";
  // Create a mock timer to control when the final transfersizeupdate is sent.
  auto timer = std::make_unique<base::MockOneShotTimer>();
  auto* raw_timer = timer.get();
  handler_->set_report_transfer_size_async_timer_for_testing(std::move(timer));

  // Create a test job so the underlying URLRequest will receive bytes.
  net::URLRequestJobFactoryImpl test_job_factory_;
  auto test_job = std::make_unique<net::URLRequestTestJob>(
      request_.get(), request_context_->network_delegate(), kResponseHeaders,
      kResponseData, true);
  auto test_job_interceptor = std::make_unique<net::TestJobInterceptor>();
  net::TestJobInterceptor* raw_test_job_interceptor =
      test_job_interceptor.get();
  EXPECT_TRUE(test_job_factory_.SetProtocolHandler(
      url::kHttpScheme, std::move(test_job_interceptor)));
  request_context_->set_job_factory(&test_job_factory_);
  raw_test_job_interceptor->set_main_intercept_job(std::move(test_job));
  request_->Start();

  // Prepare for loader read complete.
  ASSERT_TRUE(CallOnWillStartAndOnResponseStarted());
  EXPECT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnWillStart(request_->url()));
  EXPECT_EQ(MockResourceLoader::Status::IDLE, mock_loader_->OnWillRead());
  // Only headers are read by the time the response is started.
  mock_loader_->OnReadCompleted(kResponseHeaders);

  // Make the loader process another read of the rest of the URLTestJob data.
  EXPECT_EQ(MockResourceLoader::Status::IDLE, mock_loader_->OnWillRead());
  mock_loader_->OnReadCompleted(kResponseData);

  // Process the entire URL request.
  url_request_delegate_.RunUntilComplete();

  // All data received by the request.
  EXPECT_EQ(
      request_->GetTotalReceivedBytes(),
      static_cast<int64_t>(strlen(kResponseHeaders) + strlen(kResponseData)));

  // Wait for a transfer size update to be received.
  url_loader_client_.RunUntilTransferSizeUpdated();
  // Only the first read caused a transfer size update.
  EXPECT_EQ(static_cast<int64_t>(strlen(kResponseHeaders)),
            url_loader_client_.body_transfer_size());
  // Firing the timer will cause the rest of the bytes to be reported.
  // Without timer fire no transfer size updates would be received.
  raw_timer->Fire();
  url_loader_client_.RunUntilTransferSizeUpdated();
  EXPECT_EQ(request_->GetTotalReceivedBytes(),
            url_loader_client_.body_transfer_size());
}

INSTANTIATE_TEST_CASE_P(MojoAsyncResourceHandlerWithAllocationSizeTest,
                        MojoAsyncResourceHandlerWithAllocationSizeTest,
                        ::testing::Values(8, 32 * 2014));
}  // namespace
}  // namespace content
