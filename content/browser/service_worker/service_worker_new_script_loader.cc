// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_new_script_loader.h"

#include <memory>
#include "base/numerics/safe_conversions.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/browser/appcache/appcache_response.h"
#include "content/browser/service_worker/service_worker_cache_writer.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_disk_cache.h"
#include "content/browser/service_worker/service_worker_storage.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/browser/service_worker/service_worker_write_to_cache_job.h"
#include "content/browser/url_loader_factory_getter.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "net/base/load_flags.h"
#include "net/cert/cert_status_flags.h"
#include "services/network/public/cpp/resource_response.h"
#include "third_party/blink/public/common/mime_util/mime_util.h"

namespace content {

// We chose this size because the AppCache uses this.
const uint32_t ServiceWorkerNewScriptLoader::kReadBufferSize = 32768;

// TODO(nhiroki): We're doing multiple things in the ctor. Consider factors out
// some of them into a separate function.
ServiceWorkerNewScriptLoader::ServiceWorkerNewScriptLoader(
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& original_request,
    network::mojom::URLLoaderClientPtr client,
    scoped_refptr<ServiceWorkerVersion> version,
    scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
    : request_url_(original_request.url),
      resource_type_(static_cast<ResourceType>(original_request.resource_type)),
      version_(version),
      network_client_binding_(this),
      network_watcher_(FROM_HERE,
                       mojo::SimpleWatcher::ArmingPolicy::MANUAL,
                       base::SequencedTaskRunnerHandle::Get()),
      loader_factory_(std::move(loader_factory)),
      client_(std::move(client)),
      original_options_(options),
      weak_factory_(this) {
  network::ResourceRequest resource_request(original_request);

  // ServiceWorkerNewScriptLoader is used for fetching the service worker main
  // script (RESOURCE_TYPE_SERVICE_WORKER) during worker startup or
  // importScripts() (RESOURCE_TYPE_SCRIPT).
  // TODO(nhiroki): In the current implementation, importScripts() can be called
  // in any ServiceWorkerVersion::Status except for REDUNDANT, but the spec
  // defines importScripts() works only on the initial script evaluation and the
  // install event. Update this check once importScripts() is fixed.
  // (https://crbug.com/719052)
  DCHECK((resource_type_ == RESOURCE_TYPE_SERVICE_WORKER &&
          version->status() == ServiceWorkerVersion::NEW) ||
         (resource_type_ == RESOURCE_TYPE_SCRIPT &&
          version->status() != ServiceWorkerVersion::REDUNDANT));

  // TODO(nhiroki): Handle the case where |cache_resource_id| is invalid.
  int64_t cache_resource_id = version->context()->storage()->NewResourceId();

  // |incumbent_cache_resource_id| is valid if the incumbent service worker
  // exists and it's required to do the byte-for-byte check.
  int64_t incumbent_cache_resource_id = kInvalidServiceWorkerResourceId;
  scoped_refptr<ServiceWorkerRegistration> registration =
      version_->context()->GetLiveRegistration(version_->registration_id());
  // ServiceWorkerVersion keeps the registration alive while the service
  // worker is starting up, and it must be starting up here.
  DCHECK(registration);
  const bool is_main_script = resource_type_ == RESOURCE_TYPE_SERVICE_WORKER;
  if (is_main_script) {
    ServiceWorkerVersion* stored_version = registration->waiting_version()
                                               ? registration->waiting_version()
                                               : registration->active_version();
    // |pause_after_download()| indicates the version is required to do the
    // byte-for-byte check.
    if (stored_version && stored_version->script_url() == request_url_ &&
        version_->pause_after_download()) {
      incumbent_cache_resource_id =
          stored_version->script_cache_map()->LookupResourceId(request_url_);
    }
    // Request SSLInfo. It will be persisted in service worker storage and
    // may be used by ServiceWorkerNavigationLoader for navigations handled
    // by this service worker.
    options |= network::mojom::kURLLoadOptionSendSSLInfoWithResponse;

    resource_request.headers.SetHeader("Service-Worker", "script");
  }

  // Bypass the browser cache if needed, e.g., updateViaCache demands it or 24
  // hours passed since the last update check that hit network.
  base::TimeDelta time_since_last_check =
      base::Time::Now() - registration->last_update_check();
  if (ServiceWorkerUtils::ShouldBypassCacheDueToUpdateViaCache(
          is_main_script, registration->update_via_cache()) ||
      time_since_last_check > kServiceWorkerScriptMaxCacheAge ||
      version_->force_bypass_cache_for_scripts()) {
    resource_request.load_flags |= net::LOAD_BYPASS_CACHE;
  }

  // Create response readers only when we have to do the byte-for-byte check.
  std::unique_ptr<ServiceWorkerResponseReader> compare_reader;
  std::unique_ptr<ServiceWorkerResponseReader> copy_reader;
  ServiceWorkerStorage* storage = version_->context()->storage();
  if (incumbent_cache_resource_id != kInvalidServiceWorkerResourceId) {
    compare_reader = storage->CreateResponseReader(incumbent_cache_resource_id);
    copy_reader = storage->CreateResponseReader(incumbent_cache_resource_id);
  }
  cache_writer_ = std::make_unique<ServiceWorkerCacheWriter>(
      std::move(compare_reader), std::move(copy_reader),
      storage->CreateResponseWriter(cache_resource_id),
      false /* pause_when_not_identical */);

  version_->script_cache_map()->NotifyStartedCaching(request_url_,
                                                     cache_resource_id);

  // Disable MIME sniffing. The spec requires the header list to have a
  // JavaScript MIME type. Therefore, no sniffing is needed.
  options &= ~network::mojom::kURLLoadOptionSniffMimeType;

  network::mojom::URLLoaderClientPtr network_client;
  network_client_binding_.Bind(mojo::MakeRequest(&network_client));
  loader_factory_->CreateLoaderAndStart(
      mojo::MakeRequest(&network_loader_), routing_id, request_id, options,
      resource_request, std::move(network_client), traffic_annotation);
  DCHECK_EQ(NetworkLoaderState::kNotStarted, network_loader_state_);
  network_loader_state_ = NetworkLoaderState::kLoadingHeader;
}

ServiceWorkerNewScriptLoader::~ServiceWorkerNewScriptLoader() = default;

void ServiceWorkerNewScriptLoader::FollowRedirect(
    const base::Optional<std::vector<std::string>>&
        to_be_removed_request_headers,
    const base::Optional<net::HttpRequestHeaders>& modified_request_headers) {
  // Resource requests for service worker scripts should not follow redirects.
  // See comments in OnReceiveRedirect().
  NOTREACHED();
}

void ServiceWorkerNewScriptLoader::ProceedWithResponse() {
  NOTREACHED();
}

void ServiceWorkerNewScriptLoader::SetPriority(net::RequestPriority priority,
                                               int32_t intra_priority_value) {
  network_loader_->SetPriority(priority, intra_priority_value);
}

void ServiceWorkerNewScriptLoader::PauseReadingBodyFromNet() {
  network_loader_->PauseReadingBodyFromNet();
}

void ServiceWorkerNewScriptLoader::ResumeReadingBodyFromNet() {
  network_loader_->ResumeReadingBodyFromNet();
}

// URLLoaderClient for network loader ------------------------------------------

void ServiceWorkerNewScriptLoader::OnReceiveResponse(
    const network::ResourceResponseHead& response_head) {
  DCHECK_EQ(NetworkLoaderState::kLoadingHeader, network_loader_state_);
  if (!version_->context() || version_->is_redundant()) {
    CommitCompleted(network::URLLoaderCompletionStatus(net::ERR_FAILED),
                    kServiceWorkerFetchScriptError);
    return;
  }

  // We don't have complete info here, but fill in what we have now.
  // At least we need headers and SSL info.
  auto response_info = std::make_unique<net::HttpResponseInfo>();
  response_info->headers = response_head.headers;
  if (response_head.ssl_info.has_value())
    response_info->ssl_info = *response_head.ssl_info;
  response_info->was_fetched_via_spdy = response_head.was_fetched_via_spdy;
  response_info->was_alpn_negotiated = response_head.was_alpn_negotiated;
  response_info->alpn_negotiated_protocol =
      response_head.alpn_negotiated_protocol;
  response_info->connection_info = response_head.connection_info;
  response_info->socket_address = response_head.socket_address;

  // The following sequence is equivalent to
  // ServiceWorkerWriteToCacheJob::OnResponseStarted.
  // TODO(falken): Make these steps be in the same order as the spec. Right now
  // there are slight differences, like we only bump last update check time on
  // OK status.

  if (response_head.headers->response_code() / 100 != 2) {
    // Non-2XX HTTP status code is handled as an error.
    std::string error_message =
        base::StringPrintf(kServiceWorkerBadHTTPResponseError,
                           response_head.headers->response_code());
    CommitCompleted(
        network::URLLoaderCompletionStatus(net::ERR_INVALID_RESPONSE),
        error_message);
    return;
  }

  // Check the certificate error.
  if (net::IsCertStatusError(response_head.cert_status) &&
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kIgnoreCertificateErrors)) {
    CommitCompleted(
        network::URLLoaderCompletionStatus(
            net::MapCertStatusToNetError(response_head.cert_status)),
        kServiceWorkerSSLError);
    return;
  }

  if (resource_type_ == RESOURCE_TYPE_SERVICE_WORKER) {
    if (!blink::IsSupportedJavascriptMimeType(response_head.mime_type)) {
      std::string error_message =
          response_head.mime_type.empty()
              ? kServiceWorkerNoMIMEError
              : base::StringPrintf(kServiceWorkerBadMIMEError,
                                   response_head.mime_type.c_str());
      CommitCompleted(
          network::URLLoaderCompletionStatus(net::ERR_INSECURE_RESPONSE),
          error_message);
      return;
    }

    // Check the path restriction defined in the spec:
    // https://w3c.github.io/ServiceWorker/#service-worker-script-response
    std::string service_worker_allowed;
    bool has_header = response_head.headers->EnumerateHeader(
        nullptr, kServiceWorkerAllowed, &service_worker_allowed);
    std::string error_message;
    if (!ServiceWorkerUtils::IsPathRestrictionSatisfied(
            version_->scope(), request_url_,
            has_header ? &service_worker_allowed : nullptr, &error_message)) {
      CommitCompleted(
          network::URLLoaderCompletionStatus(net::ERR_INSECURE_RESPONSE),
          error_message);
      return;
    }

    if (response_head.network_accessed)
      version_->embedded_worker()->OnNetworkAccessedForScriptLoad();

    version_->SetMainScriptHttpResponseInfo(*response_info);
  }

  network_loader_state_ = NetworkLoaderState::kWaitingForBody;

  WriteHeaders(
      base::MakeRefCounted<HttpResponseInfoIOBuffer>(std::move(response_info)));

  // Don't pass SSLInfo to the client when the original request doesn't ask
  // to send it.
  if (response_head.ssl_info.has_value() &&
      !(original_options_ &
        network::mojom::kURLLoadOptionSendSSLInfoWithResponse)) {
    network::ResourceResponseHead new_response_head = response_head;
    new_response_head.ssl_info.reset();
    client_->OnReceiveResponse(new_response_head);
    return;
  }
  client_->OnReceiveResponse(response_head);
}

void ServiceWorkerNewScriptLoader::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    const network::ResourceResponseHead& response_head) {
  // Resource requests for service worker scripts should not follow redirects.
  //
  // Step 7.5: "Set request's redirect mode to "error"."
  // https://w3c.github.io/ServiceWorker/#update-algorithm
  CommitCompleted(network::URLLoaderCompletionStatus(net::ERR_UNSAFE_REDIRECT),
                  kServiceWorkerRedirectError);
}

