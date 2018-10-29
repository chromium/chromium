// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_navigation_loader.h"

#include <string>
#include <utility>
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/loader/navigation_loader_interceptor.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/common/service_worker/service_worker.mojom.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/common/single_request_url_loader_factory.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "net/ssl/ssl_info.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_response.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/test/test_url_loader_client.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_impl.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "testing/gtest/include/gtest/gtest.h"
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

// NavigationPreloadLoaderClient mocks the renderer-side URLLoaderClient for the
// navigation preload network request performed by the browser. In production
// code, this is ServiceWorkerContextClient::NavigationPreloadRequest,
// which it forwards the response to FetchEvent#preloadResponse. Here, it
// simulates passing the response to FetchEvent#respondWith.
//
// The navigation preload test is quite involved. The flow of data is:
// 1. ServiceWorkerNavigationLoader asks ServiceWorkerFetchDispatcher to start
//    navigation preload.
// 2. ServiceWorkerFetchDispatcher starts the network request which is mocked
//    by EmbeddedWorkerTestHelper's default network loader factory. The
//    response is sent to
//    ServiceWorkerFetchDispatcher::DelegatingURLLoaderClient.
// 3. DelegatingURLLoaderClient sends the response to the |preload_handle|
//    that was passed to Helper::OnFetchEvent().
// 4. Helper::OnFetchEvent() creates NavigationPreloadLoaderClient, which
//    receives the response.
// 5. NavigationPreloadLoaderClient calls OnFetchEvent()'s callbacks
//    with the response.
// 6. Like all FetchEvent responses, the response is sent to
//    ServiceWorkerNavigationLoader::DidDispatchFetchEvent, and the
//    RequestHandler is returned.
class NavigationPreloadLoaderClient final
    : public network::mojom::URLLoaderClient {
 public:
  NavigationPreloadLoaderClient(
      blink::mojom::FetchEventPreloadHandlePtr preload_handle,
      blink::mojom::ServiceWorkerFetchResponseCallbackPtr response_callback,
      mojom::ServiceWorker::DispatchFetchEventCallback finish_callback)
      : url_loader_(std::move(preload_handle->url_loader)),
        binding_(this, std::move(preload_handle->url_loader_client_request)),
        response_callback_(std::move(response_callback)),
        finish_callback_(std::move(finish_callback)) {
    binding_.set_connection_error_handler(
        base::BindOnce(&NavigationPreloadLoaderClient::OnConnectionError,
                       base::Unretained(this)));
  }
  ~NavigationPreloadLoaderClient() override = default;

  // network::mojom::URLLoaderClient implementation
  void OnReceiveResponse(
      const network::ResourceResponseHead& response_head) override {
    response_head_ = response_head;
  }
  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body) override {
    body_ = std::move(body);
    // We could call OnResponseStream() here, but for simplicity, don't do
    // anything until OnComplete().
  }
  void OnComplete(const network::URLLoaderCompletionStatus& status) override {
    blink::mojom::ServiceWorkerStreamCallbackPtr stream_callback;
    auto stream_handle = blink::mojom::ServiceWorkerStreamHandle::New();
    stream_handle->callback_request = mojo::MakeRequest(&stream_callback);
    stream_handle->stream = std::move(body_);

    // Simulate passing the navigation preload response to
    // FetchEvent#respondWith.
    auto response = blink::mojom::FetchAPIResponse::New();
    response->url_list =
        std::vector<GURL>(response_head_.url_list_via_service_worker);
    response->status_code = response_head_.headers->response_code();
    response->status_text = response_head_.headers->GetStatusText();
    response->response_type = response_head_.response_type;
    response_callback_->OnResponseStream(
        std::move(response), std::move(stream_handle),
        blink::mojom::ServiceWorkerFetchEventTiming::New());
    std::move(finish_callback_)
        .Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED,
             base::TimeTicks::Now());
    stream_callback->OnCompleted();
    delete this;
  }
  void OnReceiveRedirect(
      const net::RedirectInfo& redirect_info,
      const network::ResourceResponseHead& response_head) override {}
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback ack_callback) override {}
  void OnReceiveCachedMetadata(const std::vector<uint8_t>& data) override {}
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override {}

  void OnConnectionError() { delete this; }

 private:
  network::mojom::URLLoaderPtr url_loader_;
  mojo::Binding<network::mojom::URLLoaderClient> binding_;

  network::ResourceResponseHead response_head_;
  mojo::ScopedDataPipeConsumerHandle body_;

  // Callbacks that complete Helper::OnFetchEvent().
  blink::mojom::ServiceWorkerFetchResponseCallbackPtr response_callback_;
  mojom::ServiceWorker::DispatchFetchEventCallback finish_callback_;

  DISALLOW_COPY_AND_ASSIGN(NavigationPreloadLoaderClient);
};

