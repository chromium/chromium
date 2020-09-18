// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_new_script_loader.h"

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "content/browser/appcache/appcache_disk_cache_ops.h"
#include "content/browser/service_worker/service_worker_cache_writer.h"
#include "content/browser/service_worker/service_worker_consts.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_loader_helpers.h"
#include "content/browser/service_worker/service_worker_storage.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/browser/url_loader_factory_getter.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "net/base/ip_endpoint.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_status_flags.h"
#include "net/http/http_response_info.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace content {

// We chose this size because the AppCache uses this.
const uint32_t ServiceWorkerNewScriptLoader::kReadBufferSize = 32768;

// This is for debugging https://crbug.com/959627.
// The purpose is to see where the IOBuffer comes from by checking |__vfptr|.
class ServiceWorkerNewScriptLoader::WrappedIOBuffer
    : public net::WrappedIOBuffer {
 public:
  WrappedIOBuffer(const char* data) : net::WrappedIOBuffer(data) {}

 private:
  ~WrappedIOBuffer() override = default;

  // This is to make sure that the vtable is not merged with other classes.
  virtual void dummy() { NOTREACHED(); }
};

std::unique_ptr<ServiceWorkerNewScriptLoader>
ServiceWorkerNewScriptLoader::CreateAndStart(
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& original_request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    scoped_refptr<ServiceWorkerVersion> version,
    scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    int64_t cache_resource_id) {
  return base::WrapUnique(new ServiceWorkerNewScriptLoader(
      routing_id, request_id, options, original_request, std::move(client),
      version, loader_factory, traffic_annotation, cache_resource_id));
}