void ServiceWorkerNewScriptLoader::OnUploadProgress(
    int64_t current_position,
    int64_t total_size,
    OnUploadProgressCallback ack_callback) {
  client_->OnUploadProgress(current_position, total_size,
                            std::move(ack_callback));
}

void ServiceWorkerNewScriptLoader::OnReceiveCachedMetadata(
    const std::vector<uint8_t>& data) {
  client_->OnReceiveCachedMetadata(data);
}

void ServiceWorkerNewScriptLoader::OnTransferSizeUpdated(
    int32_t transfer_size_diff) {
  client_->OnTransferSizeUpdated(transfer_size_diff);
}

void ServiceWorkerNewScriptLoader::OnStartLoadingResponseBody(
    mojo::ScopedDataPipeConsumerHandle consumer) {
  DCHECK_EQ(NetworkLoaderState::kWaitingForBody, network_loader_state_);
  // Create a pair of the consumer and producer for responding to the client.
  mojo::ScopedDataPipeConsumerHandle client_consumer;
  if (mojo::CreateDataPipe(nullptr, &client_producer_, &client_consumer) !=
      MOJO_RESULT_OK) {
    CommitCompleted(network::URLLoaderCompletionStatus(net::ERR_FAILED),
                    kServiceWorkerFetchScriptError);
    return;
  }

  // Pass the consumer handle for responding with the response to the client.
  client_->OnStartLoadingResponseBody(std::move(client_consumer));

  network_consumer_ = std::move(consumer);
  network_loader_state_ = NetworkLoaderState::kLoadingBody;
  MaybeStartNetworkConsumerHandleWatcher();
}

