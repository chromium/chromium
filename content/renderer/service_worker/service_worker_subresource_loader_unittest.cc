// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/service_worker_subresource_loader.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "content/common/features.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "content/renderer/service_worker/controller_service_worker_connector.h"
#include "content/test/fake_network_url_loader_factory.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "net/http/http_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "services/network/public/cpp/wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_data_pipe_getter.h"
#include "services/network/test/test_url_loader_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/service_worker/service_worker_router_rule.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_response.mojom.h"
#include "third_party/blink/public/mojom/loader/fetch_client_settings_object.mojom.h"
#include "third_party/blink/public/mojom/service_worker/dispatch_fetch_event_params.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_container.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_fetch_handler_bypass_option.mojom-shared.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration_options.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_stream_handle.mojom.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "url/origin.h"

namespace content {
namespace service_worker_subresource_loader_unittest {

// A simple blob implementation for serving data stored in a vector.
class FakeBlob final : public blink::mojom::Blob {
 public:
  FakeBlob(std::optional<std::vector<uint8_t>> side_data, std::string body)
      : side_data_(std::move(side_data)), body_(std::move(body)) {}

 private:
  // Implements blink::mojom::Blob.
  void Clone(mojo::PendingReceiver<blink::mojom::Blob> receiver) override {
    receivers_.Add(this, std::move(receiver));
  }
  void AsDataPipeGetter(
      mojo::PendingReceiver<network::mojom::DataPipeGetter>) override {
    NOTREACHED_IN_MIGRATION();
  }
  void ReadRange(
      uint64_t offset,
      uint64_t length,
      mojo::ScopedDataPipeProducerHandle handle,
      mojo::PendingRemote<blink::mojom::BlobReaderClient> client) override {
    NOTREACHED_IN_MIGRATION();
  }
  void ReadAll(
      mojo::ScopedDataPipeProducerHandle handle,
      mojo::PendingRemote<blink::mojom::BlobReaderClient> client) override {
    mojo::Remote<blink::mojom::BlobReaderClient> client_remote(
        std::move(client));
    EXPECT_TRUE(mojo::BlockingCopyFromString(body_, handle));
    if (client_remote) {
      client_remote->OnCalculatedSize(body_.size(), body_.size());
      client_remote->OnComplete(net::OK, body_.size());
    }
  }
  void Load(mojo::PendingReceiver<network::mojom::URLLoader>,
            const std::string& method,
            const net::HttpRequestHeaders&,
            mojo::PendingRemote<network::mojom::URLLoaderClient>) override {
    NOTREACHED_IN_MIGRATION();
  }
  void ReadSideData(ReadSideDataCallback callback) override {
    std::move(callback).Run(side_data_);
  }
  void CaptureSnapshot(CaptureSnapshotCallback callback) override {
    std::move(callback).Run(body_.size(), std::nullopt);
  }
  void GetInternalUUID(GetInternalUUIDCallback callback) override {
    NOTREACHED_IN_MIGRATION();
  }

  mojo::ReceiverSet<blink::mojom::Blob> receivers_;
  std::optional<std::vector<uint8_t>> side_data_;
  std::string body_;
};

class FakeControllerServiceWorker
    : public blink::mojom::ControllerServiceWorker {
 public:
  FakeControllerServiceWorker() = default;

  FakeControllerServiceWorker(const FakeControllerServiceWorker&) = delete;
  FakeControllerServiceWorker& operator=(const FakeControllerServiceWorker&) =
      delete;

  ~FakeControllerServiceWorker() override = default;

  static blink::mojom::FetchAPIResponsePtr OkResponse(
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

      // Clone the blob into the side_data_blob to match cache_storage behavior.
      mojo::Remote<blink::mojom::Blob> blob_remote(
          std::move(response->blob->blob));
      blob_remote->Clone(response->blob->blob.InitWithNewPipeAndPassReceiver());
      response->side_data_blob = blink::mojom::SerializedBlob::New(
          response->blob->uuid, response->blob->content_type,
          response->blob->size, blob_remote.Unbind());
    }
    return response;
  }

  static blink::mojom::FetchAPIResponsePtr ErrorResponse() {
    auto response = blink::mojom::FetchAPIResponse::New();
    response->status_code = 0;
    response->response_type = network::mojom::FetchResponseType::kDefault;
    response->error =
        blink::mojom::ServiceWorkerResponseError::kPromiseRejected;
    return response;
  }

  static blink::mojom::FetchAPIResponsePtr RedirectResponse(
      const std::string& redirect_location_header) {
    auto response = blink::mojom::FetchAPIResponse::New();
    response->status_code = 302;
    response->status_text = "Found";
    response->response_type = network::mojom::FetchResponseType::kDefault;
    response->headers["Location"] = redirect_location_header;
    return response;
  }

  void ClearReceivers() { receivers_.Clear(); }

  // Tells this controller to abort the fetch event without a response.
  // i.e., simulate the service worker failing to handle the fetch event.
  void AbortEventWithNoResponse() { response_mode_ = ResponseMode::kAbort; }

  // Tells this controller to respond to fetch events with network fallback.
  // i.e., simulate the service worker not calling respondWith().
  void RespondWithFallback() {
    response_mode_ = ResponseMode::kFallbackResponse;
  }

  // Tells this controller to respond to fetch events with the specified stream.
  void RespondWithStream(
      mojo::PendingReceiver<blink::mojom::ServiceWorkerStreamCallback>
          callback_receiver,
      mojo::ScopedDataPipeConsumerHandle consumer_handle) {
    response_mode_ = ResponseMode::kStream;
    stream_handle_ = blink::mojom::ServiceWorkerStreamHandle::New();
    stream_handle_->callback_receiver = std::move(callback_receiver);
    stream_handle_->stream = std::move(consumer_handle);
  }

  // Tells this controller to respond to fetch events with a error response.
  void RespondWithError() { response_mode_ = ResponseMode::kErrorResponse; }

  // Tells this controller to respond to fetch events with a redirect response.
  void RespondWithRedirect(const std::string& redirect_location_header) {
    response_mode_ = ResponseMode::kRedirectResponse;
    redirect_location_header_ = redirect_location_header;
  }

  // Tells this controller to respond to fetch events with a blob response body.
  void RespondWithBlob(std::optional<std::vector<uint8_t>> metadata,
                       std::string body) {
    response_mode_ = ResponseMode::kBlob;
    blob_body_ = blink::mojom::SerializedBlob::New();
    blob_body_->uuid = "dummy-blob-uuid";
    blob_body_->size = body.size();
    mojo::MakeSelfOwnedReceiver(
        std::make_unique<FakeBlob>(std::move(metadata), std::move(body)),
        blob_body_->blob.InitWithNewPipeAndPassReceiver());
  }

  // Tells this controller to respond to fetch events with a 206 partial
  // response, returning a blob composed of the requested bytes of |body|
  // according to the request headers.
  void RespondWithBlobRange(std::string body) {
    response_mode_ = ResponseMode::kBlobRange;
    blob_range_body_ = body;
  }

  // Tells this controller to not respond to fetch events.
  void DontRespond() { response_mode_ = ResponseMode::kDontRespond; }

  void ReadRequestBody(std::string* out_string) {
    ASSERT_TRUE(request_body_);
    std::vector<network::DataElement>* elements =
        request_body_->elements_mutable();
    // So far this test expects a single element (bytes or data pipe).
    ASSERT_EQ(1u, elements->size());
    network::DataElement& element = elements->front();
    if (element.type() == network::DataElement::Tag::kBytes) {
      *out_string =
          std::string(element.As<network::DataElementBytes>().AsStringPiece());
    } else if (element.type() == network::DataElement::Tag::kDataPipe) {
      // Read the content into |producer_handle|.
      mojo::ScopedDataPipeProducerHandle producer_handle;
      mojo::ScopedDataPipeConsumerHandle consumer_handle;
      ASSERT_EQ(mojo::CreateDataPipe(nullptr, producer_handle, consumer_handle),
                MOJO_RESULT_OK);

      mojo::Remote<network::mojom::DataPipeGetter> remote(
          element.As<network::DataElementDataPipe>().ReleaseDataPipeGetter());
      base::RunLoop run_loop;
      remote->Read(
          std::move(producer_handle),
          base::BindOnce([](base::OnceClosure quit_closure, int32_t status,
                            uint64_t size) { std::move(quit_closure).Run(); },
                         run_loop.QuitClosure()));
      run_loop.Run();
      // Copy the content to |out_string|.
      mojo::BlockingCopyToString(std::move(consumer_handle), out_string);
    } else {
      NOTREACHED_IN_MIGRATION();
    }
  }

