// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_single_script_update_checker.h"

#include "content/browser/appcache/appcache_response.h"
#include "content/browser/service_worker/service_worker_cache_writer.h"
#include "content/public/common/resource_type.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "services/network/public/cpp/net_adapters.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/blink/public/common/mime_util/mime_util.h"

// TODO(momohatt): Use ServiceWorkerMetrics for UMA.

namespace {

constexpr net::NetworkTrafficAnnotationTag kUpdateCheckTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("service_worker_update_checker",
                                        R"(
    semantics {
      sender: "ServiceWorker System"
      description:
        "This request is issued by an update check to fetch the content of "
        "the new scripts."
      trigger:
        "ServiceWorker's update logic, which is triggered by a navigation to a "
        "site controlled by a service worker."
      data:
        "No body. 'Service-Worker: script' header is attached when it's the "
        "main worker script. Requests may include cookies and credentials."
      destination: WEBSITE
    }
    policy {
      cookies_allowed: YES
      cookies_store: "user"
      setting:
        "Users can control this feature via the 'Cookies' setting under "
        "'Privacy, Content settings'. If cookies are disabled for a single "
        "site, serviceworkers are disabled for the site only. If they are "
        "totally disabled, all serviceworker requests will be stopped."
      chrome_policy {
        URLBlacklist {
          URLBlacklist: { entries: '*' }
        }
      }
      chrome_policy {
        URLWhitelist {
          URLWhitelist { }
        }
      }
    }
    comments:
      "Chrome would be unable to update service workers without this type of "
      "request. Using either URLBlacklist or URLWhitelist policies (or a "
      "combination of both) limits the scope of these requests."
    )");

}  // namespace

namespace content {

ServiceWorkerSingleScriptUpdateChecker::ServiceWorkerSingleScriptUpdateChecker(
    const GURL& url,
    bool is_main_script,
    scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
    std::unique_ptr<ServiceWorkerResponseReader> compare_reader,
    std::unique_ptr<ServiceWorkerResponseReader> copy_reader,
    std::unique_ptr<ServiceWorkerResponseWriter> writer,
    ResultCallback callback)
    : network_client_binding_(this),
      network_watcher_(FROM_HERE,
                       mojo::SimpleWatcher::ArmingPolicy::MANUAL,
                       base::SequencedTaskRunnerHandle::Get()),
      callback_(std::move(callback)),
      weak_factory_(this) {
  network::ResourceRequest resource_request;
  resource_request.url = url;
  resource_request.resource_type =
      is_main_script ? RESOURCE_TYPE_SERVICE_WORKER : RESOURCE_TYPE_SCRIPT;
  resource_request.do_not_prompt_for_login = true;
  if (is_main_script)
    resource_request.headers.SetHeader("Service-Worker", "script");

  // TODO(momohatt): Handle cases where force_bypass_cache is enabled.

  // |compare_reader| shouldn't be a nullptr, which forces
  // ServiceWorkerCacheWriter to do the comparison.
  DCHECK(compare_reader);
  cache_writer_ = std::make_unique<ServiceWorkerCacheWriter>(
      std::move(compare_reader), std::move(copy_reader), std::move(writer),
      true /* pause_when_not_identical */);

  network::mojom::URLLoaderClientPtr network_client;
  network_client_binding_.Bind(mojo::MakeRequest(&network_client));

  loader_factory->CreateLoaderAndStart(
      mojo::MakeRequest(&network_loader_), -1 /* routing_id */,
      -1 /* request_id */, network::mojom::kURLLoadOptionNone, resource_request,
      std::move(network_client),
      net::MutableNetworkTrafficAnnotationTag(kUpdateCheckTrafficAnnotation));
  DCHECK_EQ(NetworkLoaderState::kNotStarted, network_loader_state_);
  network_loader_state_ = NetworkLoaderState::kLoadingHeader;
}

ServiceWorkerSingleScriptUpdateChecker::
    ~ServiceWorkerSingleScriptUpdateChecker() = default;

// URLLoaderClient override ----------------------------------------------------

void ServiceWorkerSingleScriptUpdateChecker::OnReceiveResponse(
    const network::ResourceResponseHead& response_head) {
  DCHECK_EQ(NetworkLoaderState::kLoadingHeader, network_loader_state_);

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

  // TODO(momohatt): Check for header errors.

  network_loader_state_ = NetworkLoaderState::kWaitingForBody;

  WriteHeaders(
      base::MakeRefCounted<HttpResponseInfoIOBuffer>(std::move(response_info)));
}

void ServiceWorkerSingleScriptUpdateChecker::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    const network::ResourceResponseHead& response_head) {
  // TODO(momohatt): Raise error and terminate the update check here, like
  // ServiceWorkerNewScriptLoader does.
  NOTIMPLEMENTED();
}