// TODO(nhiroki): We're doing multiple things in the ctor. Consider factors out
// some of them into a separate function.
ServiceWorkerNewScriptLoader::ServiceWorkerNewScriptLoader(
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& original_request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    scoped_refptr<ServiceWorkerVersion> version,
    scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    int64_t cache_resource_id)
    : request_url_(original_request.url),
      resource_destination_(original_request.destination),
      original_options_(options),
      version_(version),
      network_watcher_(FROM_HERE,
                       mojo::SimpleWatcher::ArmingPolicy::MANUAL,
                       base::SequencedTaskRunnerHandle::Get()),
      loader_factory_(std::move(loader_factory)),
      client_(std::move(client)) {
  DCHECK_NE(cache_resource_id, blink::mojom::kInvalidServiceWorkerResourceId);

  network::ResourceRequest resource_request(original_request);
#if DCHECK_IS_ON()
  service_worker_loader_helpers::CheckVersionStatusBeforeWorkerScriptLoad(
      version_->status(), resource_destination_);
#endif  // DCHECK_IS_ON()

  scoped_refptr<ServiceWorkerRegistration> registration =
      version_->context()->GetLiveRegistration(version_->registration_id());
  // ServiceWorkerVersion keeps the registration alive while the service
  // worker is starting up, and it must be starting up here.
  DCHECK(registration);
  const bool is_main_script =
      (resource_destination_ ==
       network::mojom::RequestDestination::kServiceWorker);
  if (is_main_script) {
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
          is_main_script, version_->force_bypass_cache_for_scripts(),
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

  loader_factory_->CreateLoaderAndStart(
      network_loader_.BindNewPipeAndPassReceiver(), routing_id, request_id,
      options, resource_request,
      network_client_receiver_.BindNewPipeAndPassRemote(), traffic_annotation);
  DCHECK_EQ(LoaderState::kNotStarted, network_loader_state_);
  network_loader_state_ = LoaderState::kLoadingHeader;
}

ServiceWorkerNewScriptLoader::~ServiceWorkerNewScriptLoader() {
  // This class is used as a SelfOwnedReceiver and its lifetime is tied to the
  // corresponding mojo connection. There could be cases where the mojo
  // connection is disconnected while writing the response to the storage.
  // Complete this loader with ERR_FAILED in such cases to update the script
  // cache map.
  bool writers_completed = header_writer_state_ == WriterState::kCompleted &&
                           body_writer_state_ == WriterState::kCompleted;
  if (network_loader_state_ == LoaderState::kCompleted && !writers_completed) {
    DCHECK(client_);
    CommitCompleted(network::URLLoaderCompletionStatus(net::ERR_FAILED),
                    ServiceWorkerConsts::kServiceWorkerInvalidVersionError);
  }
}

void ServiceWorkerNewScriptLoader::FollowRedirect(
    const std::vector<std::string>& removed_headers,
    const net::HttpRequestHeaders& modified_headers,
    const net::HttpRequestHeaders& modified_cors_exempt_headers,
    const base::Optional<GURL>& new_url) {
  // Resource requests for service worker scripts should not follow redirects.
  // See comments in OnReceiveRedirect().
  NOTREACHED();
}

void ServiceWorkerNewScriptLoader::SetPriority(net::RequestPriority priority,
                                               int32_t intra_priority_value) {
  if (network_loader_)
    network_loader_->SetPriority(priority, intra_priority_value);
}

void ServiceWorkerNewScriptLoader::PauseReadingBodyFromNet() {
  if (network_loader_)
    network_loader_->PauseReadingBodyFromNet();
}

void ServiceWorkerNewScriptLoader::ResumeReadingBodyFromNet() {
  if (network_loader_)
    network_loader_->ResumeReadingBodyFromNet();
}

// URLLoaderClient for network loader ------------------------------------------

void ServiceWorkerNewScriptLoader::OnReceiveResponse(
    network::mojom::URLResponseHeadPtr response_head) {
  DCHECK_EQ(LoaderState::kLoadingHeader, network_loader_state_);
  if (!version_->context() || version_->is_redundant()) {
    CommitCompleted(network::URLLoaderCompletionStatus(net::ERR_FAILED),
                    ServiceWorkerConsts::kServiceWorkerInvalidVersionError);
    return;
  }

  blink::ServiceWorkerStatusCode service_worker_state =
      blink::ServiceWorkerStatusCode::kOk;
  network::URLLoaderCompletionStatus completion_status;
  std::string error_message;
  if (!service_worker_loader_helpers::CheckResponseHead(
          *response_head, &service_worker_state, &completion_status,
          &error_message)) {
    DCHECK_NE(net::OK, completion_status.error_code);
    CommitCompleted(completion_status, error_message);
    return;
  }

  if (resource_destination_ ==
      network::mojom::RequestDestination::kServiceWorker) {
    // Check the path restriction defined in the spec:
    // https://w3c.github.io/ServiceWorker/#service-worker-script-response
    std::string service_worker_allowed;
    bool has_header = response_head->headers->EnumerateHeader(
        nullptr, ServiceWorkerConsts::kServiceWorkerAllowed,
        &service_worker_allowed);
    if (!ServiceWorkerUtils::IsPathRestrictionSatisfied(
            version_->scope(), request_url_,
            has_header ? &service_worker_allowed : nullptr, &error_message)) {
      CommitCompleted(
          network::URLLoaderCompletionStatus(net::ERR_INSECURE_RESPONSE),
          error_message);
      return;
    }

    // TODO(arthursonzogni): Make the Cross-Origin-Embedder-Policy to be parsed
    // when it reached this line, not matter what URLLoader it is coming from.
    // The same mechanism as the one in NavigationURLLoader must be provided.
    // Instead of being a "document", the main resource here is a "script".
    version_->set_cross_origin_embedder_policy(
        response_head->parsed_headers
            ? response_head->parsed_headers->cross_origin_embedder_policy
            : network::CrossOriginEmbedderPolicy());

    if (response_head->network_accessed)
      version_->embedded_worker()->OnNetworkAccessedForScriptLoad();

    version_->SetMainScriptResponse(
        std::make_unique<ServiceWorkerVersion::MainScriptResponse>(
            *response_head));
  }

  network_loader_state_ = LoaderState::kWaitingForBody;

  WriteHeaders(response_head.Clone());

  // Don't pass SSLInfo to the client when the original request doesn't ask
  // to send it.
  if (response_head->ssl_info.has_value() &&
      !(original_options_ &
        network::mojom::kURLLoadOptionSendSSLInfoWithResponse)) {
    response_head->ssl_info.reset();
  }
  client_->OnReceiveResponse(std::move(response_head));
}

void ServiceWorkerNewScriptLoader::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr response_head) {
  // Resource requests for service worker scripts should not follow redirects.
  //
  // Step 9.5: "Set request's redirect mode to "error"."
  // https://w3c.github.io/ServiceWorker/#update-algorithm
  //
  // TODO(https://crbug.com/889798): Follow redirects for imported scripts.
  CommitCompleted(network::URLLoaderCompletionStatus(net::ERR_UNSAFE_REDIRECT),
                  ServiceWorkerConsts::kServiceWorkerRedirectError);
}