  void SetResponseSource(network::mojom::FetchResponseSource source) {
    response_source_ = source;
  }

  void SetCacheStorageCacheName(std::string cache_name) {
    cache_storage_cache_name_ = cache_name;
  }

  void SetResponseTime(base::Time time) { response_time_ = time; }

  // blink::mojom::ControllerServiceWorker:
  void DispatchFetchEventForSubresource(
      blink::mojom::DispatchFetchEventParamsPtr params,
      mojo::PendingRemote<blink::mojom::ServiceWorkerFetchResponseCallback>
          pending_response_callback,
      DispatchFetchEventForSubresourceCallback callback) override {
    mojo::Remote<blink::mojom::ServiceWorkerFetchResponseCallback>
        response_callback(std::move(pending_response_callback));
    EXPECT_FALSE(params->request->is_main_resource_load);
    if (params->request->body)
      request_body_ = params->request->body;

    fetch_event_count_++;
    fetch_event_request_ = std::move(params->request);

    auto timing = blink::mojom::ServiceWorkerFetchEventTiming::New();
    timing->dispatch_event_time = base::TimeTicks::Now();
    timing->respond_with_settled_time = base::TimeTicks::Now();

    switch (response_mode_) {
      case ResponseMode::kDefault:
        response_callback->OnResponse(
            OkResponse(nullptr /* blob_body */, response_source_,
                       response_time_, cache_storage_cache_name_),
            std::move(timing));
        std::move(callback).Run(
            blink::mojom::ServiceWorkerEventStatus::COMPLETED);
        break;
      case ResponseMode::kAbort:
        std::move(callback).Run(
            blink::mojom::ServiceWorkerEventStatus::ABORTED);
        break;
      case ResponseMode::kStream:
        response_callback->OnResponseStream(
            OkResponse(nullptr /* blob_body */, response_source_,
                       response_time_, cache_storage_cache_name_),
            std::move(stream_handle_), std::move(timing));
        std::move(callback).Run(
            blink::mojom::ServiceWorkerEventStatus::COMPLETED);
        break;
      case ResponseMode::kBlob:
        response_callback->OnResponse(
            OkResponse(std::move(blob_body_), response_source_, response_time_,
                       cache_storage_cache_name_),
            std::move(timing));
        std::move(callback).Run(
            blink::mojom::ServiceWorkerEventStatus::COMPLETED);
        break;

      case ResponseMode::kBlobRange: {
        // Parse the Range header.
        std::string range_header;
        std::vector<net::HttpByteRange> ranges;
        ASSERT_TRUE(fetch_event_request_->headers.contains(
            std::string(net::HttpRequestHeaders::kRange)));
        ASSERT_TRUE(net::HttpUtil::ParseRangeHeader(
            fetch_event_request_->headers[net::HttpRequestHeaders::kRange],
            &ranges));
        ASSERT_EQ(1u, ranges.size());
        ASSERT_TRUE(ranges[0].ComputeBounds(blob_range_body_.size()));
        const net::HttpByteRange& range = ranges[0];

        // Build a Blob composed of the requested bytes from |blob_range_body_|.
        size_t start = static_cast<size_t>(range.first_byte_position());
        size_t end = static_cast<size_t>(range.last_byte_position());
        size_t size = end - start + 1;
        std::string body = blob_range_body_.substr(start, size);
        auto blob = blink::mojom::SerializedBlob::New();
        blob->uuid = "dummy-blob-uuid";
        blob->size = size;
        mojo::MakeSelfOwnedReceiver(
            std::make_unique<FakeBlob>(std::nullopt, body),
            blob->blob.InitWithNewPipeAndPassReceiver());

        // Respond with a 206 response.
        auto response = OkResponse(std::move(blob), response_source_,
                                   response_time_, cache_storage_cache_name_);
        response->status_code = 206;
        response->headers.emplace(
            "Content-Range", base::StringPrintf("bytes %zu-%zu/%zu", start, end,
                                                blob_range_body_.size()));
        response_callback->OnResponse(std::move(response), std::move(timing));
        std::move(callback).Run(
            blink::mojom::ServiceWorkerEventStatus::COMPLETED);
        break;
      }

      case ResponseMode::kFallbackResponse:
        response_callback->OnFallback(/*request_body=*/std::nullopt,
                                      std::move(timing));
        std::move(callback).Run(
            blink::mojom::ServiceWorkerEventStatus::COMPLETED);
        break;
      case ResponseMode::kErrorResponse:
        response_callback->OnResponse(ErrorResponse(), std::move(timing));
        std::move(callback).Run(
            blink::mojom::ServiceWorkerEventStatus::REJECTED);
        break;
      case ResponseMode::kRedirectResponse:
        response_callback->OnResponse(
            RedirectResponse(redirect_location_header_), std::move(timing));
        std::move(callback).Run(
            blink::mojom::ServiceWorkerEventStatus::COMPLETED);
        break;
      case ResponseMode::kDontRespond:
        response_callback_ = std::move(response_callback);
        callback_ = std::move(callback);
        break;
    }
    if (fetch_event_callback_)
      std::move(fetch_event_callback_).Run();
  }

  void Clone(
      mojo::PendingReceiver<blink::mojom::ControllerServiceWorker> receiver,
      const network::CrossOriginEmbedderPolicy&,
      mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>)
      override {
    receivers_.Add(this, std::move(receiver));
  }

  void RunUntilFetchEvent() {
    base::RunLoop loop;
    fetch_event_callback_ = loop.QuitClosure();
    loop.Run();
  }

  int fetch_event_count() const { return fetch_event_count_; }
  const blink::mojom::FetchAPIRequest& fetch_event_request() const {
    return *fetch_event_request_;
  }

 private:
  enum class ResponseMode {
    kDefault,
    kAbort,
    kStream,
    kBlob,
    kBlobRange,
    kFallbackResponse,
    kErrorResponse,
    kRedirectResponse,
    kDontRespond
  };

  ResponseMode response_mode_ = ResponseMode::kDefault;
  scoped_refptr<network::ResourceRequestBody> request_body_;

  int fetch_event_count_ = 0;
  blink::mojom::FetchAPIRequestPtr fetch_event_request_;
  base::OnceClosure fetch_event_callback_;
  mojo::ReceiverSet<blink::mojom::ControllerServiceWorker> receivers_;

  // For ResponseMode::kStream.
  blink::mojom::ServiceWorkerStreamHandlePtr stream_handle_;

  // For ResponseMode::kBlob.
  blink::mojom::SerializedBlobPtr blob_body_;

  // For ResponseMode::kBlobRange.
  std::string blob_range_body_;

  // For ResponseMode::kRedirectResponse.
  std::string redirect_location_header_;

  // For ResponseMode::kDontRespond.
  DispatchFetchEventForSubresourceCallback callback_;
  mojo::Remote<blink::mojom::ServiceWorkerFetchResponseCallback>
      response_callback_;

  network::mojom::FetchResponseSource response_source_ =
      network::mojom::FetchResponseSource::kUnspecified;

  std::string cache_storage_cache_name_;
  base::Time response_time_;
};

class FakeServiceWorkerContainerHost
    : public blink::mojom::ServiceWorkerContainerHost {
 public:
  explicit FakeServiceWorkerContainerHost(
      FakeControllerServiceWorker* fake_controller)
      : fake_controller_(fake_controller) {}

  FakeServiceWorkerContainerHost(const FakeServiceWorkerContainerHost&) =
      delete;
  FakeServiceWorkerContainerHost& operator=(
      const FakeServiceWorkerContainerHost&) = delete;

  ~FakeServiceWorkerContainerHost() override = default;

  void set_fake_controller(FakeControllerServiceWorker* new_fake_controller) {
    fake_controller_ = new_fake_controller;
  }

  int get_controller_service_worker_count() const {
    return get_controller_service_worker_count_;
  }

  // Implements blink::mojom::ServiceWorkerContainerHost.
  void Register(const GURL& script_url,
                blink::mojom::ServiceWorkerRegistrationOptionsPtr options,
                blink::mojom::FetchClientSettingsObjectPtr
                    outside_fetch_client_settings_object,
                RegisterCallback callback) override {
    NOTIMPLEMENTED();
  }
  void GetRegistration(const GURL& client_url,
                       GetRegistrationCallback callback) override {
    NOTIMPLEMENTED();
  }
  void GetRegistrations(GetRegistrationsCallback callback) override {
    NOTIMPLEMENTED();
  }
  void GetRegistrationForReady(
      GetRegistrationForReadyCallback callback) override {
    NOTIMPLEMENTED();
  }
  void EnsureControllerServiceWorker(
      mojo::PendingReceiver<blink::mojom::ControllerServiceWorker> receiver,
      blink::mojom::ControllerServiceWorkerPurpose purpose) override {
    get_controller_service_worker_count_++;
    if (!fake_controller_)
      return;
    fake_controller_->Clone(std::move(receiver),
                            network::CrossOriginEmbedderPolicy(),
                            mojo::NullRemote());
  }
  void CloneContainerHost(
      mojo::PendingReceiver<blink::mojom::ServiceWorkerContainerHost> receiver)
      override {
    receivers_.Add(this, std::move(receiver));
  }
  void HintToUpdateServiceWorker() override { NOTIMPLEMENTED(); }
  void EnsureFileAccess(const std::vector<base::FilePath>& files,
                        EnsureFileAccessCallback callback) override {
    std::move(callback).Run();
  }
  void OnExecutionReady() override {}

 private:
  int get_controller_service_worker_count_ = 0;
  raw_ptr<FakeControllerServiceWorker> fake_controller_;
  mojo::ReceiverSet<blink::mojom::ServiceWorkerContainerHost> receivers_;
};

// Returns an expected network::mojom::URLResponseHeadPtr which is used by
// stream response related tests.
network::mojom::URLResponseHeadPtr CreateResponseInfoFromServiceWorker() {
  auto head = network::mojom::URLResponseHead::New();
  std::string headers = "HTTP/1.1 200 OK\n\n";
  head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(headers));
  head->was_fetched_via_service_worker = true;
  head->url_list_via_service_worker = std::vector<GURL>();
  head->response_type = network::mojom::FetchResponseType::kDefault;
  head->cache_storage_cache_name = std::string();
  head->did_service_worker_navigation_preload = false;
  return head;
}

