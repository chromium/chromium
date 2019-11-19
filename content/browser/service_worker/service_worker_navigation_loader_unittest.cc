// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_navigation_loader.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "content/browser/loader/navigation_loader_interceptor.h"
#include "content/browser/loader/single_request_url_loader_factory.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/fake_embedded_worker_instance_client.h"
#include "content/browser/service_worker/fake_service_worker.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_process_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "net/ssl/ssl_info.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "services/network/public/cpp/resource_response.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/test/test_url_loader_client.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_impl.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/service_worker/service_worker.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_event_status.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"

namespace content {
namespace service_worker_navigation_loader_unittest {

void ReceiveRequestHandler(
    SingleRequestURLLoaderFactory::RequestHandler* out_handler,
    SingleRequestURLLoaderFactory::RequestHandler handler) {
  *out_handler = std::move(handler);
}

blink::mojom::FetchAPIResponsePtr OkResponse(
    blink::mojom::SerializedBlobPtr blob_body) {
  auto response = blink::mojom::FetchAPIResponse::New();
  response->status_code = 200;
  response->status_text = "OK";
  response->response_type = network::mojom::FetchResponseType::kDefault;
  response->blob = std::move(blob_body);
  if (response->blob) {
    response->headers.emplace("Content-Length",
                              base::NumberToString(response->blob->size));
  }
  return response;
}

blink::mojom::FetchAPIResponsePtr ErrorResponse() {
  auto response = blink::mojom::FetchAPIResponse::New();
  response->status_code = 0;
  response->response_type = network::mojom::FetchResponseType::kDefault;
  response->error = blink::mojom::ServiceWorkerResponseError::kPromiseRejected;
  return response;
}

blink::mojom::FetchAPIResponsePtr RedirectResponse(
    const std::string& redirect_location_header) {
  auto response = blink::mojom::FetchAPIResponse::New();
  response->status_code = 301;
  response->status_text = "Moved Permanently";
  response->response_type = network::mojom::FetchResponseType::kDefault;
  response->headers["Location"] = redirect_location_header;
  return response;
}

// Simulates a service worker handling fetch events. The response can be
// customized via RespondWith* functions.
class FetchEventServiceWorker : public FakeServiceWorker {
 public:
  FetchEventServiceWorker(
      EmbeddedWorkerTestHelper* helper,
      FakeEmbeddedWorkerInstanceClient* embedded_worker_instance_client)
      : FakeServiceWorker(helper),
        embedded_worker_instance_client_(embedded_worker_instance_client) {}
  ~FetchEventServiceWorker() override = default;

  // Tells this worker to respond to fetch events with the specified blob.
  void RespondWithBlob(blink::mojom::SerializedBlobPtr blob) {
    response_mode_ = ResponseMode::kBlob;
    blob_body_ = std::move(blob);
  }

  // Tells this worker to respond to fetch events with the specified stream.
  void RespondWithStream(
      mojo::PendingReceiver<blink::mojom::ServiceWorkerStreamCallback>
          callback_receiver,
      mojo::ScopedDataPipeConsumerHandle consumer_handle) {
    response_mode_ = ResponseMode::kStream;
    stream_handle_ = blink::mojom::ServiceWorkerStreamHandle::New();
    stream_handle_->callback_receiver = std::move(callback_receiver);
    stream_handle_->stream = std::move(consumer_handle);
  }

  // Tells this worker to respond to fetch events with network fallback.
  // i.e., simulate the service worker not calling respondWith().
  void RespondWithFallback() {
    response_mode_ = ResponseMode::kFallbackResponse;
  }

  // Tells this worker to respond to fetch events with an error response.
  void RespondWithError() { response_mode_ = ResponseMode::kErrorResponse; }

  // Tells this worker to respond to fetch events with the redirect response.
  void RespondWithRedirectResponse(const GURL& new_url) {
    response_mode_ = ResponseMode::kRedirect;
    redirected_url_ = new_url;
  }

  // Tells this worker to simulate failure to dispatch the fetch event to the
  // service worker.
  void FailToDispatchFetchEvent() {
    response_mode_ = ResponseMode::kFailFetchEventDispatch;
  }

  // Tells this worker to simulate "early response", where the respondWith()
  // promise resolves before the waitUntil() promise. In this mode, the
  // helper sets the response mode to "early response", which simulates the
  // promise passed to respondWith() resolving before the waitUntil() promise
  // resolves. In this mode, the helper will respond to fetch events
  // immediately, but will not finish the fetch event until FinishWaitUntil() is
  // called.
  void RespondEarly() { response_mode_ = ResponseMode::kEarlyResponse; }
  void FinishWaitUntil() {
    std::move(finish_callback_)
        .Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED);
    base::RunLoop().RunUntilIdle();
  }