// Helper simulates a service worker handling fetch events. The response can be
// customized via RespondWith* functions.
class Helper : public EmbeddedWorkerTestHelper {
 public:
  Helper() : EmbeddedWorkerTestHelper(base::FilePath()) {}
  ~Helper() override = default;

  // Tells this helper to respond to fetch events with the specified blob.
  void RespondWithBlob(blink::mojom::SerializedBlobPtr blob) {
    response_mode_ = ResponseMode::kBlob;
    blob_body_ = std::move(blob);
  }

  // Tells this helper to respond to fetch events with the specified stream.
  void RespondWithStream(
      blink::mojom::ServiceWorkerStreamCallbackRequest callback_request,
      mojo::ScopedDataPipeConsumerHandle consumer_handle) {
    response_mode_ = ResponseMode::kStream;
    stream_handle_ = blink::mojom::ServiceWorkerStreamHandle::New();
    stream_handle_->callback_request = std::move(callback_request);
    stream_handle_->stream = std::move(consumer_handle);
  }

  // Tells this helper to respond to fetch events with network fallback.
  // i.e., simulate the service worker not calling respondWith().
  void RespondWithFallback() {
    response_mode_ = ResponseMode::kFallbackResponse;
  }

  // Tells this helper to respond to fetch events with an error response.
  void RespondWithError() { response_mode_ = ResponseMode::kErrorResponse; }

  // Tells this helper to respond to fetch events with
  // FetchEvent#preloadResponse. See NavigationPreloadLoaderClient's
  // documentation for details.
  void RespondWithNavigationPreloadResponse() {
    response_mode_ = ResponseMode::kNavigationPreloadResponse;
  }

  // Tells this helper to respond to fetch events with the redirect response.
  void RespondWithRedirectResponse(const GURL& new_url) {
    response_mode_ = ResponseMode::kRedirect;
    redirected_url_ = new_url;
  }

  // Tells this helper to simulate failure to dispatch the fetch event to the
  // service worker.
  void FailToDispatchFetchEvent() {
    response_mode_ = ResponseMode::kFailFetchEventDispatch;
  }