// ServiceWorkerSubresourceLoader::RecordTimingMetrics() records the metrics
// only when the consistent high-resolution timer is used among processes.
bool LoaderRecordsTimingMetrics() {
  return base::TimeTicks::IsHighResolution() &&
         base::TimeTicks::IsConsistentAcrossProcesses();
}

const char kHistogramSubresourceFetchEvent[] =
    "ServiceWorker.FetchEvent.Subresource.Status";

class ServiceWorkerSubresourceLoaderTest : public ::testing::Test {
 public:
  ServiceWorkerSubresourceLoaderTest(
      const ServiceWorkerSubresourceLoaderTest&) = delete;
  ServiceWorkerSubresourceLoaderTest& operator=(
      const ServiceWorkerSubresourceLoaderTest&) = delete;

 protected:
  ServiceWorkerSubresourceLoaderTest()
      : fake_container_host_(&fake_controller_) {}
  ~ServiceWorkerSubresourceLoaderTest() override = default;

  void SetUp() override {
    mojo::PendingRemote<network::mojom::URLLoaderFactory> fake_loader_factory;
    mojo::MakeSelfOwnedReceiver(
        std::make_unique<FakeNetworkURLLoaderFactory>(),
        fake_loader_factory.InitWithNewPipeAndPassReceiver());
    loader_factory_ =
        base::MakeRefCounted<network::WrapperSharedURLLoaderFactory>(
            std::move(fake_loader_factory));
  }

  mojo::Remote<network::mojom::URLLoaderFactory>
  CreateSubresourceLoaderFactory() {
    if (!connector_) {
      mojo::PendingRemote<blink::mojom::ServiceWorkerContainerHost>
          remote_container_host;
      fake_container_host_.CloneContainerHost(
          remote_container_host.InitWithNewPipeAndPassReceiver());
      connector_ = base::MakeRefCounted<ControllerServiceWorkerConnector>(
          std::move(remote_container_host),
          mojo::NullRemote() /*remote_controller*/,
          mojo::NullRemote() /*remote_cache_storage*/, "" /*client_id*/,
          blink::mojom::ServiceWorkerFetchHandlerBypassOption::kDefault,
          std::nullopt, blink::EmbeddedWorkerStatus::kStopped,
          mojo::NullReceiver() /*running_status_receiver*/);
    }
    mojo::Remote<network::mojom::URLLoaderFactory>
        service_worker_url_loader_factory;
    ServiceWorkerSubresourceLoaderFactory::Create(
        connector_, loader_factory_,
        service_worker_url_loader_factory.BindNewPipeAndPassReceiver(),
        blink::scheduler::GetSequencedTaskRunnerForTesting());
    return service_worker_url_loader_factory;
  }