void ServiceWorkerNewScriptLoader::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  NetworkLoaderState previous_state = network_loader_state_;
  network_loader_state_ = NetworkLoaderState::kCompleted;
  if (status.error_code != net::OK) {
    CommitCompleted(status, kServiceWorkerFetchScriptError);
    return;
  }

  DCHECK(previous_state == NetworkLoaderState::kWaitingForBody ||
         previous_state == NetworkLoaderState::kLoadingBody);

  // Response body is empty.
  if (previous_state == NetworkLoaderState::kWaitingForBody) {
    DCHECK_EQ(WriterState::kNotStarted, body_writer_state_);
    body_writer_state_ = WriterState::kCompleted;
    switch (header_writer_state_) {
      case WriterState::kNotStarted:
        NOTREACHED()
            << "Response header should be received before OnComplete()";
        break;
      case WriterState::kWriting:
        // Wait until it's written. OnWriteHeadersComplete() will call
        // CommitCompleted().
        return;
      case WriterState::kCompleted:
        DCHECK(!network_consumer_.is_valid());
        CommitCompleted(network::URLLoaderCompletionStatus(net::OK),
                        std::string() /* status_message */);
        return;
    }
    NOTREACHED();
  }

  // Response body exists.
  if (previous_state == NetworkLoaderState::kLoadingBody) {
    switch (body_writer_state_) {
      case WriterState::kNotStarted:
        // Wait until it's written. OnNetworkDataAvailable() will call
        // CommitCompleted() after all data from |network_consumer_| is
        // consumed.
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

  NOTREACHED();
}

// End of URLLoaderClient ------------------------------------------------------
void ServiceWorkerNewScriptLoader::WriteHeaders(
    scoped_refptr<HttpResponseInfoIOBuffer> info_buffer) {
  DCHECK_EQ(WriterState::kNotStarted, header_writer_state_);
  header_writer_state_ = WriterState::kWriting;
  net::Error error = cache_writer_->MaybeWriteHeaders(
      info_buffer.get(),
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
                    kServiceWorkerFetchScriptError);
    return;
  }
  header_writer_state_ = WriterState::kCompleted;

  // If all other states are kCompleted the response body is empty, we can
  // finish now.
  if (network_loader_state_ == NetworkLoaderState::kCompleted &&
      body_writer_state_ == WriterState::kCompleted) {
    CommitCompleted(network::URLLoaderCompletionStatus(net::OK),
                    std::string() /* status_message */);
    return;
  }

  MaybeStartNetworkConsumerHandleWatcher();
}