  // Tells this helper to simulate "early response", where the respondWith()
  // promise resolves before the waitUntil() promise. In this mode, the
  // helper sets the response mode to "early response", which simulates the
  // promise passed to respondWith() resolving before the waitUntil() promise
  // resolves. In this mode, the helper will respond to fetch events
  // immediately, but will not finish the fetch event until FinishWaitUntil() is
  // called.
  void RespondEarly() { response_mode_ = ResponseMode::kEarlyResponse; }
  void FinishWaitUntil() {
    std::move(finish_callback_)
        .Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED,
             base::TimeTicks::Now());
    base::RunLoop().RunUntilIdle();
  }

  // Tells this helper to wait for FinishRespondWith() to be called before
  // providing the response to the fetch event.
  void DeferResponse() { response_mode_ = ResponseMode::kDeferredResponse; }
  void FinishRespondWith() {
    response_callback_->OnResponse(
        OkResponse(nullptr /* blob_body */),
        blink::mojom::ServiceWorkerFetchEventTiming::New());
    response_callback_.FlushForTesting();
    std::move(finish_callback_)
        .Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED,
             base::TimeTicks::Now());
  }

  void ReadRequestBody(std::string* out_string) {
    ASSERT_TRUE(request_body_);
    const std::vector<network::DataElement>* elements =
        request_body_->elements();
    // So far this test expects a single bytes element.
    ASSERT_EQ(1u, elements->size());
    const network::DataElement& element = elements->front();
    ASSERT_EQ(network::DataElement::TYPE_BYTES, element.type());
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
  void OnFetchEvent(
      int embedded_worker_id,
      const network::ResourceRequest& request,
      blink::mojom::FetchEventPreloadHandlePtr preload_handle,
      blink::mojom::ServiceWorkerFetchResponseCallbackPtr response_callback,
      mojom::ServiceWorker::DispatchFetchEventCallback finish_callback)
      override {
    // Basic checks on DispatchFetchEvent parameters.
    EXPECT_TRUE(ServiceWorkerUtils::IsMainResourceType(
        static_cast<ResourceType>(request.resource_type)));

    has_received_fetch_event_ = true;
    request_body_ = request.request_body;

    switch (response_mode_) {
      case ResponseMode::kDefault:
        EmbeddedWorkerTestHelper::OnFetchEvent(
            embedded_worker_id, request, std::move(preload_handle),
            std::move(response_callback), std::move(finish_callback));
        break;
      case ResponseMode::kBlob:
        response_callback->OnResponse(
            OkResponse(std::move(blob_body_)),
            blink::mojom::ServiceWorkerFetchEventTiming::New());
        std::move(finish_callback)
            .Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED,
                 base::TimeTicks::Now());
        break;
      case ResponseMode::kStream:
        response_callback->OnResponseStream(
            OkResponse(nullptr /* blob_body */), std::move(stream_handle_),
            blink::mojom::ServiceWorkerFetchEventTiming::New());
        std::move(finish_callback)
            .Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED,
                 base::TimeTicks::Now());
        break;
      case ResponseMode::kFallbackResponse:
        response_callback->OnFallback(
            blink::mojom::ServiceWorkerFetchEventTiming::New());
        std::move(finish_callback)
            .Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED,
                 base::TimeTicks::Now());
        break;
      case ResponseMode::kErrorResponse:
        response_callback->OnResponse(
            ErrorResponse(),
            blink::mojom::ServiceWorkerFetchEventTiming::New());
        std::move(finish_callback)
            .Run(blink::mojom::ServiceWorkerEventStatus::REJECTED,
                 base::TimeTicks::Now());
        break;
      case ResponseMode::kNavigationPreloadResponse:
        // Deletes itself when done.
        new NavigationPreloadLoaderClient(std::move(preload_handle),
                                          std::move(response_callback),
                                          std::move(finish_callback));
        break;
      case ResponseMode::kFailFetchEventDispatch:
        // Simulate failure by stopping the worker before the event finishes.
        // This causes ServiceWorkerVersion::StartRequest() to call its error
        // callback, which triggers ServiceWorkerNavigationLoader's dispatch
        // failed behavior.
        SimulateWorkerStopped(embedded_worker_id);
        // Finish the event by calling |finish_callback|.
        // This is the Mojo callback for
        // mojom::ServiceWorker::DispatchFetchEvent().
        // If this is not called, Mojo will complain. In production code,
        // ServiceWorkerContextClient would call this when it aborts all
        // callbacks after an unexpected stop.
        std::move(finish_callback)
            .Run(blink::mojom::ServiceWorkerEventStatus::ABORTED,
                 base::TimeTicks::Now());
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
            .Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED,
                 base::TimeTicks::Now());
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
    kNavigationPreloadResponse,
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
  mojom::ServiceWorker::DispatchFetchEventCallback finish_callback_;
  blink::mojom::ServiceWorkerFetchResponseCallbackPtr response_callback_;

  // For ResponseMode::kRedirect.
  GURL redirected_url_;

  bool has_received_fetch_event_ = false;
  base::OnceClosure quit_closure_for_fetch_event_;

  DISALLOW_COPY_AND_ASSIGN(Helper);
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
//
// ServiceWorkerNavigationLoaderTest is also a
// ServiceWorkerNavigationLoader::Delegate. In production code,
// ServiceWorkerControlleeRequestHandler is the Delegate. So this class also
// basically mocks that part of ServiceWorkerControlleeRequestHandler.
class ServiceWorkerNavigationLoaderTest
    : public testing::Test,
      public ServiceWorkerNavigationLoader::Delegate {
 public:
  ServiceWorkerNavigationLoaderTest()
      : thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP) {}
  ~ServiceWorkerNavigationLoaderTest() override = default;

  void SetUp() override {
    feature_list_.InitAndEnableFeature(network::features::kNetworkService);
    helper_ = std::make_unique<Helper>();

    // Create an active service worker.
    storage()->LazyInitializeForTest(base::DoNothing());
    base::RunLoop().RunUntilIdle();
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
    storage()->StoreRegistration(registration_.get(), version_.get(),
                                 CreateReceiverOnCurrentThread(&status));
    base::RunLoop().RunUntilIdle();
    ASSERT_EQ(blink::ServiceWorkerStatusCode::kOk, status.value());
  }

  ServiceWorkerStorage* storage() { return helper_->context()->storage(); }

  // Indicates whether ServiceWorkerNavigationLoader decided to handle a
  // request, i.e., it returned a non-null RequestHandler for the request.
  enum class LoaderResult {
    kHandledRequest,
    kDidNotHandleRequest,
  };

  // Starts a request. Returns whether ServiceWorkerNavigationLoader handled the
  // request. If kHandledRequest was returned, the request is ongoing and the
  // caller can use functions like client_.RunUntilComplete() to wait for
  // completion.
  LoaderResult StartRequest(std::unique_ptr<network::ResourceRequest> request) {
    // Start a ServiceWorkerNavigationLoader. It should return a
    // RequestHandler.
    SingleRequestURLLoaderFactory::RequestHandler handler;
    loader_ = std::make_unique<ServiceWorkerNavigationLoader>(
        base::BindOnce(&ReceiveRequestHandler, &handler),
        base::BindOnce(&ServiceWorkerNavigationLoaderTest::Fallback,
                       base::Unretained(this)),
        this, *request,
        base::WrapRefCounted<URLLoaderFactoryGetter>(
            helper_->context()->loader_factory_getter()));
    loader_->ForwardToServiceWorker();
    base::RunLoop().RunUntilIdle();
    if (!handler)
      return LoaderResult::kDidNotHandleRequest;

    // Run the handler. It will load |request.url|.
    std::move(handler).Run(*request, mojo::MakeRequest(&loader_ptr_),
                           client_.CreateInterfacePtr());

    return LoaderResult::kHandledRequest;
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
    request->url = GURL("https://www.example.com/");
    request->method = "GET";
    request->fetch_request_mode = network::mojom::FetchRequestMode::kNavigate;
    request->fetch_credentials_mode =
        network::mojom::FetchCredentialsMode::kInclude;
    request->fetch_redirect_mode = network::mojom::FetchRedirectMode::kManual;
    return request;
  }

 protected:
  // ServiceWorkerNavigationLoader::Delegate -----------------------------------
  void OnPrepareToRestart() override {}

  ServiceWorkerVersion* GetServiceWorkerVersion(
      ServiceWorkerMetrics::URLRequestJobResult* result) override {
    return version_.get();
  }

  bool RequestStillValid(
      ServiceWorkerMetrics::URLRequestJobResult* result) override {
    return true;
  }

  void MainResourceLoadFailed() override {
    was_main_resource_load_failed_called_ = true;
  }

  bool HasWorkInBrowser(ServiceWorkerVersion* version) const {
    return version->HasWorkInBrowser();
  }
  // --------------------------------------------------------------------------

  TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<Helper> helper_;
  scoped_refptr<ServiceWorkerRegistration> registration_;
  scoped_refptr<ServiceWorkerVersion> version_;
  storage::BlobStorageContext blob_context_;
  network::TestURLLoaderClient client_;
  bool was_main_resource_load_failed_called_ = false;
  std::unique_ptr<ServiceWorkerNavigationLoader> loader_;
  network::mojom::URLLoaderPtr loader_ptr_;

  bool did_call_fallback_callback_ = false;
  bool reset_subresource_loader_params_ = false;
  base::OnceClosure quit_closure_for_fallback_callback_;

  base::test::ScopedFeatureList feature_list_;
};

