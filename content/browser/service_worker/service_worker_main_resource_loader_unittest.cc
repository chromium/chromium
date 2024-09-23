// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_main_resource_loader.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "content/browser/loader/navigation_loader_interceptor.h"
#include "content/browser/loader/response_head_update_params.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/fake_embedded_worker_instance_client.h"
#include "content/browser/service_worker/fake_service_worker.h"
#include "content/browser/service_worker/service_worker_client.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_fetch_dispatcher.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_process_host.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/single_request_url_loader_factory.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_client.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_impl.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/service_worker/service_worker.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_event_status.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration_options.mojom.h"
#include "url/origin.h"

namespace content {
namespace service_worker_main_resource_loader_unittest {

class ScopedOverrideToDisableHighPriorityFetchResponseCallback {
 public:
  ScopedOverrideToDisableHighPriorityFetchResponseCallback() {
    ServiceWorkerFetchDispatcher::
        ForceDisableHighPriorityFetchResponseCallbackForTesting(
            /*force_disable=*/true);
  }
  ScopedOverrideToDisableHighPriorityFetchResponseCallback(
      const ScopedOverrideToDisableHighPriorityFetchResponseCallback&) = delete;
  ScopedOverrideToDisableHighPriorityFetchResponseCallback& operator=(
      const ScopedOverrideToDisableHighPriorityFetchResponseCallback&) = delete;
  ~ScopedOverrideToDisableHighPriorityFetchResponseCallback() {
    ServiceWorkerFetchDispatcher::
        ForceDisableHighPriorityFetchResponseCallbackForTesting(
            /*force_disable=*/false);
  }
};

void ReceiveRequestHandler(
    network::SingleRequestURLLoaderFactory::RequestHandler* out_handler,
    network::SingleRequestURLLoaderFactory::RequestHandler handler) {
  *out_handler = std::move(handler);
}

blink::mojom::FetchAPIResponsePtr OkResponse(
    blink::mojom::SerializedBlobPtr blob_body,
    network::mojom::FetchResponseSource response_source,
    base::Time response_time,
    std::string cache_storage_cache_name) {
  auto response = blink::mojom::FetchAPIResponse::New();
  response->status_code = 200;
  response->status_text = "OK";
  response->response_type = network::mojom::FetchResponseType::kDefault;
  response->response_source = response_source;
  response->response_time = response_time;
  response->cache_storage_cache_name = cache_storage_cache_name;
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

blink::mojom::FetchAPIResponsePtr HeadersResponse(
    const base::flat_map<std::string, std::string>& headers) {
  auto response = blink::mojom::FetchAPIResponse::New();
  response->status_code = 200;
  response->status_text = "OK";
  response->headers.insert(headers.begin(), headers.end());
  return response;
}

// ServiceWorkerMainResourceLoader::RecordTimingMetrics() records the metrics
// only when the consistent high-resolution timer is used among processes.
bool LoaderRecordsTimingMetrics() {
  return base::TimeTicks::IsHighResolution() &&
         base::TimeTicks::IsConsistentAcrossProcesses();
}

// Simulates a service worker handling fetch events. The response can be
// customized via RespondWith* functions.
class FetchEventServiceWorker : public FakeServiceWorker {
 public:
  FetchEventServiceWorker(
      EmbeddedWorkerTestHelper* helper,
      FakeEmbeddedWorkerInstanceClient* embedded_worker_instance_client,
      BrowserTaskEnvironment* task_environment)
      : FakeServiceWorker(helper),
        task_environment_(task_environment),
        embedded_worker_instance_client_(embedded_worker_instance_client) {}

  FetchEventServiceWorker(const FetchEventServiceWorker&) = delete;
  FetchEventServiceWorker& operator=(const FetchEventServiceWorker&) = delete;

  ~FetchEventServiceWorker() override = default;

  // Tells this worker to dispatch a fetch event 1s after the fetch event is
  // received.
  void DispatchAfter1sDelay() {
    response_mode_ = ResponseMode::kDispatchAfter1sDelay;
  }

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

  // Tells this worker to respond to fetch events with a response containing
  // specific headers.
  void RespondWithHeaders(
      const base::flat_map<std::string, std::string>& headers) {
    response_mode_ = ResponseMode::kHeaders;
    headers_ = headers;
  }

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
        OkResponse(nullptr /* blob_body */, response_source_, response_time_,
                   cache_storage_cache_name_),
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
    ASSERT_EQ(network::DataElement::Tag::kBytes, element.type());
    *out_string =
        std::string(element.As<network::DataElementBytes>().AsStringPiece());
  }

  void RunUntilFetchEvent() {
    if (has_received_fetch_event_)
      return;
    base::RunLoop run_loop;
    quit_closure_for_fetch_event_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void SetResponseSource(network::mojom::FetchResponseSource source) {
    response_source_ = source;
  }

  void SetCacheStorageCacheName(std::string cache_name) {
    cache_storage_cache_name_ = cache_name;
  }

  void SetResponseTime(base::Time time) { response_time_ = time; }

  void WaitForTransferInstalledScript() {
    embedded_worker_instance_client_->WaitForTransferInstalledScript();
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

    auto timing = blink::mojom::ServiceWorkerFetchEventTiming::New();
    auto now = base::TimeTicks::Now();
    timing->dispatch_event_time = now;
    timing->respond_with_settled_time = now;

    mojo::Remote<blink::mojom::ServiceWorkerFetchResponseCallback>
        response_callback(std::move(pending_response_callback));
    switch (response_mode_) {
      case ResponseMode::kDefault:
        FakeServiceWorker::DispatchFetchEventForMainResource(
            std::move(params), response_callback.Unbind(),
            std::move(finish_callback));
        break;
      case ResponseMode::kDispatchAfter1sDelay:
        task_environment_->AdvanceClock(base::Seconds(1));
        FakeServiceWorker::DispatchFetchEventForMainResource(
            std::move(params), response_callback.Unbind(),
            std::move(finish_callback));
        break;
      case ResponseMode::kBlob:
        response_callback->OnResponse(
            OkResponse(std::move(blob_body_), response_source_, response_time_,
                       cache_storage_cache_name_),
            std::move(timing));
        std::move(finish_callback)
            .Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED);
        break;
      case ResponseMode::kStream:
        response_callback->OnResponseStream(
            OkResponse(nullptr /* blob_body */, response_source_,
                       response_time_, cache_storage_cache_name_),
            std::move(stream_handle_), std::move(timing));

        std::move(finish_callback)
            .Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED);
        break;
      case ResponseMode::kFallbackResponse:
        response_callback->OnFallback(/*request_body=*/std::nullopt,
                                      std::move(timing));
        std::move(finish_callback)
            .Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED);
        break;
      case ResponseMode::kErrorResponse:
        response_callback->OnResponse(ErrorResponse(), std::move(timing));
        std::move(finish_callback)
            .Run(blink::mojom::ServiceWorkerEventStatus::REJECTED);
        break;
      case ResponseMode::kFailFetchEventDispatch:
        // Simulate failure by stopping the worker before the event finishes.
        // This causes ServiceWorkerVersion::StartRequest() to call its error
        // callback, which triggers ServiceWorkerMainResourceLoader's dispatch
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
            OkResponse(nullptr /* blob_body */, response_source_,
                       response_time_, cache_storage_cache_name_),
            std::move(timing));
        // Now the caller must call FinishWaitUntil() to finish the event.
        break;
      case ResponseMode::kRedirect:
        response_callback->OnResponse(RedirectResponse(redirected_url_.spec()),
                                      std::move(timing));
        std::move(finish_callback)
            .Run(blink::mojom::ServiceWorkerEventStatus::COMPLETED);
        break;
      case ResponseMode::kHeaders:
        response_callback->OnResponse(HeadersResponse(headers_),
                                      std::move(timing));
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
    kDispatchAfter1sDelay,
    kBlob,
    kStream,
    kFallbackResponse,
    kErrorResponse,
    kFailFetchEventDispatch,
    kDeferredResponse,
    kEarlyResponse,
    kRedirect,
    kHeaders
  };