void ServiceWorkerNewScriptLoader::OnUploadProgress(
    int64_t current_position,
    int64_t total_size,
    OnUploadProgressCallback ack_callback) {
  client_->OnUploadProgress(current_position, total_size,
                            std::move(ack_callback));
}

void ServiceWorkerNewScriptLoader::OnReceiveCachedMetadata(
    mojo_base::BigBuffer data) {
  client_->OnReceiveCachedMetadata(std::move(data));
}

void ServiceWorkerNewScriptLoader::OnTransferSizeUpdated(
    int32_t transfer_size_diff) {
  client_->OnTransferSizeUpdated(transfer_size_diff);
}

void ServiceWorkerNewScriptLoader::OnStartLoadingResponseBody(
    mojo::ScopedDataPipeConsumerHandle consumer) {
  DCHECK_EQ(LoaderState::kWaitingForBody, network_loader_state_);
  // Create a pair of the consumer and producer for responding to the client.
  mojo::ScopedDataPipeConsumerHandle client_consumer;
  if (mojo::CreateDataPipe(nullptr, &client_producer_, &client_consumer) !=
      MOJO_RESULT_OK) {
    CommitCompleted(network::URLLoaderCompletionStatus(net::ERR_FAILED),
                    ServiceWorkerConsts::kServiceWorkerFetchScriptError);
    return;
  }

  // Pass the consumer handle for responding with the response to the client.
  client_->OnStartLoadingResponseBody(std::move(client_consumer));

  network_consumer_ = std::move(consumer);
  network_loader_state_ = LoaderState::kLoadingBody;
  MaybeStartNetworkConsumerHandleWatcher();
}

void ServiceWorkerNewScriptLoader::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  LoaderState previous_state = network_loader_state_;
  network_loader_state_ = LoaderState::kCompleted;
  if (status.error_code != net::OK) {
    CommitCompleted(status,
                    ServiceWorkerConsts::kServiceWorkerFetchScriptError);
    return;
  }

  DCHECK_EQ(LoaderState::kLoadingBody, previous_state);

  switch (body_writer_state_) {
    case WriterState::kNotStarted:
      // The header is still being written. Wait until both the header and body
      // are written. OnNetworkDataAvailable() will call CommitCompleted() after
      // all data from |network_consumer_| is consumed.
      DCHECK_EQ(WriterState::kWriting, header_writer_state_);
      return;
    case WriterState::kWriting:
      // Wait until it's written. OnNetworkDataAvailable() will call
      // CommitCompleted() after all data from |network_consumer_| is
      // consumed.
      DCHECK_EQ(WriterState::kCompleted, header_writer_state_);
      return;
    case WriterState::kCompleted:
      DCHECK_EQ(WriterState::kCompleted, header_writer_state_);
      CommitCompleted(network::URLLoaderCompletionStatus(net::OK),
                      std::string() /* status_message */);
      return;
  }
  NOTREACHED();
}

// End of URLLoaderClient ------------------------------------------------------