TEST_F(ServiceWorkerNavigationLoaderTest, Basic) {
  base::HistogramTester histogram_tester;

  // Perform the request
  LoaderResult result = StartRequest(CreateRequest());
  EXPECT_EQ(LoaderResult::kHandledRequest, result);
  client_.RunUntilComplete();

  EXPECT_EQ(net::OK, client_.completion_status().error_code);
  const network::ResourceResponseHead& info = client_.response_head();
  EXPECT_EQ(200, info.headers->response_code());
  ExpectResponseInfo(info, *CreateResponseInfoFromServiceWorker());

  histogram_tester.ExpectUniqueSample(kHistogramMainResourceFetchEvent,
                                      blink::ServiceWorkerStatusCode::kOk, 1);
  histogram_tester.ExpectTotalCount(
      "ServiceWorker.LoadTiming.MainFrame.MainResource."
      "ResponseReceivedToCompleted",
      1);
}

TEST_F(ServiceWorkerNavigationLoaderTest, NoActiveWorker) {
  base::HistogramTester histogram_tester;

  // Clear |version_| to make GetServiceWorkerVersion() return null.
  version_ = nullptr;

  // Perform the request.
  LoaderResult result = StartRequest(CreateRequest());
  EXPECT_EQ(LoaderResult::kHandledRequest, result);

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
  helper_->ReadRequestBody(&body);
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
  blink::mojom::BlobRequest request = mojo::MakeRequest(&blob->blob);
  storage::BlobImpl::Create(std::move(blob_handle), std::move(request));
  helper_->RespondWithBlob(std::move(blob));

  // Perform the request.
  LoaderResult result = StartRequest(CreateRequest());
  EXPECT_EQ(LoaderResult::kHandledRequest, result);
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
      "ResponseReceivedToCompleted",
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
  blink::mojom::BlobRequest request = mojo::MakeRequest(&blob->blob);
  storage::BlobImpl::Create(std::move(blob_handle), std::move(request));
  helper_->RespondWithBlob(std::move(blob));

  // Perform the request.
  LoaderResult result = StartRequest(CreateRequest());
  EXPECT_EQ(LoaderResult::kHandledRequest, result);

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
      "ResponseReceivedToCompleted",
      0);
}