  // Starts |request| using |loader_factory| and sets |out_loader| and
  // |out_loader_client| to the resulting URLLoader and its URLLoaderClient. The
  // caller can then use functions like client.RunUntilComplete() to wait for
  // completion. Calling fake_controller_->RunUntilFetchEvent() also advances
  // the load to until |fake_controller_| receives the fetch event.
  void StartRequest(
      const mojo::Remote<network::mojom::URLLoaderFactory>& loader_factory,
      const network::ResourceRequest& request,
      mojo::Remote<network::mojom::URLLoader>* out_loader,
      std::unique_ptr<network::TestURLLoaderClient>* out_loader_client) {
    *out_loader_client = std::make_unique<network::TestURLLoaderClient>();
    loader_factory->CreateLoaderAndStart(
        out_loader->BindNewPipeAndPassReceiver(), 0,
        network::mojom::kURLLoadOptionNone, request,
        (*out_loader_client)->CreateRemote(),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  }

  void ExpectResponseInfo(
      const network::mojom::URLResponseHead& info,
      const network::mojom::URLResponseHead& expected_info) {
    EXPECT_EQ(expected_info.headers->response_code(),
              info.headers->response_code());
    EXPECT_EQ(expected_info.was_fetched_via_service_worker,
              info.was_fetched_via_service_worker);
    EXPECT_EQ(expected_info.url_list_via_service_worker,
              info.url_list_via_service_worker);
    EXPECT_EQ(expected_info.response_type, info.response_type);
    EXPECT_EQ(expected_info.cache_storage_cache_name,
              info.cache_storage_cache_name);
    EXPECT_EQ(expected_info.response_time, info.response_time);
    EXPECT_EQ(expected_info.service_worker_response_source,
              info.service_worker_response_source);
    EXPECT_EQ(expected_info.did_service_worker_navigation_preload,
              info.did_service_worker_navigation_preload);
    EXPECT_NE(expected_info.load_timing.service_worker_start_time,
              info.load_timing.service_worker_start_time);
    EXPECT_NE(expected_info.load_timing.service_worker_ready_time,
              info.load_timing.service_worker_ready_time);
    EXPECT_NE(expected_info.load_timing.service_worker_fetch_start,
              info.load_timing.service_worker_fetch_start);
    EXPECT_NE(expected_info.load_timing.service_worker_respond_with_settled,
              info.load_timing.service_worker_respond_with_settled);
  }

  network::ResourceRequest CreateRequest(const GURL& url) {
    network::ResourceRequest request;
    request.url = url;
    request.method = "GET";
    request.destination = network::mojom::RequestDestination::kEmpty;
    return request;
  }

  // Performs a request with the given |request_body|, and checks that the body
  // is passed to the fetch event.
  void RunFallbackWithRequestBodyTest(
      scoped_refptr<network::ResourceRequestBody> request_body,
      const std::string& expected_body) {
    mojo::Remote<network::mojom::URLLoaderFactory> factory =
        CreateSubresourceLoaderFactory();

    // Create a request with the body.
    network::ResourceRequest request =
        CreateRequest(GURL("https://www.example.com/upload"));
    request.method = "POST";
    request.request_body = std::move(request_body);

    // Set the service worker to do network fallback as it exercises a tricky
    // code path to ensure the body makes it to network.
    fake_controller_.RespondWithFallback();

    // Perform the request.
    mojo::Remote<network::mojom::URLLoader> loader;
    std::unique_ptr<network::TestURLLoaderClient> client;
    StartRequest(factory, request, &loader, &client);
    client->RunUntilComplete();

    // Verify that the request body was passed to the fetch event.
    std::string fetch_event_body;
    fake_controller_.ReadRequestBody(&fetch_event_body);
    EXPECT_EQ(expected_body, fetch_event_body);

    // TODO(falken): It'd be nicer to also check the request body was sent to
    // network but it requires more complicated network mocking and it was hard
    // getting EmbeddedTestServer working with these tests (probably
    // CorsFallbackResponse is too heavy). We also have Web Platform Tests that
    // cover this case in fetch-event.https.html.
  }

  // Performs a range request using |range_header| and returns the resulting
  // client after completion.
  std::unique_ptr<network::TestURLLoaderClient> DoRangeRequest(
      const std::string& range_header) {
    mojo::Remote<network::mojom::URLLoaderFactory> factory =
        CreateSubresourceLoaderFactory();
    network::ResourceRequest request =
        CreateRequest(GURL("https://www.example.com/big-file"));
    request.destination = network::mojom::RequestDestination::kVideo;
    request.headers.SetHeader("Range", range_header);
    mojo::Remote<network::mojom::URLLoader> loader;
    std::unique_ptr<network::TestURLLoaderClient> client;
    StartRequest(factory, request, &loader, &client);
    client->RunUntilComplete();
    return client;
  }

  std::string TakeResponseBody(network::TestURLLoaderClient* client) {
    std::string body;
    EXPECT_TRUE(client->response_body().is_valid());
    EXPECT_TRUE(
        mojo::BlockingCopyToString(client->response_body_release(), &body));
    return body;
  }

  BrowserTaskEnvironment task_environment_;
  scoped_refptr<network::SharedURLLoaderFactory> loader_factory_;
  scoped_refptr<ControllerServiceWorkerConnector> connector_;
  FakeServiceWorkerContainerHost fake_container_host_;
  FakeControllerServiceWorker fake_controller_;
};

TEST_F(ServiceWorkerSubresourceLoaderTest, Basic) {
  base::HistogramTester histogram_tester;

  mojo::Remote<network::mojom::URLLoaderFactory> factory =
      CreateSubresourceLoaderFactory();
  network::ResourceRequest request =
      CreateRequest(GURL("https://www.example.com/foo.png"));
  mojo::Remote<network::mojom::URLLoader> loader;
  std::unique_ptr<network::TestURLLoaderClient> client;
  StartRequest(factory, request, &loader, &client);
  fake_controller_.RunUntilFetchEvent();

  EXPECT_EQ(request.url, fake_controller_.fetch_event_request().url);
  EXPECT_EQ(request.method, fake_controller_.fetch_event_request().method);
  EXPECT_EQ(1, fake_controller_.fetch_event_count());
  EXPECT_EQ(1, fake_container_host_.get_controller_service_worker_count());

  client->RunUntilComplete();

  net::LoadTimingInfo load_timing_info = client->response_head()->load_timing;
  EXPECT_FALSE(load_timing_info.receive_headers_start.is_null());
  EXPECT_FALSE(load_timing_info.receive_headers_end.is_null());
  EXPECT_LE(load_timing_info.receive_headers_start,
            load_timing_info.receive_headers_end);

  histogram_tester.ExpectUniqueSample(kHistogramSubresourceFetchEvent,
                                      blink::ServiceWorkerStatusCode::kOk, 1);
  if (LoaderRecordsTimingMetrics()) {
    histogram_tester.ExpectTotalCount(
        "ServiceWorker.LoadTiming.Subresource."
        "ForwardServiceWorkerToWorkerReady",
        1);
    histogram_tester.ExpectTotalCount(
        "ServiceWorker.LoadTiming.Subresource.ResponseReceivedToCompleted2", 1);
    histogram_tester.ExpectTotalCount(
        "ServiceWorker.LoadTiming.Subresource.ResponseReceivedToCompleted2."
        "Unspecified",
        1);
  }
}

TEST_F(ServiceWorkerSubresourceLoaderTest, Abort) {
  fake_controller_.AbortEventWithNoResponse();
  base::HistogramTester histogram_tester;

  mojo::Remote<network::mojom::URLLoaderFactory> factory =
      CreateSubresourceLoaderFactory();

  // Perform the request.
  network::ResourceRequest request =
      CreateRequest(GURL("https://www.example.com/foo.png"));
  mojo::Remote<network::mojom::URLLoader> loader;
  std::unique_ptr<network::TestURLLoaderClient> client;
  StartRequest(factory, request, &loader, &client);
  client->RunUntilComplete();

  EXPECT_EQ(net::ERR_FAILED, client->completion_status().error_code);
  histogram_tester.ExpectUniqueSample(
      kHistogramSubresourceFetchEvent,
      blink::ServiceWorkerStatusCode::kErrorAbort, 1);

  if (LoaderRecordsTimingMetrics()) {
    // Timing histograms shouldn't be recorded on abort.
    histogram_tester.ExpectTotalCount(
        "ServiceWorker.LoadTiming.Subresource."
        "ForwardServiceWorkerToWorkerReady",
        0);
    histogram_tester.ExpectTotalCount(
        "ServiceWorker.LoadTiming.Subresource.ResponseReceivedToCompleted2", 0);
  }
}

TEST_F(ServiceWorkerSubresourceLoaderTest, DropController) {
  mojo::Remote<network::mojom::URLLoaderFactory> factory =
      CreateSubresourceLoaderFactory();
  {
    network::ResourceRequest request =
        CreateRequest(GURL("https://www.example.com/foo.png"));
    mojo::Remote<network::mojom::URLLoader> loader;
    std::unique_ptr<network::TestURLLoaderClient> client;
    StartRequest(factory, request, &loader, &client);
    fake_controller_.RunUntilFetchEvent();

    EXPECT_EQ(request.url, fake_controller_.fetch_event_request().url);
    EXPECT_EQ(request.method, fake_controller_.fetch_event_request().method);
    EXPECT_EQ(1, fake_controller_.fetch_event_count());
    EXPECT_EQ(1, fake_container_host_.get_controller_service_worker_count());
  }

  // Loading another resource reuses the existing connection to the
  // ControllerServiceWorker (i.e. it doesn't increase the get controller
  // service worker count).
  {
    network::ResourceRequest request =
        CreateRequest(GURL("https://www.example.com/foo2.png"));
    mojo::Remote<network::mojom::URLLoader> loader;
    std::unique_ptr<network::TestURLLoaderClient> client;
    StartRequest(factory, request, &loader, &client);
    fake_controller_.RunUntilFetchEvent();

    EXPECT_EQ(request.url, fake_controller_.fetch_event_request().url);
    EXPECT_EQ(request.method, fake_controller_.fetch_event_request().method);
    EXPECT_EQ(2, fake_controller_.fetch_event_count());
    EXPECT_EQ(1, fake_container_host_.get_controller_service_worker_count());
  }

  // Drop the connection to the ControllerServiceWorker.
  fake_controller_.ClearReceivers();
  base::RunLoop().RunUntilIdle();

  {
    // This should re-obtain the ControllerServiceWorker.
    network::ResourceRequest request =
        CreateRequest(GURL("https://www.example.com/foo3.png"));
    mojo::Remote<network::mojom::URLLoader> loader;
    std::unique_ptr<network::TestURLLoaderClient> client;
    StartRequest(factory, request, &loader, &client);
    fake_controller_.RunUntilFetchEvent();

    EXPECT_EQ(request.url, fake_controller_.fetch_event_request().url);
    EXPECT_EQ(request.method, fake_controller_.fetch_event_request().method);
    EXPECT_EQ(3, fake_controller_.fetch_event_count());
    EXPECT_EQ(2, fake_container_host_.get_controller_service_worker_count());
  }
}

TEST_F(ServiceWorkerSubresourceLoaderTest, NoController) {
  mojo::Remote<network::mojom::URLLoaderFactory> factory =
      CreateSubresourceLoaderFactory();
  {
    network::ResourceRequest request =
        CreateRequest(GURL("https://www.example.com/foo.png"));
    mojo::Remote<network::mojom::URLLoader> loader;
    std::unique_ptr<network::TestURLLoaderClient> client;
    StartRequest(factory, request, &loader, &client);
    fake_controller_.RunUntilFetchEvent();

    EXPECT_EQ(request.url, fake_controller_.fetch_event_request().url);
    EXPECT_EQ(request.method, fake_controller_.fetch_event_request().method);
    EXPECT_EQ(1, fake_controller_.fetch_event_count());
    EXPECT_EQ(1, fake_container_host_.get_controller_service_worker_count());
  }

  // Make the connector have no controller.
  connector_->UpdateController(mojo::NullRemote());
  base::RunLoop().RunUntilIdle();

  base::HistogramTester histogram_tester;
  {
    // This should fallback to the network.
    network::ResourceRequest request =
        CreateRequest(GURL("https://www.example.com/foo2.png"));
    mojo::Remote<network::mojom::URLLoader> loader;
    std::unique_ptr<network::TestURLLoaderClient> client;
    StartRequest(factory, request, &loader, &client);
    client->RunUntilComplete();

    EXPECT_TRUE(client->has_received_completion());
    EXPECT_FALSE(client->response_head()->was_fetched_via_service_worker);

    EXPECT_EQ(1, fake_controller_.fetch_event_count());
    EXPECT_EQ(1, fake_container_host_.get_controller_service_worker_count());
  }

  if (LoaderRecordsTimingMetrics()) {
    // No fetch event was dispatched, so no sample should be recorded.
    histogram_tester.ExpectTotalCount(kHistogramSubresourceFetchEvent, 0);
    histogram_tester.ExpectTotalCount(
        "ServiceWorker.LoadTiming.Subresource."
        "ForwardServiceWorkerToWorkerReady",
        0);
    histogram_tester.ExpectTotalCount(
        "ServiceWorker.LoadTiming.Subresource.ResponseReceivedToCompleted2", 0);
  }
}

TEST_F(ServiceWorkerSubresourceLoaderTest, DropController_RestartFetchEvent) {
  mojo::Remote<network::mojom::URLLoaderFactory> factory =
      CreateSubresourceLoaderFactory();

  {
    network::ResourceRequest request =
        CreateRequest(GURL("https://www.example.com/foo.png"));
    mojo::Remote<network::mojom::URLLoader> loader;
    std::unique_ptr<network::TestURLLoaderClient> client;
    StartRequest(factory, request, &loader, &client);
    fake_controller_.RunUntilFetchEvent();

    EXPECT_EQ(request.url, fake_controller_.fetch_event_request().url);
    EXPECT_EQ(request.method, fake_controller_.fetch_event_request().method);
    EXPECT_EQ(1, fake_controller_.fetch_event_count());
    EXPECT_EQ(1, fake_container_host_.get_controller_service_worker_count());
  }

  // Loading another resource reuses the existing connection to the
  // ControllerServiceWorker (i.e. it doesn't increase the get controller
  // service worker count).
  {
    network::ResourceRequest request =
        CreateRequest(GURL("https://www.example.com/foo2.png"));
    mojo::Remote<network::mojom::URLLoader> loader;
    std::unique_ptr<network::TestURLLoaderClient> client;
    StartRequest(factory, request, &loader, &client);
    fake_controller_.RunUntilFetchEvent();

    EXPECT_EQ(request.url, fake_controller_.fetch_event_request().url);
    EXPECT_EQ(request.method, fake_controller_.fetch_event_request().method);
    EXPECT_EQ(2, fake_controller_.fetch_event_count());
    EXPECT_EQ(1, fake_container_host_.get_controller_service_worker_count());
    client->RunUntilComplete();
  }

  base::HistogramTester histogram_tester;

  network::ResourceRequest request =
      CreateRequest(GURL("https://www.example.com/foo3.png"));
  mojo::Remote<network::mojom::URLLoader> loader;
  std::unique_ptr<network::TestURLLoaderClient> client;
  StartRequest(factory, request, &loader, &client);

  // Drop the connection to the ControllerServiceWorker.
  fake_controller_.ClearReceivers();
  base::RunLoop().RunUntilIdle();

  // If connection is closed during fetch event, it's restarted and successfully
  // finishes.
  EXPECT_EQ(request.url, fake_controller_.fetch_event_request().url);
  EXPECT_EQ(request.method, fake_controller_.fetch_event_request().method);
  EXPECT_EQ(3, fake_controller_.fetch_event_count());
  EXPECT_EQ(2, fake_container_host_.get_controller_service_worker_count());
  histogram_tester.ExpectUniqueSample(kHistogramSubresourceFetchEvent,
                                      blink::ServiceWorkerStatusCode::kOk, 1);
  if (LoaderRecordsTimingMetrics()) {
    histogram_tester.ExpectTotalCount(
        "ServiceWorker.LoadTiming.Subresource."
        "ForwardServiceWorkerToWorkerReady",
        1);
    histogram_tester.ExpectTotalCount(
        "ServiceWorker.LoadTiming.Subresource.ResponseReceivedToCompleted2", 1);
  }
}

TEST_F(ServiceWorkerSubresourceLoaderTest, DropController_TooManyRestart) {
  base::HistogramTester histogram_tester;
  // Simulate the container host fails to start a service worker.
  fake_container_host_.set_fake_controller(nullptr);

  mojo::Remote<network::mojom::URLLoaderFactory> factory =
      CreateSubresourceLoaderFactory();
  network::ResourceRequest request =
      CreateRequest(GURL("https://www.example.com/foo.png"));
  mojo::Remote<network::mojom::URLLoader> loader;
  std::unique_ptr<network::TestURLLoaderClient> client;
  StartRequest(factory, request, &loader, &client);

  // Try to dispatch fetch event to the bad worker.
  base::RunLoop().RunUntilIdle();

  // The request should be failed instead of infinite loop to restart the
  // inflight fetch event.
  EXPECT_EQ(2, fake_container_host_.get_controller_service_worker_count());
  EXPECT_TRUE(client->has_received_completion());
  EXPECT_EQ(net::ERR_FAILED, client->completion_status().error_code);

  histogram_tester.ExpectUniqueSample(
      kHistogramSubresourceFetchEvent,
      blink::ServiceWorkerStatusCode::kErrorStartWorkerFailed, 1);

  if (LoaderRecordsTimingMetrics()) {
    // Timing histograms shouldn't be recorded on failure.
    histogram_tester.ExpectTotalCount(
        "ServiceWorker.LoadTiming.Subresource."
        "ForwardServiceWorkerToWorkerReady",
        0);
    histogram_tester.ExpectTotalCount(
        "ServiceWorker.LoadTiming.Subresource.ResponseReceivedToCompleted2", 0);
  }
}

TEST_F(ServiceWorkerSubresourceLoaderTest,
       DropController_RestartFetchEvent_RaceNetworkRequest) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kServiceWorkerAutoPreload, {{"strategy", "opt-in"}});

  mojo::Remote<network::mojom::URLLoaderFactory> factory =
      CreateSubresourceLoaderFactory();

  {
    network::ResourceRequest request =
        CreateRequest(GURL("https://www.example.com/foo.png"));
    mojo::Remote<network::mojom::URLLoader> loader;
    std::unique_ptr<network::TestURLLoaderClient> client;
    StartRequest(factory, request, &loader, &client);
    fake_controller_.RunUntilFetchEvent();

    EXPECT_EQ(request.url, fake_controller_.fetch_event_request().url);
    EXPECT_EQ(request.method, fake_controller_.fetch_event_request().method);
    EXPECT_EQ(1, fake_controller_.fetch_event_count());
    EXPECT_EQ(1, fake_container_host_.get_controller_service_worker_count());
  }

  // Loading another resource reuses the existing connection to the
  // ControllerServiceWorker (i.e. it doesn't increase the get controller
  // service worker count).
  {
    network::ResourceRequest request =
        CreateRequest(GURL("https://www.example.com/foo2.png"));
    mojo::Remote<network::mojom::URLLoader> loader;
    std::unique_ptr<network::TestURLLoaderClient> client;
    StartRequest(factory, request, &loader, &client);
    fake_controller_.RunUntilFetchEvent();

    EXPECT_EQ(request.url, fake_controller_.fetch_event_request().url);
    EXPECT_EQ(request.method, fake_controller_.fetch_event_request().method);
    EXPECT_EQ(2, fake_controller_.fetch_event_count());
    EXPECT_EQ(1, fake_container_host_.get_controller_service_worker_count());
    client->RunUntilComplete();
  }

  base::HistogramTester histogram_tester;

  network::ResourceRequest request =
      CreateRequest(GURL("https://www.example.com/foo3.png"));
  mojo::Remote<network::mojom::URLLoader> loader;
  std::unique_ptr<network::TestURLLoaderClient> client;
  StartRequest(factory, request, &loader, &client);

  // Drop the connection to the ControllerServiceWorker.
  fake_controller_.ClearReceivers();
  base::RunLoop().RunUntilIdle();

  // If connection is closed during fetch event, it's restarted and successfully
  // finishes.
  EXPECT_EQ(request.url, fake_controller_.fetch_event_request().url);
  EXPECT_EQ(request.method, fake_controller_.fetch_event_request().method);
  EXPECT_EQ(3, fake_controller_.fetch_event_count());
  EXPECT_EQ(2, fake_container_host_.get_controller_service_worker_count());
  histogram_tester.ExpectUniqueSample(kHistogramSubresourceFetchEvent,
                                      blink::ServiceWorkerStatusCode::kOk, 1);
}