void ServiceWorkerNewScriptLoader::MaybeStartNetworkConsumerHandleWatcher() {
  if (network_loader_state_ == NetworkLoaderState::kWaitingForBody) {
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
      base::Bind(&ServiceWorkerNewScriptLoader::OnNetworkDataAvailable,
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
      // Closed by peer. This indicates all the data from the network service
      // are read or there is an error. In the error case, the reason is
      // notified via OnComplete().
      body_writer_state_ = WriterState::kCompleted;
      if (network_loader_state_ == NetworkLoaderState::kCompleted) {
        CommitCompleted(network::URLLoaderCompletionStatus(net::OK),
                        std::string() /* status_message */);
      }
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

  auto buffer =
      base::MakeRefCounted<net::WrappedIOBuffer>(pending_buffer->buffer());
  MojoResult result = client_producer_->WriteData(
      buffer->data(), &bytes_written, MOJO_WRITE_DATA_FLAG_NONE);
  switch (result) {
    case MOJO_RESULT_OK:
      break;
    case MOJO_RESULT_FAILED_PRECONDITION:
      ServiceWorkerMetrics::CountWriteResponseResult(
          ServiceWorkerMetrics::WRITE_DATA_ERROR);
      CommitCompleted(network::URLLoaderCompletionStatus(net::ERR_FAILED),
                      kServiceWorkerFetchScriptError);
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
  net::Error error = cache_writer_->MaybeWriteData(
      buffer.get(), base::strict_cast<size_t>(bytes_written),
      base::BindOnce(&ServiceWorkerNewScriptLoader::OnWriteDataComplete,
                     weak_factory_.GetWeakPtr(),
                     base::WrapRefCounted(pending_buffer.get()),
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
                    kServiceWorkerFetchScriptError);
    return;
  }
  DCHECK(pending_buffer);
  ServiceWorkerMetrics::CountWriteResponseResult(
      ServiceWorkerMetrics::WRITE_OK);
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
    DCHECK_EQ(NetworkLoaderState::kCompleted, network_loader_state_);
    DCHECK_EQ(WriterState::kCompleted, header_writer_state_);
    DCHECK_EQ(WriterState::kCompleted, body_writer_state_);
    // If all the calls to WriteHeaders/WriteData succeeded, but the incumbent
    // entry wasn't actually replaced because the new entry was equivalent, the
    // new version didn't actually install because it already exists.
    if (!cache_writer_->did_replace()) {
      version_->SetStartWorkerStatusCode(
          blink::ServiceWorkerStatusCode::kErrorExists);
      error_code = ServiceWorkerWriteToCacheJob::kIdenticalScriptError;
    }
    bytes_written = cache_writer_->bytes_written();
  } else {
    // AddMessageConsole must be called before notifying that an error occurred
    // because the worker stops soon after receiving the error response.
    // TODO(nhiroki): Consider replacing this hacky way with the new error code
    // handling mechanism in URLLoader.
    version_->embedded_worker()->AddMessageToConsole(
        blink::WebConsoleMessage::kLevelError, status_message);
  }
  version_->script_cache_map()->NotifyFinishedCaching(
      request_url_, bytes_written, error_code, status_message);

  client_->OnComplete(status);
  client_producer_.reset();

  network_loader_.reset();
  network_client_binding_.Close();
  network_consumer_.reset();
  network_watcher_.Cancel();
  cache_writer_.reset();
  network_loader_state_ = NetworkLoaderState::kCompleted;
  header_writer_state_ = WriterState::kCompleted;
  body_writer_state_ = WriterState::kCompleted;
}

}  // namespace content