TEST_F(ServiceWorkerNavigationLoaderTest, StreamResponse) {
  base::HistogramTester histogram_tester;

  // Construct the Stream to respond with.
  const char kResponseBody[] = "Here is sample text for the Stream.";
  blink::mojom::ServiceWorkerStreamCallbackPtr stream_callback;
  mojo::DataPipe data_pipe;
  helper_->RespondWithStream(mojo::MakeRequest(&stream_callback),
                             std::move(data_pipe.consumer_handle));

  // Perform the request.
  LoaderResult result = StartRequest(CreateRequest());
  EXPECT_EQ(LoaderResult::kHandledRequest, result);
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
      "ResponseReceivedToCompleted",
      1);
}

// Test when a stream response body is aborted.
TEST_F(ServiceWorkerNavigationLoaderTest, StreamResponse_Abort) {
  base::HistogramTester histogram_tester;

  // Construct the Stream to respond with.
  const char kResponseBody[] = "Here is sample text for the Stream.";
  blink::mojom::ServiceWorkerStreamCallbackPtr stream_callback;
  mojo::DataPipe data_pipe;
  helper_->RespondWithStream(mojo::MakeRequest(&stream_callback),
                             std::move(data_pipe.consumer_handle));

  // Perform the request.
  LoaderResult result = StartRequest(CreateRequest());
  EXPECT_EQ(LoaderResult::kHandledRequest, result);
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
      "ResponseReceivedToCompleted",
      0);
}

// Test when the loader is cancelled while a stream response is being written.
TEST_F(ServiceWorkerNavigationLoaderTest, StreamResponseAndCancel) {
  base::HistogramTester histogram_tester;

  // Construct the Stream to respond with.
  const char kResponseBody[] = "Here is sample text for the Stream.";
  blink::mojom::ServiceWorkerStreamCallbackPtr stream_callback;
  mojo::DataPipe data_pipe;
  helper_->RespondWithStream(mojo::MakeRequest(&stream_callback),
                             std::move(data_pipe.consumer_handle));

  // Perform the request.
  LoaderResult result = StartRequest(CreateRequest());
  EXPECT_EQ(LoaderResult::kHandledRequest, result);
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
  EXPECT_TRUE(HasWorkInBrowser(version_.get()));
  loader_ptr_.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(HasWorkInBrowser(version_.get()));

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
      "ResponseReceivedToCompleted",
      0);
}