TEST_F(ServiceWorkerSubresourceLoaderTest, StreamResponse) {
  base::HistogramTester histogram_tester;

  // Construct the Stream to respond with.
  const std::string_view kResponseBody = "Here is sample text for the Stream.";
  mojo::Remote<blink::mojom::ServiceWorkerStreamCallback> stream_callback;
  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  ASSERT_EQ(mojo::CreateDataPipe(nullptr, producer_handle, consumer_handle),
            MOJO_RESULT_OK);
  fake_controller_.RespondWithStream(
      stream_callback.BindNewPipeAndPassReceiver(), std::move(consumer_handle));
  fake_controller_.SetResponseSource(
      network::mojom::FetchResponseSource::kNetwork);

  mojo::Remote<network::mojom::URLLoaderFactory> factory =
      CreateSubresourceLoaderFactory();

  // Perform the request.
  network::ResourceRequest request =
      CreateRequest(GURL("https://www.example.com/foo.png"));
  mojo::Remote<network::mojom::URLLoader> loader;
  std::unique_ptr<network::TestURLLoaderClient> client;
  StartRequest(factory, request, &loader, &client);
  client->RunUntilResponseReceived();

  auto& info = client->response_head();
  auto expected_info = CreateResponseInfoFromServiceWorker();
  expected_info->service_worker_response_source =
      network::mojom::FetchResponseSource::kNetwork;
  ExpectResponseInfo(*info, *expected_info);

  // Write the body stream.
  size_t bytes_written = 0;
  MojoResult mojo_result =
      producer_handle->WriteData(base::as_byte_span(kResponseBody),
                                 MOJO_WRITE_DATA_FLAG_NONE, bytes_written);
  ASSERT_EQ(MOJO_RESULT_OK, mojo_result);
  EXPECT_EQ(kResponseBody.size(), bytes_written);
  stream_callback->OnCompleted();
  producer_handle.reset();

  client->RunUntilComplete();
  EXPECT_EQ(net::OK, client->completion_status().error_code);

  // Test the body.
  std::string response;
  EXPECT_TRUE(client->response_body().is_valid());
  EXPECT_TRUE(
      mojo::BlockingCopyToString(client->response_body_release(), &response));
  EXPECT_EQ(kResponseBody, response);

  histogram_tester.ExpectUniqueSample(kHistogramSubresourceFetchEvent,
                                      blink::ServiceWorkerStatusCode::kOk, 1);

  if (LoaderRecordsTimingMetrics()) {
    // Test timing histograms of reading body.
    histogram_tester.ExpectTotalCount(
        "ServiceWorker.LoadTiming.Subresource."
        "ForwardServiceWorkerToWorkerReady",
        1);
    histogram_tester.ExpectTotalCount(
        "ServiceWorker.LoadTiming.Subresource.ResponseReceivedToCompleted2", 1);
    histogram_tester.ExpectTotalCount(
        "ServiceWorker.LoadTiming.Subresource.ResponseReceivedToCompleted2."
        "Network",
        1);
  }
}