  // Tells this worker to wait for FinishRespondWith() to be called before
  // providing the response to the fetch event.
  void DeferResponse() { response_mode_ = ResponseMode::kDeferredResponse; }
  void FinishRespondWith() {
    response_callback_->OnResponse(
        OkResponse(nullptr /* blob_body */),
        blink::mojom::ServiceWorkerFetchEventTiming::New());
    response_callback_.FlushForTesting();
    std::move(finish_callback_)
        .Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED);
  }

  void ReadRequestBody(std::string* out_string) {
    ASSERT_TRUE(request_body_);
    const std::vector<network::DataElement>* elements =
        request_body_->elements();
    // So far this test expects a single bytes element.
    ASSERT_EQ(1u, elements->size());
    const network::DataElement& element = elements->front();
    ASSERT_EQ(network::mojom::DataElementType::kBytes, element.type());
    *out_string = std::string(element.bytes(), element.length());
  }

  void RunUntilFetchEvent() {
    if (has_received_fetch_event_)
      return;
    base::RunLoop run_loop;
    quit_closure_for_fetch_event_ = run_loop.QuitClosure();
    run_loop.Run();
  }

 protected:
  void DispatchFetchEventForMainResource(
      blink::mojom::DispatchFetchEventParamsPtr params,
      mojo::PendingRemote<blink::mojom::ServiceWorkerFetchResponseCallback>
          pending_response_callback,
      blink::mojom::ServiceWorker::DispatchFetchEventForMainResourceCallback
          finish_callback) override {
    // Basic checks on DispatchFetchEvent parameters.
    EXPECT_TRUE(params->request->is_main_resource_load);

    has_received_fetch_event_ = true;
    if (params->request->body)
      request_body_ = params->request->body;

    mojo::Remote<blink::mojom::ServiceWorkerFetchResponseCallback>
        response_callback(std::move(pending_response_callback));
    switch (response_mode_) {
      case ResponseMode::kDefault:
        FakeServiceWorker::DispatchFetchEventForMainResource(
            std::move(params), response_callback.Unbind(),
            std::move(finish_callback));
        break;
      case ResponseMode::kBlob:
        response_callback->OnResponse(
            OkResponse(std::move(blob_body_)),
            blink::mojom::ServiceWorkerFetchEventTiming::New());
        std::move(finish_callback)
            .Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED);
        break;
      case ResponseMode::kStream:
        response_callback->OnResponseStream(
            OkResponse(nullptr /* blob_body */), std::move(stream_handle_),
            blink::mojom::ServiceWorkerFetchEventTiming::New());
        std::move(finish_callback)
            .Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED);
        break;
      case ResponseMode::kFallbackResponse:
        response_callback->OnFallback(
            blink::mojom::ServiceWorkerFetchEventTiming::New());
        std::move(finish_callback)
            .Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED);
        break;
      case ResponseMode::kErrorResponse:
        response_callback->OnResponse(
            ErrorResponse(),
            blink::mojom::ServiceWorkerFetchEventTiming::New());
        std::move(finish_callback)
            .Run(blink::mojom::ServiceWorkerEventStatus::REJECTED);
        break;
      case ResponseMode::kFailFetchEventDispatch:
        // Simulate failure by stopping the worker before the event finishes.
        // This causes ServiceWorkerVersion::StartRequest() to call its error
        // callback, which triggers ServiceWorkerNavigationLoader's dispatch
        // failed behavior.
        embedded_worker_instance_client_->host()->OnStopped();

        // Finish the event by calling |finish_callback|.
        // This is the Mojo callback for
        // blink::mojom::ServiceWorker::DispatchFetchEventForMainResource().
        // If this is not called, Mojo will complain. In production code,
        // ServiceWorkerContextClient would call this when it aborts all
        // callbacks after an unexpected stop.
        std::move(finish_callback)
            .Run(blink::mojom::ServiceWorkerEventStatus::ABORTED);
        break;
      case ResponseMode::kDeferredResponse:
        finish_callback_ = std::move(finish_callback);
        response_callback_ = std::move(response_callback);
        // Now the caller must call FinishRespondWith() to finish the event.
        break;
      case ResponseMode::kEarlyResponse:
        finish_callback_ = std::move(finish_callback);
        response_callback->OnResponse(
            OkResponse(nullptr /* blob_body */),
            blink::mojom::ServiceWorkerFetchEventTiming::New());
        // Now the caller must call FinishWaitUntil() to finish the event.
        break;
      case ResponseMode::kRedirect:
        response_callback->OnResponse(
            RedirectResponse(redirected_url_.spec()),
            blink::mojom::ServiceWorkerFetchEventTiming::New());
        std::move(finish_callback)
            .Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED);
        break;
    }

    if (quit_closure_for_fetch_event_)
      std::move(quit_closure_for_fetch_event_).Run();
  }

 private:
  enum class ResponseMode {
    kDefault,
    kBlob,
    kStream,
    kFallbackResponse,
    kErrorResponse,
    kFailFetchEventDispatch,
    kDeferredResponse,
    kEarlyResponse,
    kRedirect
  };

  ResponseMode response_mode_ = ResponseMode::kDefault;
  scoped_refptr<network::ResourceRequestBody> request_body_;

  // For ResponseMode::kBlob.
  blink::mojom::SerializedBlobPtr blob_body_;

  // For ResponseMode::kStream.
  blink::mojom::ServiceWorkerStreamHandlePtr stream_handle_;

  // For ResponseMode::kEarlyResponse and kDeferredResponse.
  blink::mojom::ServiceWorker::DispatchFetchEventForMainResourceCallback
      finish_callback_;
  mojo::Remote<blink::mojom::ServiceWorkerFetchResponseCallback>
      response_callback_;

  // For ResponseMode::kRedirect.
  GURL redirected_url_;

  bool has_received_fetch_event_ = false;
  base::OnceClosure quit_closure_for_fetch_event_;

  FakeEmbeddedWorkerInstanceClient* const embedded_worker_instance_client_;

  DISALLOW_COPY_AND_ASSIGN(FetchEventServiceWorker);
};