// Test when the service worker responds with network fallback.
// i.e., does not call respondWith().
TEST_F(ServiceWorkerNavigationLoaderTest, FallbackResponse) {
  base::HistogramTester histogram_tester;
  helper_->RespondWithFallback();

  // Perform the request.
  LoaderResult result = StartRequest(CreateRequest());
  EXPECT_EQ(LoaderResult::kHandledRequest, result);

  // The fallback callback should be called.
  RunUntilFallbackCallback();
  EXPECT_FALSE(reset_subresource_loader_params_);
  EXPECT_FALSE(was_main_resource_load_failed_called_);

  // The request should not be handled by the loader, but it shouldn't be a
  // failure.
  EXPECT_FALSE(was_main_resource_load_failed_called_);
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
  helper_->RespondWithError();

  // Perform the request.
  LoaderResult result = StartRequest(CreateRequest());
  EXPECT_EQ(LoaderResult::kHandledRequest, result);

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
  helper_->FailToDispatchFetchEvent();

  // Perform the request.
  LoaderResult result = StartRequest(CreateRequest());
  EXPECT_EQ(LoaderResult::kHandledRequest, result);

  // The fallback callback should be called.
  RunUntilFallbackCallback();
  EXPECT_TRUE(reset_subresource_loader_params_);
  EXPECT_TRUE(was_main_resource_load_failed_called_);

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
  helper_->RespondEarly();

  // Perform the request.
  LoaderResult result = StartRequest(CreateRequest());
  EXPECT_EQ(LoaderResult::kHandledRequest, result);
  client_.RunUntilComplete();

  const network::ResourceResponseHead& info = client_.response_head();
  EXPECT_EQ(200, info.headers->response_code());
  ExpectResponseInfo(info, *CreateResponseInfoFromServiceWorker());

  // Although the response was already received, the event remains outstanding
  // until waitUntil() resolves.
  EXPECT_TRUE(HasWorkInBrowser(version_.get()));
  helper_->FinishWaitUntil();
  EXPECT_FALSE(HasWorkInBrowser(version_.get()));
}

// Test asking the loader to fallback to network. In production code, this
// happens when there is no active service worker for the URL, or it must be
// skipped, etc.
TEST_F(ServiceWorkerNavigationLoaderTest, FallbackToNetwork) {
  base::HistogramTester histogram_tester;

  network::ResourceRequest request;
  request.url = GURL("https://www.example.com/");
  request.method = "GET";
  request.fetch_request_mode = network::mojom::FetchRequestMode::kNavigate;
  request.fetch_credentials_mode =
      network::mojom::FetchCredentialsMode::kInclude;
  request.fetch_redirect_mode = network::mojom::FetchRedirectMode::kManual;

  SingleRequestURLLoaderFactory::RequestHandler handler;
  auto loader = std::make_unique<ServiceWorkerNavigationLoader>(
      base::BindOnce(&ReceiveRequestHandler, &handler),
      base::BindOnce(&ServiceWorkerNavigationLoaderTest::Fallback,
                     base::Unretained(this)),
      this, request,
      base::WrapRefCounted<URLLoaderFactoryGetter>(
          helper_->context()->loader_factory_getter()));
  // Ask the loader to fallback to network. In production code,
  // ServiceWorkerControlleeRequestHandler calls FallbackToNetwork() to do this.
  loader->FallbackToNetwork();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(handler);

  // No fetch event was dispatched.
  histogram_tester.ExpectTotalCount(kHistogramMainResourceFetchEvent, 0);
  histogram_tester.ExpectTotalCount(
      "ServiceWorker.LoadTiming.MainFrame.MainResource."
      "StartToForwardServiceWorker",
      0);
}

// Test responding to the fetch event with the navigation preload response.
TEST_F(ServiceWorkerNavigationLoaderTest, NavigationPreload) {
  registration_->EnableNavigationPreload(true);
  helper_->RespondWithNavigationPreloadResponse();

  // Perform the request
  LoaderResult result = StartRequest(CreateRequest());
  ASSERT_EQ(LoaderResult::kHandledRequest, result);
  client_.RunUntilComplete();

  EXPECT_EQ(net::OK, client_.completion_status().error_code);
  const network::ResourceResponseHead& info = client_.response_head();
  EXPECT_EQ(200, info.headers->response_code());

  std::unique_ptr<network::ResourceResponseHead> expected_info =
      CreateResponseInfoFromServiceWorker();
  expected_info->did_service_worker_navigation_preload = true;
  ExpectResponseInfo(info, *expected_info);

  std::string response;
  EXPECT_TRUE(client_.response_body().is_valid());
  EXPECT_TRUE(
      mojo::BlockingCopyToString(client_.response_body_release(), &response));
  EXPECT_EQ("this body came from the network", response);
}