TEST_F(ServiceWorkerSubresourceLoaderTest, StreamResponse_Abort) {
  base::HistogramTester histogram_tester;

  // Construct the Stream to respond with.
  const std::string_view kResponseBody = "Here is sample text for the Stream.";
  mojo::Remote<blink::mojom::ServiceWorkerStreamCallback> stream_callback;
  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  ASSERT_EQ(mojo::CreateDataPipe(nullptr, producer_handle, consumer_handle),
            MOJO_RESULT_OK);
  fake_controller_.RespondWithStream(
      stream_callback.BindNewPipeAndPassReceiver(), std::move(consumer_handle));

  mojo::Remote<network::mojom::URLLoaderFactory> factory =
      CreateSubresourceLoaderFactory();

  // Perform the request.
  network::ResourceRequest request =
      CreateRequest(GURL("https://www.example.com/foo.txt"));
  mojo::Remote<network::mojom::URLLoader> loader;
  std::unique_ptr<network::TestURLLoaderClient> client;
  StartRequest(factory, request, &loader, &client);
  client->RunUntilResponseReceived();

  auto& info = client->response_head();
  ExpectResponseInfo(*info, *CreateResponseInfoFromServiceWorker());

  // Start writing the body stream, then abort before finishing.
  size_t bytes_written = 0;
  MojoResult mojo_result =
      producer_handle->WriteData(base::as_byte_span(kResponseBody),
                                 MOJO_WRITE_DATA_FLAG_NONE, bytes_written);
  ASSERT_EQ(MOJO_RESULT_OK, mojo_result);
  EXPECT_EQ(kResponseBody.size(), bytes_written);
  stream_callback->OnAborted();
  producer_handle.reset();

  client->RunUntilComplete();
  EXPECT_EQ(net::ERR_ABORTED, client->completion_status().error_code);

  // Test the body.
  std::string response;
  EXPECT_TRUE(client->response_body().is_valid());
  EXPECT_TRUE(
      mojo::BlockingCopyToString(client->response_body_release(), &response));
  EXPECT_EQ(kResponseBody, response);

  histogram_tester.ExpectUniqueSample(kHistogramSubresourceFetchEvent,
                                      blink::ServiceWorkerStatusCode::kOk, 1);

  if (LoaderRecordsTimingMetrics()) {
    // Timing histograms shouldn't be recorded on abort.
    histogram_tester.ExpectTotalCount(
        "ServiceWorker.LoadTiming.Subresource."
        "ForwardServiceWorkerToWorkerReady",
        0);
    histogram_tester.ExpectTotalCount(
        "ServiceWorker.LoadTiming.Subresource.ResponseReceivedToCompleted2", 0);
  }
}

TEST_F(ServiceWorkerSubresourceLoaderTest, BlobResponse) {
  base::HistogramTester histogram_tester;

  // Construct the Blob to respond with.
  const std::string kResponseBody = "/* Here is sample text for the Blob. */";
  const std::vector<uint8_t> kMetadata = {0xE3, 0x81, 0x8F, 0xE3, 0x82,
                                          0x8D, 0xE3, 0x81, 0xBF, 0xE3,
                                          0x81, 0x86, 0xE3, 0x82, 0x80};
  fake_controller_.RespondWithBlob(kMetadata, kResponseBody);
  fake_controller_.SetResponseSource(
      network::mojom::FetchResponseSource::kCacheStorage);
  std::string cache_name = "v2";
  fake_controller_.SetCacheStorageCacheName(cache_name);
  base::Time response_time = base::Time::Now();
  fake_controller_.SetResponseTime(response_time);

  mojo::Remote<network::mojom::URLLoaderFactory> factory =
      CreateSubresourceLoaderFactory();

  // Perform the request.
  network::ResourceRequest request =
      CreateRequest(GURL("https://www.example.com/foo.js"));
  request.destination = network::mojom::RequestDestination::kScript;
  mojo::Remote<network::mojom::URLLoader> loader;
  std::unique_ptr<network::TestURLLoaderClient> client;
  StartRequest(factory, request, &loader, &client);
  client->RunUntilResponseReceived();

  auto expected_info = CreateResponseInfoFromServiceWorker();
  auto& info = client->response_head();
  expected_info->response_time = response_time;
  expected_info->cache_storage_cache_name = cache_name;
  expected_info->service_worker_response_source =
      network::mojom::FetchResponseSource::kCacheStorage;
  ExpectResponseInfo(*info, *expected_info);
  EXPECT_EQ(39, info->content_length);

  // Test the cached metadata.
  EXPECT_EQ(*client->cached_metadata(),
            std::string(kMetadata.begin(), kMetadata.end()));

  client->RunUntilComplete();
  EXPECT_EQ(net::OK, client->completion_status().error_code);

  // Test the body.
  std::string response;
  EXPECT_TRUE(client->response_body().is_valid());
  EXPECT_TRUE(
      mojo::BlockingCopyToString(client->response_body_release(), &response));
  EXPECT_EQ(kResponseBody, response);

  histogram_tester.ExpectUniqueSample(kHistogramSubresourceFetchEvent,
                                      blink::ServiceWorkerStatusCode::kOk, 1);

  if (LoaderRecordsTimingMetrics()) {
    // Test timing histograms of reading body.
    histogram_tester.ExpectTotalCount(
        "ServiceWorker.LoadTiming.Subresource."
        "ForwardServiceWorkerToWorkerReady",
        1);
    histogram_tester.ExpectTotalCount(
        "ServiceWorker.LoadTiming.Subresource.ResponseReceivedToCompleted2", 1);
    histogram_tester.ExpectTotalCount(
        "ServiceWorker.LoadTiming.Subresource.ResponseReceivedToCompleted2."
        "CacheStorage",
        1);
  }
}