// Returns typical response info for a resource load that went through a service
// worker.
std::unique_ptr<network::ResourceResponseHead>
CreateResponseInfoFromServiceWorker() {
  auto head = std::make_unique<network::ResourceResponseHead>();
  head->was_fetched_via_service_worker = true;
  head->was_fallback_required_by_service_worker = false;
  head->url_list_via_service_worker = std::vector<GURL>();
  head->response_type = network::mojom::FetchResponseType::kDefault;
  head->is_in_cache_storage = false;
  head->cache_storage_cache_name = std::string();
  head->did_service_worker_navigation_preload = false;
  return head;
}

const char kHistogramMainResourceFetchEvent[] =
    "ServiceWorker.FetchEvent.MainResource.Status";

// ServiceWorkerNavigationLoaderTest is for testing the handling of requests
// by a service worker via ServiceWorkerNavigationLoader.
//
// Of course, no actual service worker runs in the unit test, it is simulated
// via EmbeddedWorkerTestHelper receiving IPC messages from the browser and
// responding as if a service worker is running in the renderer.
class ServiceWorkerNavigationLoaderTest : public testing::Test {
 public:
  ServiceWorkerNavigationLoaderTest()
      : task_environment_(BrowserTaskEnvironment::IO_MAINLOOP) {}
  ~ServiceWorkerNavigationLoaderTest() override = default;

  void SetUp() override {
    helper_ = std::make_unique<EmbeddedWorkerTestHelper>(base::FilePath());

    // Create an active service worker.
    storage()->LazyInitializeForTest();
    blink::mojom::ServiceWorkerRegistrationOptions options;
    options.scope = GURL("https://example.com/");
    registration_ =
        new ServiceWorkerRegistration(options, storage()->NewRegistrationId(),
                                      helper_->context()->AsWeakPtr());
    version_ = new ServiceWorkerVersion(
        registration_.get(), GURL("https://example.com/service_worker.js"),
        blink::mojom::ScriptType::kClassic, storage()->NewVersionId(),
        helper_->context()->AsWeakPtr());
    std::vector<ServiceWorkerDatabase::ResourceRecord> records;
    records.push_back(WriteToDiskCacheSync(
        storage(), version_->script_url(), storage()->NewResourceId(),
        {} /* headers */, "I'm the body", "I'm the meta data"));
    version_->script_cache_map()->SetResources(records);
    version_->set_fetch_handler_existence(
        ServiceWorkerVersion::FetchHandlerExistence::EXISTS);
    version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
    registration_->SetActiveVersion(version_);

    // Make the registration findable via storage functions.
    registration_->set_last_update_check(base::Time::Now());
    base::Optional<blink::ServiceWorkerStatusCode> status;
    base::RunLoop run_loop;
    storage()->StoreRegistration(
        registration_.get(), version_.get(),
        ReceiveServiceWorkerStatus(&status, run_loop.QuitClosure()));
    run_loop.Run();
    ASSERT_EQ(blink::ServiceWorkerStatusCode::kOk, status.value());

    // Set up custom fakes to let tests customize how to respond to fetch
    // events.
    auto* client =
        helper_->AddNewPendingInstanceClient<FakeEmbeddedWorkerInstanceClient>(
            helper_.get());
    service_worker_ =
        helper_->AddNewPendingServiceWorker<FetchEventServiceWorker>(
            helper_.get(), client);
  }

  ServiceWorkerStorage* storage() { return helper_->context()->storage(); }