  const raw_ptr<BrowserTaskEnvironment> task_environment_;

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

  // For ResponseMode::kHeaders
  base::flat_map<std::string, std::string> headers_;

  bool has_received_fetch_event_ = false;
  base::OnceClosure quit_closure_for_fetch_event_;

  const raw_ptr<FakeEmbeddedWorkerInstanceClient>
      embedded_worker_instance_client_;

  network::mojom::FetchResponseSource response_source_ =
      network::mojom::FetchResponseSource::kUnspecified;

  std::string cache_storage_cache_name_;
  base::Time response_time_;
};

// Returns typical response info for a resource load that went through a service
// worker.
network::mojom::URLResponseHeadPtr CreateResponseInfoFromServiceWorker() {
  auto head = network::mojom::URLResponseHead::New();
  head->was_fetched_via_service_worker = true;
  head->url_list_via_service_worker = std::vector<GURL>();
  head->response_type = network::mojom::FetchResponseType::kDefault;
  head->cache_storage_cache_name = std::string();
  head->did_service_worker_navigation_preload = false;
  return head;
}

const char kHistogramMainResourceFetchEvent[] =
    "ServiceWorker.FetchEvent.MainResource.Status";

// ServiceWorkerMainResourceLoaderTest is for testing the handling of requests
// by a service worker via ServiceWorkerMainResourceLoader.
//
// Of course, no actual service worker runs in the unit test, it is simulated
// via EmbeddedWorkerTestHelper receiving IPC messages from the browser and
// responding as if a service worker is running in the renderer.
class ServiceWorkerMainResourceLoaderTest : public testing::Test {
 public:
  ServiceWorkerMainResourceLoaderTest()
      : task_environment_(BrowserTaskEnvironment::IO_MAINLOOP,
                          base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~ServiceWorkerMainResourceLoaderTest() override = default;

  void SetUp() override {
    helper_ = std::make_unique<EmbeddedWorkerTestHelper>(base::FilePath());

    // Create an active service worker.
    blink::mojom::ServiceWorkerRegistrationOptions options;
    options.scope = GURL("https://example.com/");
    registration_ = CreateNewServiceWorkerRegistration(
        helper_->context()->registry(), options,
        blink::StorageKey::CreateFirstParty(
            url::Origin::Create(options.scope)));
    version_ = CreateNewServiceWorkerVersion(
        helper_->context()->registry(), registration_.get(),
        GURL("https://example.com/service_worker.js"),
        blink::mojom::ScriptType::kClassic);
    std::vector<storage::mojom::ServiceWorkerResourceRecordPtr> records;
    records.push_back(WriteToDiskCacheSync(
        GetStorageControl(), version_->script_url(), {} /* headers */,
        "I'm the body", "I'm the meta data"));
    version_->script_cache_map()->SetResources(records);
    version_->set_fetch_handler_type(
        ServiceWorkerVersion::FetchHandlerType::kNotSkippable);
    version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
    PolicyContainerPolicies policies;
    policies.ip_address_space = network::mojom::IPAddressSpace::kPrivate;
    version_->set_policy_container_host(
        base::MakeRefCounted<PolicyContainerHost>(std::move(policies)));
    registration_->SetActiveVersion(version_);

    // Make the registration findable via storage functions.
    registration_->set_last_update_check(base::Time::Now());
    std::optional<blink::ServiceWorkerStatusCode> status;
    base::RunLoop run_loop;
    registry()->StoreRegistration(
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
            helper_.get(), client, &task_environment_);

    // Wait for main script response is set to |version| because
    // ServiceWorkerMainResourceLoader needs the main script response to
    // create a response. The main script response is set when the first
    // TransferInstalledScript().
    {
      base::RunLoop loop;
      version_->StartWorker(
          ServiceWorkerMetrics::EventType::UNKNOWN,
          ReceiveServiceWorkerStatus(&status, loop.QuitClosure()));
      loop.Run();
      ASSERT_EQ(blink::ServiceWorkerStatusCode::kOk, status.value());
      service_worker_->WaitForTransferInstalledScript();
    }
  }

  ServiceWorkerRegistry* registry() { return helper_->context()->registry(); }
  mojo::Remote<storage::mojom::ServiceWorkerStorageControl>&
  GetStorageControl() {
    return helper_->context()->GetStorageControl();
  }

  // Starts a request. After calling this, the request is ongoing and the
  // caller can use functions like client_.RunUntilComplete() to wait for
  // completion.
  void StartRequest(std::unique_ptr<network::ResourceRequest> request) {
    // Create a ServiceWorkerClient and simulate what
    // ServiceWorkerControlleeRequestHandler does to assign it a controller.
    if (!service_worker_client_) {
      service_worker_client_ = std::make_unique<ScopedServiceWorkerClient>(
          CreateServiceWorkerClient(helper_->context(), request->url));
      service_worker_client()->AddMatchingRegistration(registration_.get());
      service_worker_client()->SetControllerRegistration(
          registration_, /*notify_controllerchange=*/false);
    }

    // Create a ServiceWorkerMainResourceLoader.
    loader_ = std::make_unique<ServiceWorkerMainResourceLoader>(
        base::BindOnce(&ServiceWorkerMainResourceLoaderTest::Fallback,
                       base::Unretained(this)),
        service_worker_client()->AsWeakPtr(), FrameTreeNodeId(),
        /*find_registration_start_time=*/base::TimeTicks::Now());

    // Load |request.url|.
    loader_->StartRequest(*request, loader_remote_.BindNewPipeAndPassReceiver(),
                          client_.CreateRemote());
  }

  // The |fallback_callback| passed to the ServiceWorkerMainResourceLoader in
  // StartRequest().
  void Fallback(ResponseHeadUpdateParams) {
    did_call_fallback_callback_ = true;
    if (quit_closure_for_fallback_callback_)
      std::move(quit_closure_for_fallback_callback_).Run();
  }

  // Runs until the ServiceWorkerMainResourceLoader created in StartRequest()
  // calls the |fallback_callback| given to it. The argument passed to
  // |fallback_callback| is saved in |reset_subresurce_loader_params_|.
  void RunUntilFallbackCallback() {
    if (did_call_fallback_callback_)
      return;
    base::RunLoop run_loop;
    quit_closure_for_fallback_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void ExpectResponseInfo(
      const network::mojom::URLResponseHead& info,
      const network::mojom::URLResponseHead& expected_info) {
    EXPECT_EQ(expected_info.was_fetched_via_service_worker,
              info.was_fetched_via_service_worker);
    EXPECT_EQ(expected_info.url_list_via_service_worker,
              info.url_list_via_service_worker);
    EXPECT_EQ(expected_info.response_type, info.response_type);
    EXPECT_EQ(expected_info.response_time, info.response_time);
    EXPECT_FALSE(info.load_timing.service_worker_start_time.is_null());
    EXPECT_FALSE(info.load_timing.service_worker_ready_time.is_null());
    EXPECT_FALSE(info.load_timing.service_worker_fetch_start.is_null());
    EXPECT_FALSE(
        info.load_timing.service_worker_respond_with_settled.is_null());
    EXPECT_LE(info.load_timing.service_worker_start_time,
              info.load_timing.service_worker_ready_time);
    EXPECT_LE(info.load_timing.service_worker_ready_time,
              info.load_timing.service_worker_fetch_start);
    EXPECT_LE(info.load_timing.service_worker_fetch_start,
              info.load_timing.service_worker_respond_with_settled);
    EXPECT_EQ(expected_info.service_worker_response_source,
              info.service_worker_response_source);
    EXPECT_EQ(expected_info.cache_storage_cache_name,
              info.cache_storage_cache_name);
    EXPECT_EQ(expected_info.did_service_worker_navigation_preload,
              info.did_service_worker_navigation_preload);
    // TODO(crbug.com/40944544): Write tests about Static Routing API, in
    // particular, checking the correctness of `service_worker_router_info`.
  }

  std::unique_ptr<network::ResourceRequest> CreateRequest() {
    std::unique_ptr<network::ResourceRequest> request =
        std::make_unique<network::ResourceRequest>();
    request->url = GURL("https://example.com/");
    request->method = "GET";
    request->mode = network::mojom::RequestMode::kNavigate;
    request->credentials_mode = network::mojom::CredentialsMode::kInclude;
    request->redirect_mode = network::mojom::RedirectMode::kManual;
    request->destination = network::mojom::RequestDestination::kDocument;
    return request;
  }

  bool HasWorkInBrowser(ServiceWorkerVersion* version) const {
    return version->HasWorkInBrowser();
  }

 protected:
  ServiceWorkerClient* service_worker_client() const {
    return service_worker_client_->get();
  }

  BrowserTaskEnvironment task_environment_;
  std::unique_ptr<EmbeddedWorkerTestHelper> helper_;
  scoped_refptr<ServiceWorkerRegistration> registration_;
  scoped_refptr<ServiceWorkerVersion> version_;
  raw_ptr<FetchEventServiceWorker> service_worker_;
  storage::BlobStorageContext blob_context_;
  network::TestURLLoaderClient client_;
  std::unique_ptr<ServiceWorkerMainResourceLoader> loader_;
  mojo::Remote<network::mojom::URLLoader> loader_remote_;
  std::unique_ptr<ScopedServiceWorkerClient> service_worker_client_;

  bool did_call_fallback_callback_ = false;
  base::OnceClosure quit_closure_for_fallback_callback_;
};

TEST_F(ServiceWorkerMainResourceLoaderTest, Basic) {
  base::HistogramTester histogram_tester;

  // Perform the request.
  StartRequest(CreateRequest());
  client_.RunUntilComplete();

  EXPECT_EQ(net::OK, client_.completion_status().error_code);
  auto& info = client_.response_head();
  EXPECT_EQ(200, info->headers->response_code());
  EXPECT_FALSE(info->load_timing.receive_headers_start.is_null());
  EXPECT_FALSE(info->load_timing.receive_headers_end.is_null());
  EXPECT_LE(info->load_timing.receive_headers_start,
            info->load_timing.receive_headers_end);
  EXPECT_TRUE(info->was_fetched_via_service_worker);
  EXPECT_EQ(info->client_address_space,
            network::mojom::IPAddressSpace::kPrivate);
  ExpectResponseInfo(*info, *CreateResponseInfoFromServiceWorker());

  histogram_tester.ExpectUniqueSample(kHistogramMainResourceFetchEvent,
                                      blink::ServiceWorkerStatusCode::kOk, 1);

  if (LoaderRecordsTimingMetrics()) {
    histogram_tester.ExpectTotalCount(
        "ServiceWorker.LoadTiming.MainFrame.MainResource."
        "ResponseReceivedToCompleted2",
        1);
  }
}

TEST_F(ServiceWorkerMainResourceLoaderTest, NoActiveWorker) {
  base::HistogramTester histogram_tester;

  // Make a container host without a controller.
  service_worker_client_ =
      std::make_unique<ScopedServiceWorkerClient>(CreateServiceWorkerClient(
          helper_->context(), GURL("https://example.com/")));

  // Perform the request.
  StartRequest(CreateRequest());
  client_.RunUntilComplete();
  EXPECT_EQ(net::ERR_FAILED, client_.completion_status().error_code);

  // No fetch event was dispatched.
  histogram_tester.ExpectTotalCount(kHistogramMainResourceFetchEvent, 0);
  if (LoaderRecordsTimingMetrics()) {
    histogram_tester.ExpectTotalCount(
        "ServiceWorker.LoadTiming.MainFrame.MainResource."
        "StartToForwardServiceWorker",
        0);
  }
}

// Test that the request body is passed to the fetch event.
TEST_F(ServiceWorkerMainResourceLoaderTest, RequestBody) {
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

TEST_F(ServiceWorkerMainResourceLoaderTest, BlobResponse) {
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
  service_worker_->SetResponseSource(
      network::mojom::FetchResponseSource::kCacheStorage);
  std::string cache_name = "v1";
  service_worker_->SetCacheStorageCacheName(cache_name);
  base::Time response_time = base::Time::Now();
  service_worker_->SetResponseTime(response_time);

  // Perform the request.
  StartRequest(CreateRequest());
  client_.RunUntilComplete();

  auto& info = client_.response_head();
  EXPECT_EQ(200, info->headers->response_code());
  auto expected_info = CreateResponseInfoFromServiceWorker();
  expected_info->response_time = response_time;
  expected_info->cache_storage_cache_name = cache_name;
  expected_info->service_worker_response_source =
      network::mojom::FetchResponseSource::kCacheStorage;
  ExpectResponseInfo(*info, *expected_info);
  EXPECT_EQ(33, info->content_length);

  // Test the body.
  std::string body;
  EXPECT_TRUE(client_.response_body().is_valid());
  EXPECT_TRUE(
      mojo::BlockingCopyToString(client_.response_body_release(), &body));
  EXPECT_EQ(kResponseBody, body);
  EXPECT_EQ(net::OK, client_.completion_status().error_code);

  if (LoaderRecordsTimingMetrics()) {
    // Test histogram of reading body.
    histogram_tester.ExpectTotalCount(
        "ServiceWorker.LoadTiming.MainFrame.MainResource."
        "ResponseReceivedToCompleted2",
        1);
  }
}

// Tell the helper to respond with a non-existent Blob.
TEST_F(ServiceWorkerMainResourceLoaderTest, BrokenBlobResponse) {
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
  auto& info = client_.response_head();
  EXPECT_EQ(200, info->headers->response_code());
  ExpectResponseInfo(*info, *CreateResponseInfoFromServiceWorker());

  // However, since the blob is broken we should get an error while transferring
  // the body.
  client_.RunUntilComplete();
  EXPECT_EQ(net::ERR_OUT_OF_MEMORY, client_.completion_status().error_code);

  if (LoaderRecordsTimingMetrics()) {
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
}

TEST_F(ServiceWorkerMainResourceLoaderTest, StreamResponse) {
  base::HistogramTester histogram_tester;

  // Construct the Stream to respond with.
  const std::string_view kResponseBody = "Here is sample text for the Stream.";
  mojo::Remote<blink::mojom::ServiceWorkerStreamCallback> stream_callback;
  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  ASSERT_EQ(mojo::CreateDataPipe(nullptr, producer_handle, consumer_handle),
            MOJO_RESULT_OK);
  service_worker_->RespondWithStream(
      stream_callback.BindNewPipeAndPassReceiver(), std::move(consumer_handle));

  // Perform the request.
  StartRequest(CreateRequest());
  client_.RunUntilResponseReceived();

  auto& info = client_.response_head();
  EXPECT_EQ(200, info->headers->response_code());
  ExpectResponseInfo(*info, *CreateResponseInfoFromServiceWorker());

  EXPECT_FALSE(version_->HasNoWork());

  // Write the body stream.
  size_t actually_written_bytes = 0;
  MojoResult mojo_result = producer_handle->WriteData(
      base::as_byte_span(kResponseBody), MOJO_WRITE_DATA_FLAG_NONE,
      actually_written_bytes);
  ASSERT_EQ(MOJO_RESULT_OK, mojo_result);
  EXPECT_EQ(kResponseBody.size(), actually_written_bytes);
  stream_callback->OnCompleted();
  producer_handle.reset();

  client_.RunUntilComplete();
  EXPECT_EQ(net::OK, client_.completion_status().error_code);

  // Test the body.
  std::string response;
  EXPECT_TRUE(client_.response_body().is_valid());
  EXPECT_TRUE(
      mojo::BlockingCopyToString(client_.response_body_release(), &response));
  EXPECT_EQ(kResponseBody, response);

  if (LoaderRecordsTimingMetrics()) {
    // Test histogram of reading body.
    histogram_tester.ExpectTotalCount(
        "ServiceWorker.LoadTiming.MainFrame.MainResource."
        "ResponseReceivedToCompleted2",
        1);
  }
}

// Test when a stream response body is aborted.
TEST_F(ServiceWorkerMainResourceLoaderTest, StreamResponse_Abort) {
  base::HistogramTester histogram_tester;

  // Construct the Stream to respond with.
  const std::string_view kResponseBody = "Here is sample text for the Stream.";
  mojo::Remote<blink::mojom::ServiceWorkerStreamCallback> stream_callback;
  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  ASSERT_EQ(mojo::CreateDataPipe(nullptr, producer_handle, consumer_handle),
            MOJO_RESULT_OK);
  service_worker_->RespondWithStream(
      stream_callback.BindNewPipeAndPassReceiver(), std::move(consumer_handle));

  // Perform the request.
  StartRequest(CreateRequest());
  client_.RunUntilResponseReceived();

  auto& info = client_.response_head();
  EXPECT_EQ(200, info->headers->response_code());
  ExpectResponseInfo(*info, *CreateResponseInfoFromServiceWorker());

  // Start writing the body stream, then abort before finishing.
  size_t actually_written_bytes = 0;
  MojoResult mojo_result = producer_handle->WriteData(
      base::as_byte_span(kResponseBody), MOJO_WRITE_DATA_FLAG_NONE,
      actually_written_bytes);
  ASSERT_EQ(MOJO_RESULT_OK, mojo_result);
  EXPECT_EQ(kResponseBody.size(), actually_written_bytes);
  stream_callback->OnAborted();
  producer_handle.reset();

  client_.RunUntilComplete();
  EXPECT_EQ(net::ERR_ABORTED, client_.completion_status().error_code);

  // Test the body.
  std::string response;
  EXPECT_TRUE(client_.response_body().is_valid());
  EXPECT_TRUE(
      mojo::BlockingCopyToString(client_.response_body_release(), &response));
  EXPECT_EQ(kResponseBody, response);

  if (LoaderRecordsTimingMetrics()) {
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
}

// Test when the loader is cancelled while a stream response is being written.
TEST_F(ServiceWorkerMainResourceLoaderTest, StreamResponseAndCancel) {
  base::HistogramTester histogram_tester;

  // Construct the Stream to respond with.
  const std::string_view kResponseBody = "Here is sample text for the Stream.";
  mojo::Remote<blink::mojom::ServiceWorkerStreamCallback> stream_callback;
  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  ASSERT_EQ(mojo::CreateDataPipe(nullptr, producer_handle, consumer_handle),
            MOJO_RESULT_OK);
  service_worker_->RespondWithStream(
      stream_callback.BindNewPipeAndPassReceiver(), std::move(consumer_handle));

  // Perform the request.
  StartRequest(CreateRequest());
  client_.RunUntilResponseReceived();

  auto& info = client_.response_head();
  EXPECT_EQ(200, info->headers->response_code());
  ExpectResponseInfo(*info, *CreateResponseInfoFromServiceWorker());

  // Start writing the body stream, then break the Mojo connection to the loader
  // before finishing.
  size_t actually_written_bytes = 0;
  MojoResult mojo_result = producer_handle->WriteData(
      base::as_byte_span(kResponseBody), MOJO_WRITE_DATA_FLAG_NONE,
      actually_written_bytes);
  ASSERT_EQ(MOJO_RESULT_OK, mojo_result);
  EXPECT_EQ(kResponseBody.size(), actually_written_bytes);
  EXPECT_TRUE(producer_handle.is_valid());
  loader_remote_.reset();
  base::RunLoop().RunUntilIdle();

  // Although ServiceWorkerMainResourceLoader resets its URLLoaderClient pointer
  // on connection error, the URLLoaderClient still exists. In this test, it is
  // |client_| which owns the data pipe, so it's still valid to write data to
  // it.
  mojo_result = producer_handle->WriteData(base::as_byte_span(kResponseBody),
                                           MOJO_WRITE_DATA_FLAG_NONE,
                                           actually_written_bytes);
  // TODO(falken): This should probably be an error.
  EXPECT_EQ(MOJO_RESULT_OK, mojo_result);

  client_.RunUntilComplete();
  EXPECT_EQ(net::ERR_ABORTED, client_.completion_status().error_code);

  if (LoaderRecordsTimingMetrics()) {
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
}

// Test when the service worker responds with network fallback.
// i.e., does not call respondWith().
TEST_F(ServiceWorkerMainResourceLoaderTest, FallbackResponse) {
  base::HistogramTester histogram_tester;
  service_worker_->RespondWithFallback();

  // Perform the request.
  StartRequest(CreateRequest());

  // The fallback callback should be called.
  RunUntilFallbackCallback();

  // The request should not be handled by the loader, but it shouldn't be a
  // failure.
  EXPECT_TRUE(service_worker_client()->controller());
  histogram_tester.ExpectUniqueSample(kHistogramMainResourceFetchEvent,
                                      blink::ServiceWorkerStatusCode::kOk, 1);
  if (LoaderRecordsTimingMetrics()) {
    // Test histogram of network fallback.
    histogram_tester.ExpectTotalCount(
        "ServiceWorker.LoadTiming.MainFrame.MainResource."
        "FetchHandlerEndToFallbackNetwork",
        1);
  }
}

// Test when the service worker rejects the FetchEvent.
TEST_F(ServiceWorkerMainResourceLoaderTest, ErrorResponse) {
  base::HistogramTester histogram_tester;
  service_worker_->RespondWithError();

  // Perform the request.
  StartRequest(CreateRequest());
  client_.RunUntilComplete();
  EXPECT_EQ(net::ERR_FAILED, client_.completion_status().error_code);

  // Event dispatch still succeeded.
  histogram_tester.ExpectUniqueSample(kHistogramMainResourceFetchEvent,
                                      blink::ServiceWorkerStatusCode::kOk, 1);
  if (LoaderRecordsTimingMetrics()) {
    // Timing UMAs shouldn't be recorded when we receive an error response.
    histogram_tester.ExpectTotalCount(
        "ServiceWorker.LoadTiming.MainFrame.MainResource."
        "StartToForwardServiceWorker",
        0);
  }
}

// Test when dispatching the fetch event to the service worker failed.
TEST_F(ServiceWorkerMainResourceLoaderTest, FailFetchDispatch) {
  // This test simulates failure to dispatch the fetch event to the
  // service worker by calling
  // `service_worker_->FailToDispatchFetchEvent()`. But without
  // disabling high priority fetch response callback, request processing
  // comes earlier, and doesn't fail to fetch dispatch.  This test is
  // still valid after introducing HighPriorityFetchResponseCallback.
  ScopedOverrideToDisableHighPriorityFetchResponseCallback
      disable_high_priority_fetch_response_callback;

  base::HistogramTester histogram_tester;
  service_worker_->FailToDispatchFetchEvent();

  // Perform the request.
  StartRequest(CreateRequest());

  // The fallback callback should be called.
  RunUntilFallbackCallback();
  EXPECT_FALSE(service_worker_client()->controller());

  histogram_tester.ExpectUniqueSample(
      kHistogramMainResourceFetchEvent,
      blink::ServiceWorkerStatusCode::kErrorFailed, 1);
  if (LoaderRecordsTimingMetrics()) {
    // Timing UMAs shouldn't be recorded when failed to dispatch an event.
    histogram_tester.ExpectTotalCount(
        "ServiceWorker.LoadTiming.MainFrame.MainResource."
        "StartToForwardServiceWorker",
        0);
  }
}

// Test when the respondWith() promise resolves before the waitUntil() promise
// resolves. The response should be received before the event finishes.
TEST_F(ServiceWorkerMainResourceLoaderTest, EarlyResponse) {
  service_worker_->RespondEarly();

  // Perform the request.
  StartRequest(CreateRequest());
  client_.RunUntilComplete();

  auto& info = client_.response_head();
  EXPECT_EQ(200, info->headers->response_code());
  ExpectResponseInfo(*info, *CreateResponseInfoFromServiceWorker());

  // Although the response was already received, the event remains outstanding
  // until waitUntil() resolves.
  EXPECT_TRUE(HasWorkInBrowser(version_.get()));
  service_worker_->FinishWaitUntil();
  EXPECT_FALSE(HasWorkInBrowser(version_.get()));
}

// Test responding to the fetch event with a redirect response.
TEST_F(ServiceWorkerMainResourceLoaderTest, Redirect) {
  base::HistogramTester histogram_tester;
  GURL new_url("https://example.com/redirected");
  service_worker_->RespondWithRedirectResponse(new_url);

  // Perform the request.
  StartRequest(CreateRequest());
  client_.RunUntilRedirectReceived();

  auto& info = client_.response_head();
  EXPECT_EQ(301, info->headers->response_code());
  ExpectResponseInfo(*info, *CreateResponseInfoFromServiceWorker());

  const net::RedirectInfo& redirect_info = client_.redirect_info();
  EXPECT_EQ(301, redirect_info.status_code);
  EXPECT_EQ("GET", redirect_info.new_method);
  EXPECT_EQ(new_url, redirect_info.new_url);

  histogram_tester.ExpectUniqueSample(kHistogramMainResourceFetchEvent,
                                      blink::ServiceWorkerStatusCode::kOk, 1);
}

TEST_F(ServiceWorkerMainResourceLoaderTest, Lifetime) {
  StartRequest(CreateRequest());
  base::WeakPtr<ServiceWorkerMainResourceLoader> loader = loader_->AsWeakPtr();
  ASSERT_TRUE(loader);

  client_.RunUntilComplete();
  EXPECT_TRUE(loader);

  // Even after calling DetachedFromRequest(), |loader_| should be alive until
  // the Mojo connection to the loader is disconnected.
  loader_.release()->DetachedFromRequest();
  EXPECT_TRUE(loader);

  // When the remote for |loader_| is disconnected, its weak pointers (|loader|)
  // are invalidated.
  loader_remote_.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(loader);

  // |loader_| is deleted here. LSan test will alert if it leaks.
}

TEST_F(ServiceWorkerMainResourceLoaderTest, ConnectionErrorDuringFetchEvent) {
  service_worker_->DeferResponse();
  StartRequest(CreateRequest());

  // Wait for the fetch event to be dispatched.
  service_worker_->RunUntilFetchEvent();

  // Break the Mojo connection. The loader should return an aborted status.
  loader_remote_.reset();
  client_.RunUntilComplete();
  EXPECT_EQ(net::ERR_ABORTED, client_.completion_status().error_code);

  // The loader is still alive. Finish the fetch event. It shouldn't crash or
  // call any callbacks on |client_|, which would throw an error.
  service_worker_->FinishRespondWith();

  // There's no event to wait for, so just pump the message loop and the test
  // passes if there is no error or crash.
  base::RunLoop().RunUntilIdle();
}

TEST_F(ServiceWorkerMainResourceLoaderTest, CancelNavigationDuringFetchEvent) {
  // This test simulates failure by resetting ServiceWorkerClient.  But without
  // disabling HighPriorityFetchResponseCallback,
  // `Release()` comes later than
  // request processing, and doesn't cancel navigation during the fetch
  // event.  This test is still valid after introducing
  // HighPriorityFetchResponseCallback.
  ScopedOverrideToDisableHighPriorityFetchResponseCallback
      disable_high_priority_fetch_response_callback;

  StartRequest(CreateRequest());

  // Delete the container host during the request. The load should abort without
  // crashing.
  service_worker_client_.reset();
  base::RunLoop().RunUntilIdle();

  client_.RunUntilComplete();
  EXPECT_EQ(net::ERR_ABORTED, client_.completion_status().error_code);
}

TEST_F(ServiceWorkerMainResourceLoaderTest, TimingInfo) {
  service_worker_->DispatchAfter1sDelay();

  // Perform the request.
  StartRequest(CreateRequest());
  client_.RunUntilComplete();

  // The response header's timing is recorded appropriately.
  auto& info = client_.response_head();
  EXPECT_EQ(200, info->headers->response_code());
  ExpectResponseInfo(*info, *CreateResponseInfoFromServiceWorker());
  EXPECT_EQ(base::Seconds(1), info->load_timing.service_worker_ready_time -
                                  info->load_timing.service_worker_start_time);
  EXPECT_EQ(base::Seconds(1), info->load_timing.service_worker_fetch_start -
                                  info->load_timing.service_worker_start_time);
  EXPECT_EQ(base::Seconds(1),
            info->load_timing.service_worker_respond_with_settled -
                info->load_timing.service_worker_start_time);
}

TEST_F(ServiceWorkerMainResourceLoaderTest, FencedFrameNavigationPreload) {
  registration_->EnableNavigationPreload(true);

  std::unique_ptr<network::ResourceRequest> request = CreateRequest();
  request->destination = network::mojom::RequestDestination::kFencedframe;

  // Perform the request.
  StartRequest(std::move(request));
  client_.RunUntilComplete();

  EXPECT_EQ(net::OK, client_.completion_status().error_code);
  const auto& info = client_.response_head();
  EXPECT_EQ(200, info->headers->response_code());
  EXPECT_FALSE(info->load_timing.receive_headers_start.is_null());
  EXPECT_FALSE(info->load_timing.receive_headers_end.is_null());
  EXPECT_LE(info->load_timing.receive_headers_start,
            info->load_timing.receive_headers_end);
  auto expected_info = CreateResponseInfoFromServiceWorker();
  expected_info->did_service_worker_navigation_preload = true;
  ExpectResponseInfo(*info, *expected_info);
}

}  // namespace service_worker_main_resource_loader_unittest
}  // namespace content
