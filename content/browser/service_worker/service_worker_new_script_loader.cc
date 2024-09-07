// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_new_script_loader.h"

#include <memory>
#include <vector>

#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/renderer_host/policy_container_host.h"
#include "content/browser/service_worker/service_worker_cache_writer.h"
#include "content/browser/service_worker/service_worker_consts.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_loader_helpers.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/public/browser/url_loader_throttles.h"
#include "content/public/common/content_client.h"
#include "net/base/ip_endpoint.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_status_flags.h"
#include "net/http/http_response_info.h"
#include "services/network/public/mojom/early_hints.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/loader/throttling_url_loader.h"

namespace content {

namespace {
constexpr char kServiceWorkerNewScriptLoaderScope[] =
    "ServiceWorkerNewScriptLoader";
}  // namespace

// We chose this size because the AppCache uses this.
const uint32_t ServiceWorkerNewScriptLoader::kReadBufferSize = 32768;

// This is for debugging https://crbug.com/959627.
// The purpose is to see where the IOBuffer comes from by checking |__vfptr|.
class ServiceWorkerNewScriptLoader::WrappedIOBuffer
    : public net::WrappedIOBuffer {
 public:
  WrappedIOBuffer(const char* data, size_t size)
      : net::WrappedIOBuffer(base::make_span(data, size)) {}

 private:
  ~WrappedIOBuffer() override = default;

  // This is to make sure that the vtable is not merged with other classes.
  virtual void dummy() {
    // TODO(crbug.com/40220780): Change back to NOTREACHED() once the
    // cause of the bug is identified.
    CHECK(false);  // NOTREACHED
  }
};

std::unique_ptr<ServiceWorkerNewScriptLoader>
ServiceWorkerNewScriptLoader::CreateAndStart(
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& original_request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    scoped_refptr<ServiceWorkerVersion> version,
    scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    int64_t cache_resource_id,
    bool is_throttle_needed,
    const GlobalRenderFrameHostId& requesting_frame_id) {
  return base::WrapUnique(new ServiceWorkerNewScriptLoader(
      request_id, options, original_request, std::move(client), version,
      loader_factory, traffic_annotation, cache_resource_id, is_throttle_needed,
      requesting_frame_id));
}

// TODO(nhiroki): We're doing multiple things in the ctor. Consider factors out
// some of them into a separate function.
ServiceWorkerNewScriptLoader::ServiceWorkerNewScriptLoader(
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& original_request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    scoped_refptr<ServiceWorkerVersion> version,
    scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    int64_t cache_resource_id,
    bool is_throttle_needed,
    const GlobalRenderFrameHostId& requesting_frame_id)
    : request_id_(request_id),
      request_url_(original_request.url),
      is_main_script_(original_request.destination ==
                          network::mojom::RequestDestination::kServiceWorker &&
                      original_request.mode ==
                          network::mojom::RequestMode::kSameOrigin),
      original_options_(options),
      version_(version),
      network_watcher_(FROM_HERE,
                       mojo::SimpleWatcher::ArmingPolicy::MANUAL,
                       base::SequencedTaskRunner::GetCurrentDefault()),
      loader_factory_(std::move(loader_factory)),
      client_(std::move(client)),
      client_producer_watcher_(FROM_HERE,
                               mojo::SimpleWatcher::ArmingPolicy::MANUAL,
                               base::SequencedTaskRunner::GetCurrentDefault()),
      requesting_frame_id_(requesting_frame_id) {
  TRACE_EVENT_WITH_FLOW1(
      "ServiceWorker",
      "ServiceWorkerNewScriptLoader::ServiceWorkerNewScriptLoader",
      TRACE_ID_WITH_SCOPE(kServiceWorkerNewScriptLoaderScope,
                          TRACE_ID_LOCAL(request_id_)),
      TRACE_EVENT_FLAG_FLOW_OUT, "request_url", request_url_);
  CHECK_NE(cache_resource_id, blink::mojom::kInvalidServiceWorkerResourceId);

  network::ResourceRequest resource_request(original_request);
#if DCHECK_IS_ON()
  service_worker_loader_helpers::CheckVersionStatusBeforeWorkerScriptLoad(
      version_->status(), is_main_script_, version_->script_type());
#endif  // DCHECK_IS_ON()

  scoped_refptr<ServiceWorkerRegistration> registration =
      version_->context()->GetLiveRegistration(version_->registration_id());
  // ServiceWorkerVersion keeps the registration alive while the service
  // worker is starting up, and it must be starting up here.
  CHECK(registration);

  // We need to filter on mode, since module imports use kServiceWorker as
  // destination, but only top level module scripts are same-origin.
  if (is_main_script_) {
    // Request SSLInfo. It will be persisted in service worker storage and
    // may be used by ServiceWorkerMainResourceLoader for navigations handled
    // by this service worker.
    options |= network::mojom::kURLLoadOptionSendSSLInfoWithResponse;
    resource_request.headers.SetHeader("Service-Worker", "script");
  }

  // Validate the browser cache if needed, e.g., updateViaCache demands it or 24
  // hours passed since the last update check that hit network.
  base::TimeDelta time_since_last_check =
      base::Time::Now() - registration->last_update_check();
  if (service_worker_loader_helpers::ShouldValidateBrowserCacheForScript(
          is_main_script_, version_->force_bypass_cache_for_scripts(),
          registration->update_via_cache(), time_since_last_check)) {
    resource_request.load_flags |= net::LOAD_VALIDATE_CACHE;
  }

  mojo::Remote<storage::mojom::ServiceWorkerResourceWriter> writer;
  version_->context()
      ->registry()
      ->GetRemoteStorageControl()
      ->CreateResourceWriter(cache_resource_id,
                             writer.BindNewPipeAndPassReceiver());

  cache_writer_ = ServiceWorkerCacheWriter::CreateForWriteBack(
      std::move(writer), cache_resource_id);

  version_->script_cache_map()->NotifyStartedCaching(request_url_,
                                                     cache_resource_id);

  // Disable MIME sniffing. The spec requires the header list to have a
  // JavaScript MIME type. Therefore, no sniffing is needed.
  options &= ~network::mojom::kURLLoadOptionSniffMimeType;

  std::vector<std::unique_ptr<blink::URLLoaderThrottle>> throttles;
  if (is_throttle_needed) {
    // A service worker is independent from WebContents and FrameTreeNode.
    // Return null or empty values when queried for either.
    base::RepeatingCallback<WebContents*()> web_contents_getter =
        base::BindRepeating([]() -> WebContents* { return nullptr; });
    throttles = CreateContentBrowserURLLoaderThrottles(
        resource_request, version_->context()->wrapper()->browser_context(),
        std::move(web_contents_getter),
        /*navigation_ui_data=*/nullptr, FrameTreeNodeId(),
        /*navigation_id=*/std::nullopt);
  }

  network_loader_ = blink::ThrottlingURLLoader::CreateLoaderAndStart(
      std::move(loader_factory_), std::move(throttles), request_id, options,
      &resource_request, this,
      net::NetworkTrafficAnnotationTag(traffic_annotation),
      base::SingleThreadTaskRunner::GetCurrentDefault());

  CHECK_EQ(LoaderState::kNotStarted, network_loader_state_);
  network_loader_state_ = LoaderState::kLoadingHeader;
}

ServiceWorkerNewScriptLoader::~ServiceWorkerNewScriptLoader() {
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker",
      "ServiceWorkerNewScriptLoader::~ServiceWorkerNewScriptLoader",
      TRACE_ID_WITH_SCOPE(kServiceWorkerNewScriptLoaderScope,
                          TRACE_ID_LOCAL(request_id_)),
      TRACE_EVENT_FLAG_FLOW_IN);
  // This class is used as a SelfOwnedReceiver and its lifetime is tied to the
  // corresponding mojo connection. There could be cases where the mojo
  // connection is disconnected while writing the response to the storage.
  // Complete this loader with ERR_FAILED in such cases to update the script
  // cache map.
  bool writers_completed = header_writer_state_ == WriterState::kCompleted &&
                           body_writer_state_ == WriterState::kCompleted;
  if (network_loader_state_ == LoaderState::kCompleted && !writers_completed) {
    CHECK(client_);
    CommitCompleted(network::URLLoaderCompletionStatus(net::ERR_FAILED),
                    ServiceWorkerConsts::kServiceWorkerInvalidVersionError,
                    nullptr);
  }
}