TEST_F(ServiceWorkerSubresourceLoaderTest, BlobResponseWithoutMetadata) {
  base::HistogramTester histogram_tester;

  // Construct the Blob to respond with.
  const std::string kResponseBody = "/* Here is sample text for the Blob. */";
  fake_controller_.RespondWithBlob(std::nullopt, kResponseBody);

  mojo::Remote<network::mojom::URLLoaderFactory> factory =
      CreateSubresourceLoaderFactory();

  // Perform the request.
  network::ResourceRequest request =
      CreateRequest(GURL("https://www.example.com/foo.js"));
  request.destination = network::mojom::RequestDestination::kScript;
  mojo::Remote<network::mojom::URLLoader> loader;
  std::unique_ptr<network::TestURLLoaderClient> client;
  StartRequest(factory, request, &loader, &client);
  client->RunUntilResponseReceived();

  auto& info = client->response_head();
  ExpectResponseInfo(*info, *CreateResponseInfoFromServiceWorker());

  client->RunUntilComplete();
  EXPECT_EQ(net::OK, client->completion_status().error_code);

  // Test the body.
  std::string response;
  EXPECT_TRUE(client->response_body().is_valid());
  EXPECT_TRUE(
      mojo::BlockingCopyToString(client->response_body_release(), &response));
  EXPECT_EQ(kResponseBody, response);
  EXPECT_FALSE(client->cached_metadata().has_value());

  histogram_tester.ExpectUniqueSample(kHistogramSubresourceFetchEvent,
                                      blink::ServiceWorkerStatusCode::kOk, 1);

  if (LoaderRecordsTimingMetrics()) {
    // Test timing histograms of reading body.
    histogram_tester.ExpectTotalCount(
        "ServiceWorker.LoadTiming.Subresource."
        "ForwardServiceWorkerToWorkerReady",
        1);
    histogram_tester.ExpectTotalCount(
        "ServiceWorker.LoadTiming.Subresource.ResponseReceivedToCompleted2", 1);
  }
}

TEST_F(ServiceWorkerSubresourceLoaderTest, BlobResponseNonScript) {
  // Construct the Blob to respond with.
  const std::string kResponseBody = "Here is sample text for the Blob.";
  const std::vector<uint8_t> kMetadata = {0xE3, 0x81, 0x8F, 0xE3, 0x82,
                                          0x8D, 0xE3, 0x81, 0xBF, 0xE3,
                                          0x81, 0x86, 0xE3, 0x82, 0x80};
  fake_controller_.RespondWithBlob(kMetadata, kResponseBody);
  fake_controller_.SetResponseSource(
      network::mojom::FetchResponseSource::kCacheStorage);

  mojo::Remote<network::mojom::URLLoaderFactory> factory =
      CreateSubresourceLoaderFactory();

  // Perform the request.
  network::ResourceRequest request =
      CreateRequest(GURL("https://www.example.com/foo.txt"));
  request.destination = network::mojom::RequestDestination::kEmpty;
  mojo::Remote<network::mojom::URLLoader> loader;
  std::unique_ptr<network::TestURLLoaderClient> client;
  StartRequest(factory, request, &loader, &client);
  client->RunUntilResponseReceived();

  auto& info = client->response_head();
  auto expected_info = CreateResponseInfoFromServiceWorker();
  expected_info->service_worker_response_source =
      network::mojom::FetchResponseSource::kCacheStorage;
  ExpectResponseInfo(*info, *expected_info);
  EXPECT_EQ(33, info->content_length);

  client->RunUntilComplete();
  EXPECT_EQ(net::OK, client->completion_status().error_code);

  // Even though the blob has metadata, verify that the client didn't receive
  // it because this is not a script resource.
  EXPECT_FALSE(client->cached_metadata().has_value());

  // Test the body.
  std::string response;
  EXPECT_TRUE(client->response_body().is_valid());
  EXPECT_TRUE(
      mojo::BlockingCopyToString(client->response_body_release(), &response));
  EXPECT_EQ(kResponseBody, response);
}

// Test when the service worker responds with network fallback.
// i.e., does not call respondWith().
TEST_F(ServiceWorkerSubresourceLoaderTest, FallbackResponse) {
  base::HistogramTester histogram_tester;
  fake_controller_.RespondWithFallback();

  mojo::Remote<network::mojom::URLLoaderFactory> factory =
      CreateSubresourceLoaderFactory();

  // Perform the request.
  network::ResourceRequest request =
      CreateRequest(GURL("https://www.example.com/foo.png"));
  mojo::Remote<network::mojom::URLLoader> loader;
  std::unique_ptr<network::TestURLLoaderClient> client;
  StartRequest(factory, request, &loader, &client);
  client->RunUntilComplete();

  // OnFallback() should complete the network request using network loader.
  EXPECT_TRUE(client->has_received_completion());
  EXPECT_FALSE(client->response_head()->was_fetched_via_service_worker);

  histogram_tester.ExpectUniqueSample(kHistogramSubresourceFetchEvent,
                                      blink::ServiceWorkerStatusCode::kOk, 1);

  if (LoaderRecordsTimingMetrics()) {
    // Test timing histograms of network fallback.
    histogram_tester.ExpectTotalCount(
        "ServiceWorker.LoadTiming.Subresource."
        "ForwardServiceWorkerToWorkerReady",
        1);
    histogram_tester.ExpectTotalCount(
        "ServiceWorker.LoadTiming.Subresource.FetchHandlerEndToFallbackNetwork",
        1);
  }
}

TEST_F(ServiceWorkerSubresourceLoaderTest, ErrorResponse) {
  base::HistogramTester histogram_tester;
  fake_controller_.RespondWithError();

  mojo::Remote<network::mojom::URLLoaderFactory> factory =
      CreateSubresourceLoaderFactory();

  // Perform the request.
  network::ResourceRequest request =
      CreateRequest(GURL("https://www.example.com/foo.png"));
  mojo::Remote<network::mojom::URLLoader> loader;
  std::unique_ptr<network::TestURLLoaderClient> client;
  StartRequest(factory, request, &loader, &client);
  client->RunUntilComplete();

  EXPECT_EQ(net::ERR_FAILED, client->completion_status().error_code);
  histogram_tester.ExpectUniqueSample(kHistogramSubresourceFetchEvent,
                                      blink::ServiceWorkerStatusCode::kOk, 1);

  if (LoaderRecordsTimingMetrics()) {
    // Timing histograms shouldn't be recorded when we receive an error
    // response.
    histogram_tester.ExpectTotalCount(
        "ServiceWorker.LoadTiming.Subresource."
        "ForwardServiceWorkerToWorkerReady",
        0);
    histogram_tester.ExpectTotalCount(
        "ServiceWorker.LoadTiming.Subresource.ResponseReceivedToCompleted2", 0);
  }
}

TEST_F(ServiceWorkerSubresourceLoaderTest, RedirectResponse) {
  base::HistogramTester histogram_tester;
  fake_controller_.RespondWithRedirect("https://www.example.com/bar.png");

  mojo::Remote<network::mojom::URLLoaderFactory> factory =
      CreateSubresourceLoaderFactory();

  // Perform the request.
  network::ResourceRequest request =
      CreateRequest(GURL("https://www.example.com/foo.png"));
  mojo::Remote<network::mojom::URLLoader> loader;
  std::unique_ptr<network::TestURLLoaderClient> client;
  StartRequest(factory, request, &loader, &client);
  client->RunUntilRedirectReceived();

  EXPECT_EQ(net::OK, client->completion_status().error_code);
  EXPECT_TRUE(client->has_received_redirect());
  {
    const net::RedirectInfo& redirect_info = client->redirect_info();
    EXPECT_EQ(302, redirect_info.status_code);
    EXPECT_EQ("GET", redirect_info.new_method);
    EXPECT_EQ(GURL("https://www.example.com/bar.png"), redirect_info.new_url);
  }
  client->ClearHasReceivedRedirect();

  // Redirect once more.
  fake_controller_.RespondWithRedirect("https://other.example.com/baz.png");
  loader->FollowRedirect({}, {}, {}, std::nullopt);
  client->RunUntilRedirectReceived();

  EXPECT_EQ(net::OK, client->completion_status().error_code);
  EXPECT_TRUE(client->has_received_redirect());
  {
    const net::RedirectInfo& redirect_info = client->redirect_info();
    EXPECT_EQ(302, redirect_info.status_code);
    EXPECT_EQ("GET", redirect_info.new_method);
    EXPECT_EQ(GURL("https://other.example.com/baz.png"), redirect_info.new_url);
  }
  client->ClearHasReceivedRedirect();

  // Give the final response.
  const std::string_view kResponseBody = "Here is sample text for the Stream.";
  mojo::Remote<blink::mojom::ServiceWorkerStreamCallback> stream_callback;
  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  ASSERT_EQ(mojo::CreateDataPipe(nullptr, producer_handle, consumer_handle),
            MOJO_RESULT_OK);
  fake_controller_.RespondWithStream(
      stream_callback.BindNewPipeAndPassReceiver(), std::move(consumer_handle));
  loader->FollowRedirect({}, {}, {}, std::nullopt);
  client->RunUntilResponseReceived();

  auto& info = client->response_head();
  EXPECT_EQ(200, info->headers->response_code());
  EXPECT_EQ(network::mojom::FetchResponseType::kDefault, info->response_type);

  // Write the body stream.
  size_t bytes_written = 0;
  MojoResult mojo_result =
      producer_handle->WriteData(base::as_byte_span(kResponseBody),
                                 MOJO_WRITE_DATA_FLAG_NONE, bytes_written);
  ASSERT_EQ(MOJO_RESULT_OK, mojo_result);
  EXPECT_EQ(kResponseBody.size(), bytes_written);
  stream_callback->OnCompleted();
  producer_handle.reset();

  client->RunUntilComplete();
  EXPECT_EQ(net::OK, client->completion_status().error_code);

  // Test the body.
  std::string response;
  EXPECT_TRUE(client->response_body().is_valid());
  EXPECT_TRUE(
      mojo::BlockingCopyToString(client->response_body_release(), &response));
  EXPECT_EQ(kResponseBody, response);

  // There were 3 fetch events, so expect a count of 3.
  histogram_tester.ExpectUniqueSample(kHistogramSubresourceFetchEvent,
                                      blink::ServiceWorkerStatusCode::kOk, 3);
}