// Test responding to the fetch event with a redirect response.
TEST_F(ServiceWorkerNavigationLoaderTest, Redirect) {
  base::HistogramTester histogram_tester;
  GURL new_url("https://example.com/redirected");
  helper_->RespondWithRedirectResponse(new_url);

  // Perform the request.
  LoaderResult result = StartRequest(CreateRequest());
  EXPECT_EQ(LoaderResult::kHandledRequest, result);
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

TEST_F(ServiceWorkerNavigationLoaderTest, LifetimeAfterForwardToServiceWorker) {
  LoaderResult result = StartRequest(CreateRequest());
  EXPECT_EQ(LoaderResult::kHandledRequest, result);
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

TEST_F(ServiceWorkerNavigationLoaderTest, LifetimeAfterFallbackToNetwork) {
  network::ResourceRequest request;
  request.url = GURL("https://www.example.com/");
  request.method = "GET";
  request.fetch_request_mode = network::mojom::FetchRequestMode::kNavigate;
  request.fetch_credentials_mode =
      network::mojom::FetchCredentialsMode::kInclude;
  request.fetch_redirect_mode = network::mojom::FetchRedirectMode::kManual;

  SingleRequestURLLoaderFactory::RequestHandler handler;
  auto loader = std::make_unique<ServiceWorkerNavigationLoader>(
      base::BindOnce(&ReceiveRequestHandler, &handler),
      base::BindOnce(&ServiceWorkerNavigationLoaderTest::Fallback,
                     base::Unretained(this)),
      this, request,
      base::WrapRefCounted<URLLoaderFactoryGetter>(
          helper_->context()->loader_factory_getter()));
  base::WeakPtr<ServiceWorkerNavigationLoader> loader_weakptr =
      loader->AsWeakPtr();
  // Ask the loader to fallback to network. In production code,
  // ServiceWorkerControlleeRequestHandler calls FallbackToNetwork() to do this.
  loader->FallbackToNetwork();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(handler);
  EXPECT_TRUE(loader_weakptr);

  // DetachedFromRequest() deletes |loader_|.
  loader.release()->DetachedFromRequest();
  EXPECT_FALSE(loader_weakptr);
}

TEST_F(ServiceWorkerNavigationLoaderTest, ConnectionErrorDuringFetchEvent) {
  helper_->DeferResponse();
  LoaderResult result = StartRequest(CreateRequest());
  EXPECT_EQ(LoaderResult::kHandledRequest, result);

  // Wait for the fetch event to be dispatched.
  helper_->RunUntilFetchEvent();

  // Break the Mojo connection. The loader should return an aborted status.
  loader_ptr_.reset();
  client_.RunUntilComplete();
  EXPECT_EQ(net::ERR_ABORTED, client_.completion_status().error_code);

  // The loader is still alive. Finish the fetch event. It shouldn't crash or
  // call any callbacks on |client_|, which would throw an error.
  helper_->FinishRespondWith();
  // There's no event to wait for, so just pump the message loop and the test
  // passes if there is no error or crash.
  base::RunLoop().RunUntilIdle();
}

TEST_F(ServiceWorkerNavigationLoaderTest, DetachedDuringFetchEvent) {
  LoaderResult result = StartRequest(CreateRequest());
  EXPECT_EQ(LoaderResult::kHandledRequest, result);

  // Detach the loader immediately after it started. This results in
  // DidDispatchFetchEvent() being invoked later with null |delegate_|.
  loader_.release()->DetachedFromRequest();
  client_.RunUntilComplete();
  EXPECT_EQ(net::ERR_ABORTED, client_.completion_status().error_code);
}

}  // namespace service_worker_navigation_loader_unittest
}  // namespace content