  // Starts a request. After calling this, the request is ongoing and the
  // caller can use functions like client_.RunUntilComplete() to wait for
  // completion.
  void StartRequest(std::unique_ptr<network::ResourceRequest> request) {
    // Create a ServiceWorkerProviderHost and simulate what
    // ServiceWorkerControlleeRequestHandler does to assign it a controller.
    if (!provider_host_) {
      provider_host_ = CreateProviderHostForWindow(
          helper_->mock_render_process_id(), /*is_parent_frame_secure=*/true,
          helper_->context()->AsWeakPtr(), &provider_endpoints_);
      provider_host_->UpdateUrls(request->url, request->url,
                                 url::Origin::Create(request->url));
      provider_host_->AddMatchingRegistration(registration_.get());
      provider_host_->SetControllerRegistration(
          registration_, /*notify_controllerchange=*/false);
    }

    // Create a ServiceWorkerNavigationLoader.
    loader_ = std::make_unique<ServiceWorkerNavigationLoader>(
        base::BindOnce(&ServiceWorkerNavigationLoaderTest::Fallback,
                       base::Unretained(this)),
        provider_host_,
        base::WrapRefCounted<URLLoaderFactoryGetter>(
            helper_->context()->loader_factory_getter()));

    // Load |request.url|.
    loader_->StartRequest(*request, mojo::MakeRequest(&loader_ptr_),
                          client_.CreateRemote());
  }

  // The |fallback_callback| passed to the ServiceWorkerNavigationLoader in
  // StartRequest().
  void Fallback(bool reset_subresource_loader_params) {
    did_call_fallback_callback_ = true;
    reset_subresource_loader_params_ = reset_subresource_loader_params;
    if (quit_closure_for_fallback_callback_)
      std::move(quit_closure_for_fallback_callback_).Run();
  }