void ServiceWorkerSingleScriptUpdateChecker::OnUploadProgress(
    int64_t current_position,
    int64_t total_size,
    OnUploadProgressCallback ack_callback) {
  // The network request for update checking shouldn't have upload data.
  NOTREACHED();
}

void ServiceWorkerSingleScriptUpdateChecker::OnReceiveCachedMetadata(
    const std::vector<uint8_t>& data) {}

void ServiceWorkerSingleScriptUpdateChecker::OnTransferSizeUpdated(
    int32_t transfer_size_diff) {
  NOTIMPLEMENTED();
}

void ServiceWorkerSingleScriptUpdateChecker::OnStartLoadingResponseBody(
    mojo::ScopedDataPipeConsumerHandle consumer) {
  DCHECK_EQ(NetworkLoaderState::kWaitingForBody, network_loader_state_);

  network_consumer_ = std::move(consumer);
  network_loader_state_ = NetworkLoaderState::kLoadingBody;
  MaybeStartNetworkConsumerHandleWatcher();
}

void ServiceWorkerSingleScriptUpdateChecker::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  NetworkLoaderState previous_loader_state = network_loader_state_;
  network_loader_state_ = NetworkLoaderState::kCompleted;
  if (status.error_code != net::OK) {
    Finish(false /* is_script_changed */);
    return;
  }

  DCHECK(previous_loader_state == NetworkLoaderState::kWaitingForBody ||
         previous_loader_state == NetworkLoaderState::kLoadingBody);

  // Response body is empty.
  if (previous_loader_state == NetworkLoaderState::kWaitingForBody) {
    DCHECK_EQ(CacheWriterState::kNotStarted, body_writer_state_);
    body_writer_state_ = CacheWriterState::kCompleted;
    switch (header_writer_state_) {
      case CacheWriterState::kNotStarted:
        NOTREACHED()
            << "Response header should be received before OnComplete()";
        break;
      case CacheWriterState::kWriting:
        // Wait until it's written. OnWriteHeadersComplete() will call
        // Finish().
        return;
      case CacheWriterState::kCompleted:
        DCHECK(!network_consumer_.is_valid());
        // Compare the cached data with an empty data to notify |cache_writer_|
        // of the end of the comparison.
        CompareData(nullptr /* pending_buffer */, 0 /* bytes_available */);
        break;
    }
  }

  // Response body exists.
  if (previous_loader_state == NetworkLoaderState::kLoadingBody) {
    switch (body_writer_state_) {
      case CacheWriterState::kNotStarted:
        DCHECK_EQ(CacheWriterState::kWriting, header_writer_state_);
        return;
      case CacheWriterState::kWriting:
        DCHECK_EQ(CacheWriterState::kCompleted, header_writer_state_);
        return;
      case CacheWriterState::kCompleted:
        DCHECK_EQ(CacheWriterState::kCompleted, header_writer_state_);
        Finish(false /* is_script_changed */);
        return;
    }
  }
}

//------------------------------------------------------------------------------

void ServiceWorkerSingleScriptUpdateChecker::WriteHeaders(
    scoped_refptr<HttpResponseInfoIOBuffer> info_buffer) {
  DCHECK_EQ(CacheWriterState::kNotStarted, header_writer_state_);
  header_writer_state_ = CacheWriterState::kWriting;

  // Pass the header to the cache_writer_. This is written to the storage when
  // the body had changes.
  net::Error error = cache_writer_->MaybeWriteHeaders(
      info_buffer.get(),
      base::BindOnce(
          &ServiceWorkerSingleScriptUpdateChecker::OnWriteHeadersComplete,
          weak_factory_.GetWeakPtr()));
  if (error == net::ERR_IO_PENDING) {
    // OnWriteHeadersComplete() will be called asynchronously.
    return;
  }
  // MaybeWriteHeaders() doesn't run the callback if it finishes synchronously,
  // so explicitly call it here.
  OnWriteHeadersComplete(error);
}

void ServiceWorkerSingleScriptUpdateChecker::OnWriteHeadersComplete(
    net::Error error) {
  DCHECK_EQ(CacheWriterState::kWriting, header_writer_state_);
  DCHECK_NE(net::ERR_IO_PENDING, error);
  header_writer_state_ = CacheWriterState::kCompleted;

  if (error != net::OK) {
    Finish(false /* is_script_changed */);
    return;
  }

  // Response body is empty.
  if (network_loader_state_ == NetworkLoaderState::kCompleted &&
      body_writer_state_ == CacheWriterState::kCompleted) {
    // Compare the cached data with an empty data to notify |cache_writer_|
    // the end of the comparison.
    CompareData(nullptr /* pending_buffer */, 0 /* bytes_available */);
    return;
  }

  MaybeStartNetworkConsumerHandleWatcher();
}