TEST_F(ServiceWorkerSubresourceLoaderTest, TooManyRedirects) {
  base::HistogramTester histogram_tester;

  int count = 1;
  std::string redirect_location =
      std::string("https://www.example.com/redirect_") +
      base::NumberToString(count);
  fake_controller_.RespondWithRedirect(redirect_location);
  mojo::Remote<network::mojom::URLLoaderFactory> factory =
      CreateSubresourceLoaderFactory();

  // Perform the request.
  network::ResourceRequest request =
      CreateRequest(GURL("https://www.example.com/foo.png"));
  mojo::Remote<network::mojom::URLLoader> loader;
  std::unique_ptr<network::TestURLLoaderClient> client;
  StartRequest(factory, request, &loader, &client);

  // The Fetch spec says: "If request’s redirect count is twenty, return a
  // network error." https://fetch.spec.whatwg.org/#http-redirect-fetch
  // So fetch can follow the redirect response until 20 times.
  static_assert(net::URLRequest::kMaxRedirects == 20,
                "The Fetch spec requires kMaxRedirects to be 20");
  for (; count < net::URLRequest::kMaxRedirects + 1; ++count) {
    client->RunUntilRedirectReceived();

    EXPECT_TRUE(client->has_received_redirect());
    EXPECT_EQ(net::OK, client->completion_status().error_code);
    const net::RedirectInfo& redirect_info = client->redirect_info();
    EXPECT_EQ(302, redirect_info.status_code);
    EXPECT_EQ("GET", redirect_info.new_method);
    EXPECT_EQ(GURL(redirect_location), redirect_info.new_url);

    client->ClearHasReceivedRedirect();

    // Redirect more.
    redirect_location = std::string("https://www.example.com/redirect_") +
                        base::NumberToString(count);
    fake_controller_.RespondWithRedirect(redirect_location);
    loader->FollowRedirect({}, {}, {}, std::nullopt);
  }
  client->RunUntilComplete();

  // Fetch can't follow the redirect response 21 times.
  EXPECT_FALSE(client->has_received_redirect());
  EXPECT_EQ(net::ERR_TOO_MANY_REDIRECTS,
            client->completion_status().error_code);

  // Expect a sample for each fetch event (kMaxRedirects + 1).
  histogram_tester.ExpectUniqueSample(kHistogramSubresourceFetchEvent,
                                      blink::ServiceWorkerStatusCode::kOk,
                                      net::URLRequest::kMaxRedirects + 1);
}

TEST_F(ServiceWorkerSubresourceLoaderTest, FollowNonexistentRedirect) {
  // Delay the response from the service worker indefinitely so the test can
  // run without races.
  fake_controller_.DontRespond();

  // Start a request.
  mojo::Remote<network::mojom::URLLoaderFactory> factory =
      CreateSubresourceLoaderFactory();
  network::ResourceRequest request =
      CreateRequest(GURL("https://www.example.com/foo.png"));
  mojo::Remote<network::mojom::URLLoader> loader;
  std::unique_ptr<network::TestURLLoaderClient> client;
  StartRequest(factory, request, &loader, &client);

  // Tell the loader to follow a non-existent redirect. It should complete
  // with network error.
  loader->FollowRedirect({}, {}, {}, std::nullopt);
  client->RunUntilComplete();
  EXPECT_EQ(net::ERR_INVALID_REDIRECT, client->completion_status().error_code);
}

TEST_F(ServiceWorkerSubresourceLoaderTest, FallbackWithRequestBody_String) {
  const std::string kData = "Hi, this is the request body (string)";
  auto request_body = base::MakeRefCounted<network::ResourceRequestBody>();
  request_body->AppendBytes(kData.c_str(), kData.length());

  RunFallbackWithRequestBodyTest(std::move(request_body), kData);
}

TEST_F(ServiceWorkerSubresourceLoaderTest, FallbackWithRequestBody_DataPipe) {
  const std::string kData = "Hi, this is the request body (data pipe)";
  auto request_body = base::MakeRefCounted<network::ResourceRequestBody>();
  mojo::PendingRemote<network::mojom::DataPipeGetter> data_pipe_getter_remote;
  auto data_pipe_getter = std::make_unique<network::TestDataPipeGetter>(
      kData, data_pipe_getter_remote.InitWithNewPipeAndPassReceiver());
  request_body->AppendDataPipe(std::move(data_pipe_getter_remote));

  RunFallbackWithRequestBodyTest(std::move(request_body), kData);
}

// Test a range request that the service worker responds to with a 200
// (non-ranged) response. The client should get the entire response as-is from
// the service worker.
TEST_F(ServiceWorkerSubresourceLoaderTest, RangeRequest_200Response) {
  // Construct the Blob to respond with.
  const std::string kResponseBody = "Here is sample text for the Blob.";
  fake_controller_.RespondWithBlob(std::nullopt, kResponseBody);

  // Perform the request.
  std::unique_ptr<network::TestURLLoaderClient> client =
      DoRangeRequest("bytes=5-13");
  EXPECT_EQ(net::OK, client->completion_status().error_code);

  // Test the response.
  auto& info = client->response_head();
  ExpectResponseInfo(*info, *CreateResponseInfoFromServiceWorker());
  EXPECT_EQ(33, info->content_length);
  EXPECT_FALSE(info->headers->HasHeader("Content-Range"));
  EXPECT_EQ(kResponseBody, TakeResponseBody(client.get()));
}

// Test a range request that the service worker responds to with a 206 ranged
// response. The client should get the partial response as-is from the service
// worker.
TEST_F(ServiceWorkerSubresourceLoaderTest, RangeRequest_206Response) {
  // Tell the controller to respond with a 206 response.
  const std::string kResponseBody = "Here is sample text for the Blob.";
  fake_controller_.RespondWithBlobRange(kResponseBody);

  // Perform the request.
  std::unique_ptr<network::TestURLLoaderClient> client =
      DoRangeRequest("bytes=5-13");
  EXPECT_EQ(net::OK, client->completion_status().error_code);

  // Test the response.
  auto& info = client->response_head();
  EXPECT_EQ(206, info->headers->response_code());
  std::string range;
  ASSERT_TRUE(info->headers->GetNormalizedHeader("Content-Range", &range));
  EXPECT_EQ("bytes 5-13/33", range);
  EXPECT_EQ(9, info->content_length);
  EXPECT_EQ("is sample", TakeResponseBody(client.get()));
}

// Test a range request that the service worker responds to with a 206 ranged
// response. The requested range has an unbounded end. The client should get the
// partial response as-is from the service worker.
TEST_F(ServiceWorkerSubresourceLoaderTest,
       RangeRequest_UnboundedRight_206Response) {
  // Tell the controller to respond with a 206 response.
  const std::string kResponseBody = "Here is sample text for the Blob.";
  fake_controller_.RespondWithBlobRange(kResponseBody);

  // Perform the request.
  std::unique_ptr<network::TestURLLoaderClient> client =
      DoRangeRequest("bytes=5-");
  EXPECT_EQ(net::OK, client->completion_status().error_code);

  // Test the response.
  auto& info = client->response_head();
  EXPECT_EQ(206, info->headers->response_code());
  std::string range;
  ASSERT_TRUE(info->headers->GetNormalizedHeader("Content-Range", &range));
  EXPECT_EQ("bytes 5-32/33", range);
  EXPECT_EQ(28, info->content_length);
  EXPECT_EQ("is sample text for the Blob.", TakeResponseBody(client.get()));
}

}  // namespace service_worker_subresource_loader_unittest
}  // namespace content