  // Runs until the ServiceWorkerNavigationLoader created in StartRequest()
  // calls the |fallback_callback| given to it. The argument passed to
  // |fallback_callback| is saved in |reset_subresurce_loader_params_|.
  void RunUntilFallbackCallback() {
    if (did_call_fallback_callback_)
      return;
    base::RunLoop run_loop;
    quit_closure_for_fallback_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void ExpectResponseInfo(const network::ResourceResponseHead& info,
                          const network::ResourceResponseHead& expected_info) {
    EXPECT_EQ(expected_info.was_fetched_via_service_worker,
              info.was_fetched_via_service_worker);
    EXPECT_EQ(expected_info.was_fallback_required_by_service_worker,
              info.was_fallback_required_by_service_worker);
    EXPECT_EQ(expected_info.url_list_via_service_worker,
              info.url_list_via_service_worker);
    EXPECT_EQ(expected_info.response_type, info.response_type);
    EXPECT_FALSE(info.service_worker_start_time.is_null());
    EXPECT_FALSE(info.service_worker_ready_time.is_null());
    EXPECT_LT(info.service_worker_start_time, info.service_worker_ready_time);
    EXPECT_EQ(expected_info.is_in_cache_storage, info.is_in_cache_storage);
    EXPECT_EQ(expected_info.cache_storage_cache_name,
              info.cache_storage_cache_name);
    EXPECT_EQ(expected_info.did_service_worker_navigation_preload,
              info.did_service_worker_navigation_preload);
  }

  std::unique_ptr<network::ResourceRequest> CreateRequest() {
    std::unique_ptr<network::ResourceRequest> request =
        std::make_unique<network::ResourceRequest>();
    request->url = GURL("https://example.com/");
    request->method = "GET";
    request->mode = network::mojom::RequestMode::kNavigate;
    request->credentials_mode = network::mojom::CredentialsMode::kInclude;
    request->redirect_mode = network::mojom::RedirectMode::kManual;
    return request;
  }

  bool HasWorkInBrowser(ServiceWorkerVersion* version) const {
    return version->HasWorkInBrowser();
  }

 protected:
  BrowserTaskEnvironment task_environment_;
  std::unique_ptr<EmbeddedWorkerTestHelper> helper_;
  scoped_refptr<ServiceWorkerRegistration> registration_;
  scoped_refptr<ServiceWorkerVersion> version_;
  FetchEventServiceWorker* service_worker_;
  storage::BlobStorageContext blob_context_;
  network::TestURLLoaderClient client_;
  std::unique_ptr<ServiceWorkerNavigationLoader> loader_;
  network::mojom::URLLoaderPtr loader_ptr_;
  base::WeakPtr<ServiceWorkerProviderHost> provider_host_;
  ServiceWorkerRemoteProviderEndpoint provider_endpoints_;

  bool did_call_fallback_callback_ = false;
  bool reset_subresource_loader_params_ = false;
  base::OnceClosure quit_closure_for_fallback_callback_;
};

TEST_F(ServiceWorkerNavigationLoaderTest, Basic) {
  base::HistogramTester histogram_tester;

  // Perform the request.
  StartRequest(CreateRequest());
  client_.RunUntilComplete();

  EXPECT_EQ(net::OK, client_.completion_status().error_code);
  const network::ResourceResponseHead& info = client_.response_head();
  EXPECT_EQ(200, info.headers->response_code());
  EXPECT_FALSE(info.load_timing.receive_headers_start.is_null());
  EXPECT_FALSE(info.load_timing.receive_headers_end.is_null());
  EXPECT_LE(info.load_timing.receive_headers_start,
            info.load_timing.receive_headers_end);
  ExpectResponseInfo(info, *CreateResponseInfoFromServiceWorker());

  histogram_tester.ExpectUniqueSample(kHistogramMainResourceFetchEvent,
                                      blink::ServiceWorkerStatusCode::kOk, 1);
  histogram_tester.ExpectTotalCount(
      "ServiceWorker.LoadTiming.MainFrame.MainResource."
      "ResponseReceivedToCompleted2",
      1);
}

TEST_F(ServiceWorkerNavigationLoaderTest, NoActiveWorker) {
  base::HistogramTester histogram_tester;

  // Make a provider host without a controller.
  provider_host_ = CreateProviderHostForWindow(
      helper_->mock_render_process_id(), /*is_parent_frame_secure=*/true,
      helper_->context()->AsWeakPtr(), &provider_endpoints_);
  provider_host_->UpdateUrls(GURL("https://example.com/"),
                             GURL("https://example.com/"),
                             url::Origin::Create(GURL("https://example.com/")));

  // Perform the request.
  StartRequest(CreateRequest());
  client_.RunUntilComplete();
  EXPECT_EQ(net::ERR_FAILED, client_.completion_status().error_code);

  // No fetch event was dispatched.
  histogram_tester.ExpectTotalCount(kHistogramMainResourceFetchEvent, 0);
  histogram_tester.ExpectTotalCount(
      "ServiceWorker.LoadTiming.MainFrame.MainResource."
      "StartToForwardServiceWorker",
      0);
}

// Test that the request body is passed to the fetch event.
TEST_F(ServiceWorkerNavigationLoaderTest, RequestBody) {
  const std::string kData = "hi this is the request body";

  // Create a request with a body.
  auto request_body = base::MakeRefCounted<network::ResourceRequestBody>();
  request_body->AppendBytes(kData.c_str(), kData.length());
  std::unique_ptr<network::ResourceRequest> request = CreateRequest();
  request->method = "POST";
  request->request_body = request_body;

  // This test doesn't use the response to the fetch event, so just have the
  // service worker do the default simple response.
  StartRequest(std::move(request));
  client_.RunUntilComplete();

  // Verify that the request body was passed to the fetch event.
  std::string body;
  service_worker_->ReadRequestBody(&body);
  EXPECT_EQ(kData, body);
}

TEST_F(ServiceWorkerNavigationLoaderTest, BlobResponse) {
  base::HistogramTester histogram_tester;

  // Construct the blob to respond with.
  const std::string kResponseBody = "Here is sample text for the blob.";
  auto blob_data = std::make_unique<storage::BlobDataBuilder>("blob-id:myblob");
  blob_data->AppendData(kResponseBody);
  std::unique_ptr<storage::BlobDataHandle> blob_handle =
      blob_context_.AddFinishedBlob(std::move(blob_data));

  auto blob = blink::mojom::SerializedBlob::New();
  blob->uuid = blob_handle->uuid();
  blob->size = blob_handle->size();
  storage::BlobImpl::Create(std::move(blob_handle),
                            blob->blob.InitWithNewPipeAndPassReceiver());
  service_worker_->RespondWithBlob(std::move(blob));

  // Perform the request.
  StartRequest(CreateRequest());
  client_.RunUntilComplete();

  const network::ResourceResponseHead& info = client_.response_head();
  EXPECT_EQ(200, info.headers->response_code());
  ExpectResponseInfo(info, *CreateResponseInfoFromServiceWorker());
  EXPECT_EQ(33, info.content_length);

  // Test the body.
  std::string body;
  EXPECT_TRUE(client_.response_body().is_valid());
  EXPECT_TRUE(
      mojo::BlockingCopyToString(client_.response_body_release(), &body));
  EXPECT_EQ(kResponseBody, body);
  EXPECT_EQ(net::OK, client_.completion_status().error_code);

  // Test histogram of reading body.
  histogram_tester.ExpectTotalCount(
      "ServiceWorker.LoadTiming.MainFrame.MainResource."
      "ResponseReceivedToCompleted2",
      1);
}

// Tell the helper to respond with a non-existent Blob.
TEST_F(ServiceWorkerNavigationLoaderTest, BrokenBlobResponse) {
  base::HistogramTester histogram_tester;

  const std::string kBrokenUUID = "broken_uuid";

  // Create the broken blob.
  std::unique_ptr<storage::BlobDataHandle> blob_handle =
      blob_context_.AddBrokenBlob(kBrokenUUID, "", "",
                                  storage::BlobStatus::ERR_OUT_OF_MEMORY);
  auto blob = blink::mojom::SerializedBlob::New();
  blob->uuid = kBrokenUUID;
  storage::BlobImpl::Create(std::move(blob_handle),
                            blob->blob.InitWithNewPipeAndPassReceiver());
  service_worker_->RespondWithBlob(std::move(blob));

  // Perform the request.
  StartRequest(CreateRequest());

  // We should get a valid response once the headers arrive.
  client_.RunUntilResponseReceived();
  const network::ResourceResponseHead& info = client_.response_head();
  EXPECT_EQ(200, info.headers->response_code());
  ExpectResponseInfo(info, *CreateResponseInfoFromServiceWorker());

  // However, since the blob is broken we should get an error while transferring
  // the body.
  client_.RunUntilComplete();
  EXPECT_EQ(net::ERR_OUT_OF_MEMORY, client_.completion_status().error_code);

  // Timing histograms shouldn't be recorded on broken response.
  histogram_tester.ExpectTotalCount(
      "ServiceWorker.LoadTiming.MainFrame.MainResource."
      "StartToForwardServiceWorker",
      0);
  histogram_tester.ExpectTotalCount(
      "ServiceWorker.LoadTiming.MainFrame.MainResource."
      "ResponseReceivedToCompleted2",
      0);
}

TEST_F(ServiceWorkerNavigationLoaderTest, StreamResponse) {
  base::HistogramTester histogram_tester;

  // Construct the Stream to respond with.
  const char kResponseBody[] = "Here is sample text for the Stream.";
  mojo::Remote<blink::mojom::ServiceWorkerStreamCallback> stream_callback;
  mojo::DataPipe data_pipe;
  service_worker_->RespondWithStream(
      stream_callback.BindNewPipeAndPassReceiver(),
      std::move(data_pipe.consumer_handle));

  // Perform the request.
  StartRequest(CreateRequest());
  client_.RunUntilResponseReceived();

  const network::ResourceResponseHead& info = client_.response_head();
  EXPECT_EQ(200, info.headers->response_code());
  ExpectResponseInfo(info, *CreateResponseInfoFromServiceWorker());

  EXPECT_FALSE(version_->HasNoWork());

  // Write the body stream.
  uint32_t written_bytes = sizeof(kResponseBody) - 1;
  MojoResult mojo_result = data_pipe.producer_handle->WriteData(
      kResponseBody, &written_bytes, MOJO_WRITE_DATA_FLAG_NONE);
  ASSERT_EQ(MOJO_RESULT_OK, mojo_result);
  EXPECT_EQ(sizeof(kResponseBody) - 1, written_bytes);
  stream_callback->OnCompleted();
  data_pipe.producer_handle.reset();

  client_.RunUntilComplete();
  EXPECT_EQ(net::OK, client_.completion_status().error_code);

  // Test the body.
  std::string response;
  EXPECT_TRUE(client_.response_body().is_valid());
  EXPECT_TRUE(
      mojo::BlockingCopyToString(client_.response_body_release(), &response));
  EXPECT_EQ(kResponseBody, response);

  // Test histogram of reading body.
  histogram_tester.ExpectTotalCount(
      "ServiceWorker.LoadTiming.MainFrame.MainResource."
      "ResponseReceivedToCompleted2",
      1);
}

// Test when a stream response body is aborted.
TEST_F(ServiceWorkerNavigationLoaderTest, StreamResponse_Abort) {
  base::HistogramTester histogram_tester;

  // Construct the Stream to respond with.
  const char kResponseBody[] = "Here is sample text for the Stream.";
  mojo::Remote<blink::mojom::ServiceWorkerStreamCallback> stream_callback;
  mojo::DataPipe data_pipe;
  service_worker_->RespondWithStream(
      stream_callback.BindNewPipeAndPassReceiver(),
      std::move(data_pipe.consumer_handle));

  // Perform the request.
  StartRequest(CreateRequest());
  client_.RunUntilResponseReceived();

  const network::ResourceResponseHead& info = client_.response_head();
  EXPECT_EQ(200, info.headers->response_code());
  ExpectResponseInfo(info, *CreateResponseInfoFromServiceWorker());

  // Start writing the body stream, then abort before finishing.
  uint32_t written_bytes = sizeof(kResponseBody) - 1;
  MojoResult mojo_result = data_pipe.producer_handle->WriteData(
      kResponseBody, &written_bytes, MOJO_WRITE_DATA_FLAG_NONE);
  ASSERT_EQ(MOJO_RESULT_OK, mojo_result);
  EXPECT_EQ(sizeof(kResponseBody) - 1, written_bytes);
  stream_callback->OnAborted();
  data_pipe.producer_handle.reset();

  client_.RunUntilComplete();
  EXPECT_EQ(net::ERR_ABORTED, client_.completion_status().error_code);

  // Test the body.
  std::string response;
  EXPECT_TRUE(client_.response_body().is_valid());
  EXPECT_TRUE(
      mojo::BlockingCopyToString(client_.response_body_release(), &response));
  EXPECT_EQ(kResponseBody, response);

  // Timing histograms shouldn't be recorded on abort.
  histogram_tester.ExpectTotalCount(
      "ServiceWorker.LoadTiming.MainFrame.MainResource."
      "StartToForwardServiceWorker",
      0);
  histogram_tester.ExpectTotalCount(
      "ServiceWorker.LoadTiming.MainFrame.MainResource."
      "ResponseReceivedToCompleted2",
      0);
}

// Test when the loader is cancelled while a stream response is being written.
TEST_F(ServiceWorkerNavigationLoaderTest, StreamResponseAndCancel) {
  base::HistogramTester histogram_tester;

  // Construct the Stream to respond with.
  const char kResponseBody[] = "Here is sample text for the Stream.";
  mojo::Remote<blink::mojom::ServiceWorkerStreamCallback> stream_callback;
  mojo::DataPipe data_pipe;
  service_worker_->RespondWithStream(
      stream_callback.BindNewPipeAndPassReceiver(),
      std::move(data_pipe.consumer_handle));

  // Perform the request.
  StartRequest(CreateRequest());
  client_.RunUntilResponseReceived();

  const network::ResourceResponseHead& info = client_.response_head();
  EXPECT_EQ(200, info.headers->response_code());
  ExpectResponseInfo(info, *CreateResponseInfoFromServiceWorker());

  // Start writing the body stream, then break the Mojo connection to the loader
  // before finishing.
  uint32_t written_bytes = sizeof(kResponseBody) - 1;
  MojoResult mojo_result = data_pipe.producer_handle->WriteData(
      kResponseBody, &written_bytes, MOJO_WRITE_DATA_FLAG_NONE);
  ASSERT_EQ(MOJO_RESULT_OK, mojo_result);
  EXPECT_EQ(sizeof(kResponseBody) - 1, written_bytes);
  EXPECT_TRUE(data_pipe.producer_handle.is_valid());
  loader_ptr_.reset();
  base::RunLoop().RunUntilIdle();

  // Although ServiceWorkerNavigationLoader resets its URLLoaderClient pointer
  // on connection error, the URLLoaderClient still exists. In this test, it is
  // |client_| which owns the data pipe, so it's still valid to write data to
  // it.
  mojo_result = data_pipe.producer_handle->WriteData(
      kResponseBody, &written_bytes, MOJO_WRITE_DATA_FLAG_NONE);
  // TODO(falken): This should probably be an error.
  EXPECT_EQ(MOJO_RESULT_OK, mojo_result);

  client_.RunUntilComplete();
  EXPECT_FALSE(data_pipe.consumer_handle.is_valid());
  EXPECT_EQ(net::ERR_ABORTED, client_.completion_status().error_code);

  // Timing histograms shouldn't be recorded on cancel.
  histogram_tester.ExpectTotalCount(
      "ServiceWorker.LoadTiming.MainFrame.MainResource."
      "StartToForwardServiceWorker",
      0);
  histogram_tester.ExpectTotalCount(
      "ServiceWorker.LoadTiming.MainFrame.MainResource."
      "ResponseReceivedToCompleted2",
      0);
}

// Test when the service worker responds with network fallback.
// i.e., does not call respondWith().
TEST_F(ServiceWorkerNavigationLoaderTest, FallbackResponse) {
  base::HistogramTester histogram_tester;
  service_worker_->RespondWithFallback();

  // Perform the request.
  StartRequest(CreateRequest());

  // The fallback callback should be called.
  RunUntilFallbackCallback();
  EXPECT_FALSE(reset_subresource_loader_params_);

  // The request should not be handled by the loader, but it shouldn't be a
  // failure.
  EXPECT_TRUE(provider_host_->controller());
  histogram_tester.ExpectUniqueSample(kHistogramMainResourceFetchEvent,
                                      blink::ServiceWorkerStatusCode::kOk, 1);

  // Test histogram of network fallback.
  histogram_tester.ExpectTotalCount(
      "ServiceWorker.LoadTiming.MainFrame.MainResource."
      "FetchHandlerEndToFallbackNetwork",
      1);
}

// Test when the service worker rejects the FetchEvent.
TEST_F(ServiceWorkerNavigationLoaderTest, ErrorResponse) {
  base::HistogramTester histogram_tester;
  service_worker_->RespondWithError();

  // Perform the request.
  StartRequest(CreateRequest());
  client_.RunUntilComplete();
  EXPECT_EQ(net::ERR_FAILED, client_.completion_status().error_code);

  // Event dispatch still succeeded.
  histogram_tester.ExpectUniqueSample(kHistogramMainResourceFetchEvent,
                                      blink::ServiceWorkerStatusCode::kOk, 1);
  // Timing UMAs shouldn't be recorded when we receive an error response.
  histogram_tester.ExpectTotalCount(
      "ServiceWorker.LoadTiming.MainFrame.MainResource."
      "StartToForwardServiceWorker",
      0);
}

// Test when dispatching the fetch event to the service worker failed.
TEST_F(ServiceWorkerNavigationLoaderTest, FailFetchDispatch) {
  base::HistogramTester histogram_tester;
  service_worker_->FailToDispatchFetchEvent();

  // Perform the request.
  StartRequest(CreateRequest());

  // The fallback callback should be called.
  RunUntilFallbackCallback();
  EXPECT_TRUE(reset_subresource_loader_params_);
  EXPECT_FALSE(provider_host_->controller());

  histogram_tester.ExpectUniqueSample(
      kHistogramMainResourceFetchEvent,
      blink::ServiceWorkerStatusCode::kErrorFailed, 1);
  // Timing UMAs shouldn't be recorded when failed to dispatch an event.
  histogram_tester.ExpectTotalCount(
      "ServiceWorker.LoadTiming.MainFrame.MainResource."
      "StartToForwardServiceWorker",
      0);
}

// Test when the respondWith() promise resolves before the waitUntil() promise
// resolves. The response should be received before the event finishes.
TEST_F(ServiceWorkerNavigationLoaderTest, EarlyResponse) {
  service_worker_->RespondEarly();

  // Perform the request.
  StartRequest(CreateRequest());
  client_.RunUntilComplete();

  const network::ResourceResponseHead& info = client_.response_head();
  EXPECT_EQ(200, info.headers->response_code());
  ExpectResponseInfo(info, *CreateResponseInfoFromServiceWorker());

  // Although the response was already received, the event remains outstanding
  // until waitUntil() resolves.
  EXPECT_TRUE(HasWorkInBrowser(version_.get()));
  service_worker_->FinishWaitUntil();
  EXPECT_FALSE(HasWorkInBrowser(version_.get()));
}

// Test responding to the fetch event with a redirect response.
TEST_F(ServiceWorkerNavigationLoaderTest, Redirect) {
  base::HistogramTester histogram_tester;
  GURL new_url("https://example.com/redirected");
  service_worker_->RespondWithRedirectResponse(new_url);

  // Perform the request.
  StartRequest(CreateRequest());
  client_.RunUntilRedirectReceived();

  const network::ResourceResponseHead& info = client_.response_head();
  EXPECT_EQ(301, info.headers->response_code());
  ExpectResponseInfo(info, *CreateResponseInfoFromServiceWorker());

  const net::RedirectInfo& redirect_info = client_.redirect_info();
  EXPECT_EQ(301, redirect_info.status_code);
  EXPECT_EQ("GET", redirect_info.new_method);
  EXPECT_EQ(new_url, redirect_info.new_url);

  histogram_tester.ExpectUniqueSample(kHistogramMainResourceFetchEvent,
                                      blink::ServiceWorkerStatusCode::kOk, 1);
}

TEST_F(ServiceWorkerNavigationLoaderTest, Lifetime) {
  StartRequest(CreateRequest());
  base::WeakPtr<ServiceWorkerNavigationLoader> loader = loader_->AsWeakPtr();
  ASSERT_TRUE(loader);

  client_.RunUntilComplete();
  EXPECT_TRUE(loader);

  // Even after calling DetachedFromRequest(), |loader_| should be alive until
  // the Mojo connection to the loader is disconnected.
  loader_.release()->DetachedFromRequest();
  EXPECT_TRUE(loader);

  // When the interface pointer to |loader_| is disconnected, its weak pointers
  // (|loader|) are invalidated.
  loader_ptr_.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(loader);

  // |loader_| is deleted here. LSan test will alert if it leaks.
}

TEST_F(ServiceWorkerNavigationLoaderTest, ConnectionErrorDuringFetchEvent) {
  service_worker_->DeferResponse();
  StartRequest(CreateRequest());

  // Wait for the fetch event to be dispatched.
  service_worker_->RunUntilFetchEvent();

  // Break the Mojo connection. The loader should return an aborted status.
  loader_ptr_.reset();
  client_.RunUntilComplete();
  EXPECT_EQ(net::ERR_ABORTED, client_.completion_status().error_code);

  // The loader is still alive. Finish the fetch event. It shouldn't crash or
  // call any callbacks on |client_|, which would throw an error.
  service_worker_->FinishRespondWith();

  // There's no event to wait for, so just pump the message loop and the test
  // passes if there is no error or crash.
  base::RunLoop().RunUntilIdle();
}

TEST_F(ServiceWorkerNavigationLoaderTest, CancelNavigationDuringFetchEvent) {
  StartRequest(CreateRequest());

  // Delete the provider host during the request. The load should abort without
  // crashing.
  provider_endpoints_.host_remote()->reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(provider_host_);

  client_.RunUntilComplete();
  EXPECT_EQ(net::ERR_ABORTED, client_.completion_status().error_code);
}

}  // namespace service_worker_navigation_loader_unittest
}  // namespace content