void ServiceWorkerSingleScriptUpdateChecker::
    MaybeStartNetworkConsumerHandleWatcher() {
  if (network_loader_state_ == NetworkLoaderState::kWaitingForBody) {
    // OnStartLoadingResponseBody() or OnComplete() will continue the sequence.
    return;
  }
  if (header_writer_state_ != CacheWriterState::kCompleted) {
    DCHECK_EQ(CacheWriterState::kWriting, header_writer_state_);
    // OnWriteHeadersComplete() will continue the sequence.
    return;
  }

  DCHECK_EQ(CacheWriterState::kNotStarted, body_writer_state_);
  body_writer_state_ = CacheWriterState::kWriting;

  network_watcher_.Watch(
      network_consumer_.get(),
      MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
      base::BindRepeating(
          &ServiceWorkerSingleScriptUpdateChecker::OnNetworkDataAvailable,
          weak_factory_.GetWeakPtr()));
  network_watcher_.ArmOrNotify();
}

void ServiceWorkerSingleScriptUpdateChecker::OnNetworkDataAvailable(
    MojoResult,
    const mojo::HandleSignalsState& state) {
  DCHECK_EQ(CacheWriterState::kCompleted, header_writer_state_);
  DCHECK(network_consumer_.is_valid());
  scoped_refptr<network::MojoToNetPendingBuffer> pending_buffer;
  uint32_t bytes_available = 0;
  MojoResult result = network::MojoToNetPendingBuffer::BeginRead(
      &network_consumer_, &pending_buffer, &bytes_available);
  switch (result) {
    case MOJO_RESULT_OK:
      CompareData(std::move(pending_buffer), bytes_available);
      return;
    case MOJO_RESULT_FAILED_PRECONDITION:
      // Closed by peer. This indicates all the data from the network service
      // are read or there is an error. In the error case, the reason is
      // notified via OnComplete().
      if (network_loader_state_ == NetworkLoaderState::kCompleted) {
        // Compare the cached data with an empty data to notify |cache_writer_|
        // the end of the comparison.
        CompareData(nullptr /* pending_buffer */, 0 /* bytes_available */);
      }
      return;
    case MOJO_RESULT_SHOULD_WAIT:
      network_watcher_.ArmOrNotify();
      return;
  }
  NOTREACHED() << static_cast<int>(result);
}

void ServiceWorkerSingleScriptUpdateChecker::CompareData(
    scoped_refptr<network::MojoToNetPendingBuffer> pending_buffer,
    uint32_t bytes_to_compare) {
  auto buffer = base::MakeRefCounted<net::WrappedIOBuffer>(
      pending_buffer ? pending_buffer->buffer() : nullptr);

  // Compare the network data and the stored data.
  net::Error error = cache_writer_->MaybeWriteData(
      buffer.get(), bytes_to_compare,
      base::BindOnce(
          &ServiceWorkerSingleScriptUpdateChecker::OnCompareDataComplete,
          weak_factory_.GetWeakPtr(),
          base::WrapRefCounted(pending_buffer.get()), bytes_to_compare));

  if (pending_buffer) {
    pending_buffer->CompleteRead(bytes_to_compare);
    network_consumer_ = pending_buffer->ReleaseHandle();
  }

  if (error == net::ERR_IO_PENDING && !cache_writer_->is_pausing()) {
    // OnCompareDataComplete() will be called asynchronously.
    return;
  }
  // MaybeWriteData() doesn't run the callback if it finishes synchronously, so
  // explicitly call it here.
  OnCompareDataComplete(std::move(pending_buffer), bytes_to_compare, error);
}

void ServiceWorkerSingleScriptUpdateChecker::OnCompareDataComplete(
    scoped_refptr<network::MojoToNetPendingBuffer> pending_buffer,
    uint32_t bytes_written,
    net::Error error) {
  if (cache_writer_->is_pausing()) {
    // |cache_writer_| can be pausing only when it finds difference between
    // stored body and network body.
    DCHECK_EQ(net::ERR_IO_PENDING, error);
    Finish(true /* is_script_changed */);
    return;
  }
  if (!pending_buffer || error != net::OK) {
    Finish(false /* is_script_changed */);
    return;
  }
  DCHECK(pending_buffer);
  network_watcher_.ArmOrNotify();
}

void ServiceWorkerSingleScriptUpdateChecker::Finish(bool is_script_changed) {
  if (is_script_changed) {
    // TODO(momohatt): pass the necessary information to the version to update.
  } else {
    network_loader_.reset();
    network_client_binding_.Close();
    network_consumer_.reset();
  }
  network_watcher_.Cancel();
  network_loader_state_ = NetworkLoaderState::kCompleted;
  header_writer_state_ = CacheWriterState::kCompleted;
  body_writer_state_ = CacheWriterState::kCompleted;

  std::move(callback_).Run(is_script_changed);
}

}  // namespace content