void ServiceWorkerNewScriptLoader::WriteHeaders(
    network::mojom::URLResponseHeadPtr response_head) {
  DCHECK_EQ(WriterState::kNotStarted, header_writer_state_);
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
  DCHECK_EQ(WriterState::kWriting, header_writer_state_);
  DCHECK_NE(net::ERR_IO_PENDING, error);
  if (error != net::OK) {
    ServiceWorkerMetrics::CountWriteResponseResult(
        ServiceWorkerMetrics::WRITE_HEADERS_ERROR);
    CommitCompleted(network::URLLoaderCompletionStatus(error),
                    ServiceWorkerConsts::kDatabaseErrorMessage);
    return;
  }
  header_writer_state_ = WriterState::kCompleted;

  // If all other states are kCompleted the response body is empty, we can
  // finish now.
  if (network_loader_state_ == LoaderState::kCompleted &&
      body_writer_state_ == WriterState::kCompleted) {
    CommitCompleted(network::URLLoaderCompletionStatus(net::OK),
                    std::string() /* status_message */);
    return;
  }

  MaybeStartNetworkConsumerHandleWatcher();
}

void ServiceWorkerNewScriptLoader::MaybeStartNetworkConsumerHandleWatcher() {
  if (network_loader_state_ == LoaderState::kWaitingForBody) {
    // OnStartLoadingResponseBody() or OnComplete() will continue the sequence.
    return;
  }
  if (header_writer_state_ != WriterState::kCompleted) {
    DCHECK_EQ(WriterState::kWriting, header_writer_state_);
    // OnWriteHeadersComplete() will continue the sequence.
    return;
  }

  DCHECK_EQ(WriterState::kNotStarted, body_writer_state_);
  body_writer_state_ = WriterState::kWriting;

  network_watcher_.Watch(
      network_consumer_.get(),
      MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      base::BindRepeating(&ServiceWorkerNewScriptLoader::OnNetworkDataAvailable,
                          weak_factory_.GetWeakPtr()));
  network_watcher_.ArmOrNotify();
}

void ServiceWorkerNewScriptLoader::OnNetworkDataAvailable(MojoResult) {
  DCHECK_EQ(WriterState::kCompleted, header_writer_state_);
  DCHECK_EQ(WriterState::kWriting, body_writer_state_);
  DCHECK(network_consumer_.is_valid());
  scoped_refptr<network::MojoToNetPendingBuffer> pending_buffer;
  uint32_t bytes_available = 0;
  MojoResult result = network::MojoToNetPendingBuffer::BeginRead(
      &network_consumer_, &pending_buffer, &bytes_available);
  switch (result) {
    case MOJO_RESULT_OK:
      WriteData(std::move(pending_buffer), bytes_available);
      return;
    case MOJO_RESULT_FAILED_PRECONDITION:
      // Call WriteData() with null buffer to let the cache writer know that
      // body from the network reaches to the end.
      WriteData(/*pending_buffer=*/nullptr, /*bytes_available=*/0);
      return;
    case MOJO_RESULT_SHOULD_WAIT:
      network_watcher_.ArmOrNotify();
      return;
  }
  NOTREACHED() << static_cast<int>(result);
}