void ServiceWorkerNewScriptLoader::FollowRedirect(
    const std::vector<std::string>& removed_headers,
    const net::HttpRequestHeaders& modified_headers,
    const net::HttpRequestHeaders& modified_cors_exempt_headers,
    const std::optional<GURL>& new_url) {
  // Resource requests for service worker scripts should not follow redirects.
  // See comments in OnReceiveRedirect().
  CHECK(false);  // NOTREACHED
}

void ServiceWorkerNewScriptLoader::SetPriority(net::RequestPriority priority,
                                               int32_t intra_priority_value) {
  TRACE_EVENT_WITH_FLOW0("ServiceWorker",
                         "ServiceWorkerNewScriptLoader::SetPriority",
                         TRACE_ID_WITH_SCOPE(kServiceWorkerNewScriptLoaderScope,
                                             TRACE_ID_LOCAL(request_id_)),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  if (network_loader_)
    network_loader_->SetPriority(priority, intra_priority_value);
}

void ServiceWorkerNewScriptLoader::PauseReadingBodyFromNet() {
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker", "ServiceWorkerNewScriptLoader::PauseReadingBodyFromNet",
      TRACE_ID_WITH_SCOPE(kServiceWorkerNewScriptLoaderScope,
                          TRACE_ID_LOCAL(request_id_)),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  if (network_loader_)
    network_loader_->PauseReadingBodyFromNet();
}

void ServiceWorkerNewScriptLoader::ResumeReadingBodyFromNet() {
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker", "ServiceWorkerNewScriptLoader::ResumeReadingBodyFromNet",
      TRACE_ID_WITH_SCOPE(kServiceWorkerNewScriptLoaderScope,
                          TRACE_ID_LOCAL(request_id_)),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  if (network_loader_)
    network_loader_->ResumeReadingBodyFromNet();
}

// URLLoaderClient for network loader ------------------------------------------

void ServiceWorkerNewScriptLoader::OnReceiveEarlyHints(
    network::mojom::EarlyHintsPtr early_hints) {
  TRACE_EVENT_WITH_FLOW0("ServiceWorker",
                         "ServiceWorkerNewScriptLoader::OnReceiveEarlyHints",
                         TRACE_ID_WITH_SCOPE(kServiceWorkerNewScriptLoaderScope,
                                             TRACE_ID_LOCAL(request_id_)),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
}

void ServiceWorkerNewScriptLoader::OnReceiveResponse(
    network::mojom::URLResponseHeadPtr response_head,
    mojo::ScopedDataPipeConsumerHandle body,
    std::optional<mojo_base::BigBuffer> cached_metadata) {
  TRACE_EVENT_WITH_FLOW0("ServiceWorker",
                         "ServiceWorkerNewScriptLoader::OnReceiveResponse",
                         TRACE_ID_WITH_SCOPE(kServiceWorkerNewScriptLoaderScope,
                                             TRACE_ID_LOCAL(request_id_)),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  CHECK_EQ(LoaderState::kLoadingHeader, network_loader_state_);
  if (!version_->context() || version_->is_redundant()) {
    CommitCompleted(network::URLLoaderCompletionStatus(net::ERR_FAILED),
                    ServiceWorkerConsts::kServiceWorkerInvalidVersionError,
                    std::move(response_head));
    return;
  }

  blink::ServiceWorkerStatusCode service_worker_state =
      blink::ServiceWorkerStatusCode::kOk;
  network::URLLoaderCompletionStatus completion_status;
  std::string error_message;
  if (!service_worker_loader_helpers::CheckResponseHead(
          *response_head, &service_worker_state, &completion_status,
          &error_message)) {
    CHECK_NE(net::OK, completion_status.error_code);
    CommitCompleted(completion_status, error_message, std::move(response_head));
    return;
  }

  if (is_main_script_) {
    // Check the path restriction defined in the spec:
    // https://w3c.github.io/ServiceWorker/#service-worker-script-response
    std::string service_worker_allowed;
    bool has_header = response_head->headers->EnumerateHeader(
        nullptr, ServiceWorkerConsts::kServiceWorkerAllowed,
        &service_worker_allowed);
    if (!service_worker_loader_helpers::IsPathRestrictionSatisfied(
            version_->scope(), request_url_,
            has_header ? &service_worker_allowed : nullptr, &error_message)) {
      CommitCompleted(
          network::URLLoaderCompletionStatus(net::ERR_INSECURE_RESPONSE),
          error_message, std::move(response_head));
      return;
    }

    if (!GetContentClient()
             ->browser()
             ->ShouldServiceWorkerInheritPolicyContainerFromCreator(
                 request_url_)) {
      version_->set_policy_container_host(
          base::MakeRefCounted<PolicyContainerHost>(
              // TODO(crbug.com/40235036): Add DCHECK to parsed_headers
              response_head->parsed_headers
                  // This does not parse the referrer policy, which will be
                  // updated in ServiceWorkerGlobalScope::Initialize
                  ? PolicyContainerPolicies(request_url_, response_head.get(),
                                            /*client=*/nullptr)
                  : PolicyContainerPolicies()));
    }

    if (response_head->network_accessed)
      version_->embedded_worker()->OnNetworkAccessedForScriptLoad();

    version_->SetMainScriptResponse(
        std::make_unique<ServiceWorkerVersion::MainScriptResponse>(
            *response_head));
  }

  WriteHeaders(response_head.Clone());

  // WriteHeaders() can commit completed.
  if (network_loader_state_ == LoaderState::kCompleted &&
      header_writer_state_ == WriterState::kCompleted &&
      body_writer_state_ == WriterState::kCompleted) {
    return;
  }

  // Don't pass SSLInfo to the client when the original request doesn't ask
  // to send it.
  if (response_head->ssl_info.has_value() &&
      !(original_options_ &
        network::mojom::kURLLoadOptionSendSSLInfoWithResponse)) {
    response_head->ssl_info.reset();
  }

  if (!body) {
    client_->OnReceiveResponse(std::move(response_head),
                               mojo::ScopedDataPipeConsumerHandle(),
                               std::move(cached_metadata));
    return;
  }

  // Create a pair of the consumer and producer for responding to the client.
  mojo::ScopedDataPipeConsumerHandle client_consumer;
  if (mojo::CreateDataPipe(nullptr, client_producer_, client_consumer) !=
      MOJO_RESULT_OK) {
    CommitCompleted(network::URLLoaderCompletionStatus(net::ERR_FAILED),
                    ServiceWorkerConsts::kServiceWorkerFetchScriptError,
                    std::move(response_head));
    return;
  }

  // Pass the consumer handle for responding with the response to the client.
  client_->OnReceiveResponse(std::move(response_head),
                             std::move(client_consumer),
                             std::move(cached_metadata));

  client_producer_watcher_.Watch(
      client_producer_.get(), MOJO_HANDLE_SIGNAL_WRITABLE,
      base::BindRepeating(&ServiceWorkerNewScriptLoader::OnClientWritable,
                          weak_factory_.GetWeakPtr()));

  network_consumer_ = std::move(body);
  network_loader_state_ = LoaderState::kLoadingBody;
  MaybeStartNetworkConsumerHandleWatcher();
}

void ServiceWorkerNewScriptLoader::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr response_head) {
  TRACE_EVENT_WITH_FLOW0("ServiceWorker",
                         "ServiceWorkerNewScriptLoader::OnReceiveRedirect",
                         TRACE_ID_WITH_SCOPE(kServiceWorkerNewScriptLoaderScope,
                                             TRACE_ID_LOCAL(request_id_)),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  // Resource requests for service worker scripts should not follow redirects.
  //
  // Step 9.5: "Set request's redirect mode to "error"."
  // https://w3c.github.io/ServiceWorker/#update-algorithm
  //
  // TODO(crbug.com/40595655): Follow redirects for imported scripts.
  CommitCompleted(network::URLLoaderCompletionStatus(net::ERR_UNSAFE_REDIRECT),
                  ServiceWorkerConsts::kServiceWorkerRedirectError,
                  std::move(response_head));
}

void ServiceWorkerNewScriptLoader::OnUploadProgress(
    int64_t current_position,
    int64_t total_size,
    OnUploadProgressCallback ack_callback) {
  TRACE_EVENT_WITH_FLOW0("ServiceWorker",
                         "ServiceWorkerNewScriptLoader::OnUploadProgress",
                         TRACE_ID_WITH_SCOPE(kServiceWorkerNewScriptLoaderScope,
                                             TRACE_ID_LOCAL(request_id_)),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  client_->OnUploadProgress(current_position, total_size,
                            std::move(ack_callback));
}

void ServiceWorkerNewScriptLoader::OnTransferSizeUpdated(
    int32_t transfer_size_diff) {
  TRACE_EVENT_WITH_FLOW0("ServiceWorker",
                         "ServiceWorkerNewScriptLoader::OnTransferSizeUpdated",
                         TRACE_ID_WITH_SCOPE(kServiceWorkerNewScriptLoaderScope,
                                             TRACE_ID_LOCAL(request_id_)),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  client_->OnTransferSizeUpdated(transfer_size_diff);
}

void ServiceWorkerNewScriptLoader::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  TRACE_EVENT_WITH_FLOW0("ServiceWorker",
                         "ServiceWorkerNewScriptLoader::OnComplete",
                         TRACE_ID_WITH_SCOPE(kServiceWorkerNewScriptLoaderScope,
                                             TRACE_ID_LOCAL(request_id_)),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  LoaderState previous_state = network_loader_state_;
  network_loader_state_ = LoaderState::kCompleted;
  if (status.error_code != net::OK) {
    CommitCompleted(status, ServiceWorkerConsts::kServiceWorkerFetchScriptError,
                    nullptr);
    return;
  }

  CHECK_EQ(LoaderState::kLoadingBody, previous_state);

  switch (body_writer_state_) {
    case WriterState::kNotStarted:
      // The header is still being written. Wait until both the header and body
      // are written. OnNetworkDataAvailable() will call CommitCompleted() after
      // all data from |network_consumer_| is consumed.
      CHECK_EQ(WriterState::kWriting, header_writer_state_);
      return;
    case WriterState::kWriting:
      // Wait until it's written. OnNetworkDataAvailable() will call
      // CommitCompleted() after all data from |network_consumer_| is
      // consumed.
      CHECK_EQ(WriterState::kCompleted, header_writer_state_);
      return;
    case WriterState::kCompleted:
      CHECK_EQ(WriterState::kCompleted, header_writer_state_);
      CommitCompleted(network::URLLoaderCompletionStatus(net::OK),
                      std::string() /* status_message */, nullptr);
      return;
  }
  CHECK(false) << static_cast<int>(body_writer_state_);  // NOTREACHED
}

// End of URLLoaderClient ------------------------------------------------------

void ServiceWorkerNewScriptLoader::WriteHeaders(
    network::mojom::URLResponseHeadPtr response_head) {
  TRACE_EVENT_WITH_FLOW0("ServiceWorker",
                         "ServiceWorkerNewScriptLoader::WriteHeaders",
                         TRACE_ID_WITH_SCOPE(kServiceWorkerNewScriptLoaderScope,
                                             TRACE_ID_LOCAL(request_id_)),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  CHECK_EQ(WriterState::kNotStarted, header_writer_state_);
  header_writer_state_ = WriterState::kWriting;
  net::Error error = cache_writer_->MaybeWriteHeaders(
      std::move(response_head),
      base::BindOnce(&ServiceWorkerNewScriptLoader::OnWriteHeadersComplete,
                     weak_factory_.GetWeakPtr()));
  if (error == net::ERR_IO_PENDING) {
    // OnWriteHeadersComplete() will be called asynchronously.
    return;
  }
  // MaybeWriteHeaders() doesn't run the callback if it finishes synchronously,
  // so explicitly call it here.
  OnWriteHeadersComplete(error);
}

void ServiceWorkerNewScriptLoader::OnWriteHeadersComplete(net::Error error) {
  TRACE_EVENT_WITH_FLOW0("ServiceWorker",
                         "ServiceWorkerNewScriptLoader::OnWriteHeadersComplete",
                         TRACE_ID_WITH_SCOPE(kServiceWorkerNewScriptLoaderScope,
                                             TRACE_ID_LOCAL(request_id_)),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  CHECK_EQ(WriterState::kWriting, header_writer_state_);
  CHECK_NE(net::ERR_IO_PENDING, error);
  if (error != net::OK) {
    ServiceWorkerMetrics::CountWriteResponseResult(
        ServiceWorkerMetrics::WRITE_HEADERS_ERROR);
    CommitCompleted(network::URLLoaderCompletionStatus(error),
                    ServiceWorkerConsts::kDatabaseErrorMessage, nullptr);
    return;
  }
  header_writer_state_ = WriterState::kCompleted;

  // If all other states are kCompleted the response body is empty, we can
  // finish now.
  if (network_loader_state_ == LoaderState::kCompleted &&
      body_writer_state_ == WriterState::kCompleted) {
    CommitCompleted(network::URLLoaderCompletionStatus(net::OK),
                    std::string() /* status_message */, nullptr);
    return;
  }

  MaybeStartNetworkConsumerHandleWatcher();
}

void ServiceWorkerNewScriptLoader::MaybeStartNetworkConsumerHandleWatcher() {
  TRACE_EVENT_WITH_FLOW0("ServiceWorker",
                         "ServiceWorkerNewScriptLoader::"
                         "MaybeStartNetworkConsumerHandleWatcher",
                         TRACE_ID_WITH_SCOPE(kServiceWorkerNewScriptLoaderScope,
                                             TRACE_ID_LOCAL(request_id_)),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  if (network_loader_state_ == LoaderState::kLoadingHeader) {
    // OnReceiveResponse() or OnComplete() will continue the sequence.
    return;
  }

  if (header_writer_state_ != WriterState::kCompleted) {
    CHECK_EQ(WriterState::kWriting, header_writer_state_);
    // OnWriteHeadersComplete() will continue the sequence.
    return;
  }

  if (body_writer_state_ != WriterState::kNotStarted) {
    static bool has_dumped_without_crashing = false;
    if (!has_dumped_without_crashing) {
      has_dumped_without_crashing = true;
      SCOPED_CRASH_KEY_NUMBER("SWNewScriptLoader", "network_loader_state",
                              static_cast<int>(network_loader_state_));
      SCOPED_CRASH_KEY_NUMBER("SWNewScriptLoader", "header_writer_state",
                              static_cast<int>(header_writer_state_));
      SCOPED_CRASH_KEY_NUMBER("SWNewScriptLoader", "body_writer_state",
                              static_cast<int>(body_writer_state_));
      base::debug::DumpWithoutCrashing();
    }
    return;
  }
  body_writer_state_ = WriterState::kWriting;

  network_watcher_.Watch(
      network_consumer_.get(),
      MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      base::BindRepeating(&ServiceWorkerNewScriptLoader::OnNetworkDataAvailable,
                          weak_factory_.GetWeakPtr()));
  network_watcher_.ArmOrNotify();
}

void ServiceWorkerNewScriptLoader::OnNetworkDataAvailable(MojoResult) {
  CHECK_EQ(WriterState::kCompleted, header_writer_state_);
  CHECK_EQ(WriterState::kWriting, body_writer_state_);
  CHECK(network_consumer_.is_valid());
  scoped_refptr<network::MojoToNetPendingBuffer> pending_buffer;
  MojoResult result = network::MojoToNetPendingBuffer::BeginRead(
      &network_consumer_, &pending_buffer);
  TRACE_EVENT_WITH_FLOW1("ServiceWorker",
                         "ServiceWorkerNewScriptLoader::OnNetworkDataAvailable",
                         TRACE_ID_WITH_SCOPE(kServiceWorkerNewScriptLoaderScope,
                                             TRACE_ID_LOCAL(request_id_)),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                         "begin_read_result", result);
  switch (result) {
    case MOJO_RESULT_OK: {
      const uint32_t bytes_available = pending_buffer->size();
      WriteData(std::move(pending_buffer), bytes_available);
      return;
    }
    case MOJO_RESULT_FAILED_PRECONDITION: {
      // Call WriteData() with null buffer to let the cache writer know that
      // body from the network reaches to the end.
      WriteData(/*pending_buffer=*/nullptr, /*bytes_available=*/0);
      return;
    }
    case MOJO_RESULT_SHOULD_WAIT: {
      network_watcher_.ArmOrNotify();
      return;
    }
  }
  CHECK(false) << static_cast<int>(result);  // NOTREACHED
}

void ServiceWorkerNewScriptLoader::WriteData(
    scoped_refptr<network::MojoToNetPendingBuffer> pending_buffer,
    uint32_t bytes_available) {
  auto buffer = base::MakeRefCounted<WrappedIOBuffer>(
      pending_buffer ? pending_buffer->buffer() : nullptr,
      pending_buffer ? pending_buffer->size() : 0);

  // Cap the buffer size up to |kReadBufferSize|. The remaining will be written
  // next time.
  base::span<const uint8_t> bytes = buffer->span();
  bytes = bytes.first(std::min<size_t>(kReadBufferSize, bytes_available));

  size_t bytes_written = 0;
  MojoResult result = client_producer_->WriteData(
      bytes, MOJO_WRITE_DATA_FLAG_NONE, bytes_written);
  TRACE_EVENT_WITH_FLOW1("ServiceWorker",
                         "ServiceWorkerNewScriptLoader::WriteData",
                         TRACE_ID_WITH_SCOPE(kServiceWorkerNewScriptLoaderScope,
                                             TRACE_ID_LOCAL(request_id_)),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                         "write_data_result", result);
  switch (result) {
    case MOJO_RESULT_OK:
      break;
    case MOJO_RESULT_FAILED_PRECONDITION:
      ServiceWorkerMetrics::CountWriteResponseResult(
          ServiceWorkerMetrics::WRITE_DATA_ERROR);
      CommitCompleted(network::URLLoaderCompletionStatus(net::ERR_FAILED),
                      ServiceWorkerConsts::kServiceWorkerFetchScriptError,
                      nullptr);
      return;
    case MOJO_RESULT_SHOULD_WAIT:
      DCHECK(pending_buffer);
      DCHECK(!pending_network_buffer_);
      DCHECK_EQ(pending_network_bytes_available_, 0u);
      // No data was written to `client_producer_` because the pipe was full.
      // Retry when the pipe becomes ready again.
      pending_network_buffer_ = std::move(pending_buffer);
      pending_network_bytes_available_ = bytes_available;
      client_producer_watcher_.ArmOrNotify();
      return;
    default:
      CHECK(false) << static_cast<int>(result);  // NOTREACHED
      return;
  }

  // Write the buffer in the service worker script storage up to the size we
  // successfully wrote to the data pipe (i.e., |bytes_written|).
  // A null buffer and zero |bytes_written| are passed when this is the end of
  // the body.
  net::Error error = cache_writer_->MaybeWriteData(
      buffer.get(), bytes_written,
      base::BindOnce(&ServiceWorkerNewScriptLoader::OnWriteDataComplete,
                     weak_factory_.GetWeakPtr(), pending_buffer,
                     bytes_written));
  if (error == net::ERR_IO_PENDING) {
    // OnWriteDataComplete() will be called asynchronously.
    return;
  }
  // MaybeWriteData() doesn't run the callback if it finishes synchronously, so
  // explicitly call it here.
  OnWriteDataComplete(std::move(pending_buffer), bytes_written, error);
}

void ServiceWorkerNewScriptLoader::OnWriteDataComplete(
    scoped_refptr<network::MojoToNetPendingBuffer> pending_buffer,
    size_t bytes_written,
    net::Error error) {
  TRACE_EVENT_WITH_FLOW0("ServiceWorker",
                         "ServiceWorkerNewScriptLoader::OnWriteDataComplete",
                         TRACE_ID_WITH_SCOPE(kServiceWorkerNewScriptLoaderScope,
                                             TRACE_ID_LOCAL(request_id_)),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  CHECK_NE(net::ERR_IO_PENDING, error);
  if (error != net::OK) {
    ServiceWorkerMetrics::CountWriteResponseResult(
        ServiceWorkerMetrics::WRITE_DATA_ERROR);
    CommitCompleted(network::URLLoaderCompletionStatus(error),
                    ServiceWorkerConsts::kDatabaseErrorMessage, nullptr);
    return;
  }
  ServiceWorkerMetrics::CountWriteResponseResult(
      ServiceWorkerMetrics::WRITE_OK);

  if (bytes_written == 0) {
    // Zero |bytes_written| with net::OK means that all data has been read from
    // the network and the Mojo data pipe has been closed. Thus we can complete
    // the request if OnComplete() has already been received.
    CHECK(!pending_buffer);
    body_writer_state_ = WriterState::kCompleted;
    if (network_loader_state_ == LoaderState::kCompleted) {
      CommitCompleted(network::URLLoaderCompletionStatus(net::OK),
                      std::string() /* status_message */, nullptr);
    }
    return;
  }

  CHECK(pending_buffer);
  pending_buffer->CompleteRead(bytes_written);
  // Get the consumer handle from a previous read operation if we have one.
  network_consumer_ = pending_buffer->ReleaseHandle();
  network_watcher_.ArmOrNotify();
}

void ServiceWorkerNewScriptLoader::CommitCompleted(
    const network::URLLoaderCompletionStatus& status,
    const std::string& status_message,
    const network::mojom::URLResponseHeadPtr response_head) {
  TRACE_EVENT_WITH_FLOW0("ServiceWorker",
                         "ServiceWorkerNewScriptLoader::CommitCompleted",
                         TRACE_ID_WITH_SCOPE(kServiceWorkerNewScriptLoaderScope,
                                             TRACE_ID_LOCAL(request_id_)),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  net::Error error_code = static_cast<net::Error>(status.error_code);
  int bytes_written = -1;
  std::string sha256_checksum;
  if (error_code == net::OK) {
    CHECK_EQ(LoaderState::kCompleted, network_loader_state_);
    CHECK_EQ(WriterState::kCompleted, header_writer_state_);
    CHECK_EQ(WriterState::kCompleted, body_writer_state_);
    CHECK(cache_writer_->did_replace());
    bytes_written = cache_writer_->bytes_written();
    DCHECK_EQ(cache_writer_->checksum_update_timing(),
              ServiceWorkerCacheWriter::ChecksumUpdateTiming::kCacheMismatch);
    sha256_checksum = cache_writer_->GetSha256Checksum();
  } else {
    // When we fail a main script fetch, we do not have a renderer in which to
    // log the failure. We call into devtools with the frame id instead.
    if (requesting_frame_id_ && version_->context()) {
      devtools_instrumentation::OnServiceWorkerMainScriptFetchingFailed(
          requesting_frame_id_, version_->context()->wrapper(),
          version_->version_id(), status_message, status, response_head.get(),
          request_url_);
    } else {
      // AddMessageConsole must be called before notifying that an error
      // occurred because the worker stops soon after receiving the error
      // response.
      // TODO(nhiroki): Consider replacing this hacky way with the new error
      // code handling mechanism in URLLoader.
      version_->AddMessageToConsole(blink::mojom::ConsoleMessageLevel::kError,
                                    status_message);
    }
  }
  version_->script_cache_map()->NotifyFinishedCaching(
      request_url_, bytes_written, sha256_checksum, error_code, status_message);

  client_->OnComplete(status);
  client_producer_.reset();
  client_producer_watcher_.Cancel();

  network_loader_.reset();
  network_consumer_.reset();
  network_watcher_.Cancel();
  cache_writer_.reset();
  network_loader_state_ = LoaderState::kCompleted;
  header_writer_state_ = WriterState::kCompleted;
  body_writer_state_ = WriterState::kCompleted;
}

void ServiceWorkerNewScriptLoader::OnClientWritable(MojoResult result) {
  TRACE_EVENT_WITH_FLOW1("ServiceWorker",
                         "ServiceWorkerNewScriptLoader::OnClientWritable",
                         TRACE_ID_WITH_SCOPE(kServiceWorkerNewScriptLoaderScope,
                                             TRACE_ID_LOCAL(request_id_)),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                         "mojo_result", result);
  DCHECK(pending_network_buffer_);
  DCHECK_GT(pending_network_bytes_available_, 0u);

  scoped_refptr<network::MojoToNetPendingBuffer> pending_buffer =
      std::move(pending_network_buffer_);
  uint32_t bytes_available = pending_network_bytes_available_;
  pending_network_bytes_available_ = 0;
  WriteData(std::move(pending_buffer), bytes_available);
}

}  // namespace content