void ServiceWorkerNewScriptLoader::WriteData(
    scoped_refptr<network::MojoToNetPendingBuffer> pending_buffer,
    uint32_t bytes_available) {
  // Cap the buffer size up to |kReadBufferSize|. The remaining will be written
  // next time.
  uint32_t bytes_written = std::min<uint32_t>(kReadBufferSize, bytes_available);

  auto buffer = base::MakeRefCounted<WrappedIOBuffer>(
      pending_buffer ? pending_buffer->buffer() : nullptr);
  MojoResult result = client_producer_->WriteData(
      buffer->data(), &bytes_written, MOJO_WRITE_DATA_FLAG_NONE);
  switch (result) {
    case MOJO_RESULT_OK:
      break;
    case MOJO_RESULT_FAILED_PRECONDITION:
      ServiceWorkerMetrics::CountWriteResponseResult(
          ServiceWorkerMetrics::WRITE_DATA_ERROR);
      CommitCompleted(network::URLLoaderCompletionStatus(net::ERR_FAILED),
                      ServiceWorkerConsts::kServiceWorkerFetchScriptError);
      return;
    case MOJO_RESULT_SHOULD_WAIT:
      // No data was written to |client_producer_| because the pipe was full.
      // Retry when the pipe becomes ready again.
      pending_buffer->CompleteRead(0);
      network_consumer_ = pending_buffer->ReleaseHandle();
      network_watcher_.ArmOrNotify();
      return;
    default:
      NOTREACHED() << static_cast<int>(result);
      return;
  }

  // Write the buffer in the service worker script storage up to the size we
  // successfully wrote to the data pipe (i.e., |bytes_written|).
  // A null buffer and zero |bytes_written| are passed when this is the end of
  // the body.
  net::Error error = cache_writer_->MaybeWriteData(
      buffer.get(), base::strict_cast<size_t>(bytes_written),
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
    uint32_t bytes_written,
    net::Error error) {
  DCHECK_NE(net::ERR_IO_PENDING, error);
  if (error != net::OK) {
    ServiceWorkerMetrics::CountWriteResponseResult(
        ServiceWorkerMetrics::WRITE_DATA_ERROR);
    CommitCompleted(network::URLLoaderCompletionStatus(error),
                    ServiceWorkerConsts::kDatabaseErrorMessage);
    return;
  }
  ServiceWorkerMetrics::CountWriteResponseResult(
      ServiceWorkerMetrics::WRITE_OK);

  if (bytes_written == 0) {
    // Zero |bytes_written| with net::OK means that all data has been read from
    // the network and the Mojo data pipe has been closed. Thus we can complete
    // the request if OnComplete() has already been received.
    DCHECK(!pending_buffer);
    body_writer_state_ = WriterState::kCompleted;
    if (network_loader_state_ == LoaderState::kCompleted) {
      CommitCompleted(network::URLLoaderCompletionStatus(net::OK),
                      std::string() /* status_message */);
    }
    return;
  }

  DCHECK(pending_buffer);
  pending_buffer->CompleteRead(bytes_written);
  // Get the consumer handle from a previous read operation if we have one.
  network_consumer_ = pending_buffer->ReleaseHandle();
  network_watcher_.ArmOrNotify();
}

void ServiceWorkerNewScriptLoader::CommitCompleted(
    const network::URLLoaderCompletionStatus& status,
    const std::string& status_message) {
  net::Error error_code = static_cast<net::Error>(status.error_code);
  int bytes_written = -1;
  if (error_code == net::OK) {
    DCHECK_EQ(LoaderState::kCompleted, network_loader_state_);
    DCHECK_EQ(WriterState::kCompleted, header_writer_state_);
    DCHECK_EQ(WriterState::kCompleted, body_writer_state_);
    DCHECK(cache_writer_->did_replace());
    bytes_written = cache_writer_->bytes_written();
  } else {
    // AddMessageConsole must be called before notifying that an error occurred
    // because the worker stops soon after receiving the error response.
    // TODO(nhiroki): Consider replacing this hacky way with the new error code
    // handling mechanism in URLLoader.
    version_->AddMessageToConsole(blink::mojom::ConsoleMessageLevel::kError,
                                  status_message);
  }
  version_->script_cache_map()->NotifyFinishedCaching(
      request_url_, bytes_written, error_code, status_message);

  client_->OnComplete(status);
  client_producer_.reset();

  network_loader_.reset();
  network_client_receiver_.reset();
  network_consumer_.reset();
  network_watcher_.Cancel();
  cache_writer_.reset();
  network_loader_state_ = LoaderState::kCompleted;
  header_writer_state_ = WriterState::kCompleted;
  body_writer_state_ = WriterState::kCompleted;
}

}  // namespace content
