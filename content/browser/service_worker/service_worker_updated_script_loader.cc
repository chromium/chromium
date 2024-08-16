// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "content/browser/service_worker/service_worker_updated_script_loader.h"

#include <memory>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "content/browser/service_worker/service_worker_cache_writer.h"
#include "content/browser/service_worker/service_worker_consts.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_loader_helpers.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "net/base/ip_endpoint.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_status_flags.h"
#include "services/network/public/mojom/early_hints.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/loader/throttling_url_loader.h"

namespace content {

// We chose this size because the AppCache uses this.
const size_t ServiceWorkerUpdatedScriptLoader::kReadBufferSize = 32768;

// This is for debugging https://crbug.com/959627.
// The purpose is to see where the IOBuffer comes from by checking |__vfptr|.
class ServiceWorkerUpdatedScriptLoader::WrappedIOBuffer
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

std::unique_ptr<ServiceWorkerUpdatedScriptLoader>
ServiceWorkerUpdatedScriptLoader::CreateAndStart(
    uint32_t options,
    const network::ResourceRequest& original_request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    scoped_refptr<ServiceWorkerVersion> version) {
  return base::WrapUnique(new ServiceWorkerUpdatedScriptLoader(
      options, original_request, std::move(client), version));
}

ServiceWorkerUpdatedScriptLoader::ServiceWorkerUpdatedScriptLoader(
    uint32_t options,
    const network::ResourceRequest& original_request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    scoped_refptr<ServiceWorkerVersion> version)
    : request_url_(original_request.url),
      is_main_script_(original_request.destination ==
                          network::mojom::RequestDestination::kServiceWorker &&
                      original_request.mode ==
                          network::mojom::RequestMode::kSameOrigin),
      options_(options),
      version_(std::move(version)),
      network_watcher_(FROM_HERE,
                       mojo::SimpleWatcher::ArmingPolicy::MANUAL,
                       base::SequencedTaskRunner::GetCurrentDefault()),
      client_(std::move(client)),
      client_producer_watcher_(FROM_HERE,
                               mojo::SimpleWatcher::ArmingPolicy::MANUAL,
                               base::SequencedTaskRunner::GetCurrentDefault()),
      request_start_time_(base::TimeTicks::Now()) {
#if DCHECK_IS_ON()
  service_worker_loader_helpers::CheckVersionStatusBeforeWorkerScriptLoad(
      version_->status(), is_main_script_, version_->script_type());
#endif  // DCHECK_IS_ON()

  CHECK(client_);
  ServiceWorkerUpdateChecker::ComparedScriptInfo info =
      version_->TakeComparedScriptInfo(request_url_);
  if (info.result == ServiceWorkerSingleScriptUpdateChecker::Result::kFailed) {
    CHECK(!info.paused_state);
    // A network error received during update checking. This replays it.
    CommitCompleted(info.failure_info->network_status,
                    info.failure_info->error_message);
    return;
  }

  cache_writer_ = std::move(info.paused_state->cache_writer);
  CHECK(cache_writer_);

  network_loader_ = std::move(info.paused_state->network_loader);
  network_client_remote_ = std::move(info.paused_state->network_client_remote);
  pending_network_client_receiver_ =
      std::move(info.paused_state->network_client_receiver);

  network_loader_state_ = info.paused_state->network_loader_state;
  CHECK(network_loader_state_ == LoaderState::kLoadingBody ||
        network_loader_state_ == LoaderState::kCompleted);

  body_writer_state_ = info.paused_state->body_writer_state;
  CHECK(body_writer_state_ == WriterState::kWriting ||
        body_writer_state_ == WriterState::kCompleted);

  version_->script_cache_map()->NotifyStartedCaching(
      request_url_, cache_writer_->writer_resource_id());

  // Resume the cache writer and observe its writes, so all data written
  // is sent to |client_|.
  cache_writer_->set_write_observer(this);
  net::Error error = cache_writer_->Resume(base::BindOnce(
      &ServiceWorkerUpdatedScriptLoader::OnCacheWriterResumed,
      weak_factory_.GetWeakPtr(), info.paused_state->pending_network_buffer,
      info.paused_state->consumed_bytes));

  if (error != net::ERR_IO_PENDING) {
    OnCacheWriterResumed(info.paused_state->pending_network_buffer,
                         info.paused_state->consumed_bytes, error);
  }
}

ServiceWorkerUpdatedScriptLoader::~ServiceWorkerUpdatedScriptLoader() = default;

void ServiceWorkerUpdatedScriptLoader::FollowRedirect(
    const std::vector<std::string>& removed_headers,
    const net::HttpRequestHeaders& modified_headers,
    const net::HttpRequestHeaders& modified_cors_exempt_headers,
    const std::optional<GURL>& new_url) {
  // Resource requests for service worker scripts should not follow redirects.
  // See comments in OnReceiveRedirect().
  CHECK(false);  // NOTREACHED
}

void ServiceWorkerUpdatedScriptLoader::SetPriority(
    net::RequestPriority priority,
    int32_t intra_priority_value) {
  if (network_loader_)
    network_loader_->SetPriority(priority, intra_priority_value);
}

void ServiceWorkerUpdatedScriptLoader::PauseReadingBodyFromNet() {
  if (network_loader_)
    network_loader_->PauseReadingBodyFromNet();
}

void ServiceWorkerUpdatedScriptLoader::ResumeReadingBodyFromNet() {
  if (network_loader_)
    network_loader_->ResumeReadingBodyFromNet();
}

// URLLoaderClient for network loader ------------------------------------------

void ServiceWorkerUpdatedScriptLoader::OnReceiveEarlyHints(
    network::mojom::EarlyHintsPtr early_hints) {
  CHECK(false);  // NOTREACHED
}

void ServiceWorkerUpdatedScriptLoader::OnReceiveResponse(
    network::mojom::URLResponseHeadPtr response_head,
    mojo::ScopedDataPipeConsumerHandle body,
    std::optional<mojo_base::BigBuffer> cached_metadata) {
  CHECK(false);  // NOTREACHED
}

void ServiceWorkerUpdatedScriptLoader::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr response_head) {
  CHECK(false);  // NOTREACHED
}

void ServiceWorkerUpdatedScriptLoader::OnUploadProgress(
    int64_t current_position,
    int64_t total_size,
    OnUploadProgressCallback ack_callback) {
  CHECK(false);  // NOTREACHED
}

void ServiceWorkerUpdatedScriptLoader::OnTransferSizeUpdated(
    int32_t transfer_size_diff) {
  client_->OnTransferSizeUpdated(transfer_size_diff);
}

void ServiceWorkerUpdatedScriptLoader::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  LoaderState previous_state = network_loader_state_;
  network_loader_state_ = LoaderState::kCompleted;
  if (status.error_code != net::OK) {
    CommitCompleted(status,
                    ServiceWorkerConsts::kServiceWorkerFetchScriptError);
    return;
  }

  CHECK_EQ(LoaderState::kLoadingBody, previous_state);
  switch (body_writer_state_) {
    case WriterState::kNotStarted:
      CHECK(false) << "WriterState::kNotStarted";  // NOTREACHED
      return;
    case WriterState::kWriting:
      // Wait until it's written. OnNetworkDataAvailable() will call
      // CommitCompleted() after all data from |network_consumer_| is
      // consumed.
      return;
    case WriterState::kCompleted:
      CommitCompleted(network::URLLoaderCompletionStatus(net::OK),
                      std::string() /* status_message */);
      return;
  }
  CHECK(false) << static_cast<int>(body_writer_state_);  // NOTREACHED
}

// End of URLLoaderClient ------------------------------------------------------

int ServiceWorkerUpdatedScriptLoader::WillWriteResponseHead(
    const network::mojom::URLResponseHead& response_head) {
  auto client_response = response_head.Clone();
  client_response->request_start = request_start_time_;

  if (is_main_script_) {
    version_->SetMainScriptResponse(
        std::make_unique<ServiceWorkerVersion::MainScriptResponse>(
            *client_response));
  }

  // Don't pass SSLInfo to the client when the original request doesn't ask
  // to send it.
  if (client_response->ssl_info.has_value() &&
      !(options_ & network::mojom::kURLLoadOptionSendSSLInfoWithResponse)) {
    client_response->ssl_info.reset();
  }

  mojo::ScopedDataPipeConsumerHandle client_consumer;
  if (mojo::CreateDataPipe(nullptr, client_producer_, client_consumer) !=
      MOJO_RESULT_OK) {
    // Reports error to cache writer and finally the loader would process this
    // failure in OnCacheWriterResumed()
    return net::ERR_INSUFFICIENT_RESOURCES;
  }

  // Pass the consumer handle to the client.
  client_->OnReceiveResponse(std::move(client_response),
                             std::move(client_consumer), std::nullopt);

  client_producer_watcher_.Watch(
      client_producer_.get(), MOJO_HANDLE_SIGNAL_WRITABLE,
      base::BindRepeating(&ServiceWorkerUpdatedScriptLoader::OnClientWritable,
                          weak_factory_.GetWeakPtr()));
  return net::OK;
}

void ServiceWorkerUpdatedScriptLoader::OnClientWritable(MojoResult) {
  if (pending_network_buffer_) {
    DCHECK_GT(pending_network_bytes_available_, 0u);
    scoped_refptr<network::MojoToNetPendingBuffer> pending_buffer =
        std::move(pending_network_buffer_);
    uint32_t bytes_available = pending_network_bytes_available_;
    pending_network_bytes_available_ = 0;
    WriteData(std::move(pending_buffer), bytes_available);
    return;
  }

  CHECK(data_to_send_);
  CHECK_GE(data_length_, bytes_sent_to_client_);
  CHECK(client_producer_);

  // Cap the buffer size up to |kReadBufferSize|. The remaining will be written
  // next time.
  base::span<const uint8_t> bytes_to_send = data_to_send_->span();
  bytes_to_send = bytes_to_send.first(
      std::min(bytes_to_send.size(), base::checked_cast<size_t>(data_length_)));
  bytes_to_send = bytes_to_send.subspan(bytes_sent_to_client_);
  bytes_to_send = bytes_to_send.first(
      std::min<size_t>(bytes_to_send.size(), kReadBufferSize));

  size_t actually_sent_bytes = 0;
  MojoResult result = client_producer_->WriteData(
      bytes_to_send, MOJO_WRITE_DATA_FLAG_NONE, actually_sent_bytes);

  if (result == MOJO_RESULT_SHOULD_WAIT) {
    // No data was written to |client_producer_| because the pipe was full.
    // Retry when the pipe becomes ready again.
    client_producer_watcher_.ArmOrNotify();
    return;
  }

  if (result != MOJO_RESULT_OK) {
    ServiceWorkerMetrics::CountWriteResponseResult(
        ServiceWorkerMetrics::WRITE_DATA_ERROR);
    CommitCompleted(network::URLLoaderCompletionStatus(net::ERR_FAILED),
                    ServiceWorkerConsts::kServiceWorkerFetchScriptError);
    return;
  }

  bytes_sent_to_client_ += actually_sent_bytes;
  if (bytes_sent_to_client_ != data_length_) {
    // Not all data is sent. Send the rest in another task.
    client_producer_watcher_.ArmOrNotify();
    return;
  }
  std::move(write_observer_complete_callback_).Run(net::OK);
}

int ServiceWorkerUpdatedScriptLoader::WillWriteData(
    scoped_refptr<net::IOBuffer> data,
    int length,
    base::OnceCallback<void(net::Error)> callback) {
  CHECK(!write_observer_complete_callback_);
  CHECK(client_producer_);

  data_to_send_ = std::move(data);
  data_length_ = length;
  bytes_sent_to_client_ = 0;
  write_observer_complete_callback_ = std::move(callback);
  client_producer_watcher_.ArmOrNotify();
  return net::ERR_IO_PENDING;
}

void ServiceWorkerUpdatedScriptLoader::OnCacheWriterResumed(
    scoped_refptr<network::MojoToNetPendingBuffer> pending_network_buffer,
    uint32_t consumed_bytes,
    net::Error error) {
  CHECK_NE(error, net::ERR_IO_PENDING);
  // Stop observing write operations in cache writer as further data are
  // from network which would be processed by OnNetworkDataAvailable().
  cache_writer_->set_write_observer(nullptr);

  if (error != net::OK) {
    CommitCompleted(network::URLLoaderCompletionStatus(error),
                    ServiceWorkerConsts::kDatabaseErrorMessage);
    return;
  }
  // If the script has no body or all the body has already been read when it
  // was paused, we don't have to wait for more data from network.
  if (body_writer_state_ == WriterState::kCompleted) {
    CommitCompleted(network::URLLoaderCompletionStatus(net::OK), std::string());
    return;
  }

  // The data in the pending buffer has been processed during resuming. At this
  // point, this completes the pending read and releases the Mojo handle to
  // continue with reading the rest of the body.
  CHECK(pending_network_buffer);
  pending_network_buffer->CompleteRead(consumed_bytes);
  network_consumer_ = pending_network_buffer->ReleaseHandle();

  // Continue to load the rest of the body from the network.
  CHECK_EQ(body_writer_state_, WriterState::kWriting);
  CHECK(network_consumer_);
  network_client_receiver_.Bind(std::move(pending_network_client_receiver_));
  network_watcher_.Watch(
      network_consumer_.get(),
      MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      base::BindRepeating(
          &ServiceWorkerUpdatedScriptLoader::OnNetworkDataAvailable,
          weak_factory_.GetWeakPtr()));
  network_watcher_.ArmOrNotify();
}

void ServiceWorkerUpdatedScriptLoader::OnNetworkDataAvailable(MojoResult) {
  CHECK_EQ(WriterState::kWriting, body_writer_state_);
  CHECK(network_consumer_.is_valid());
  scoped_refptr<network::MojoToNetPendingBuffer> pending_buffer;
  MojoResult result = network::MojoToNetPendingBuffer::BeginRead(
      &network_consumer_, &pending_buffer);
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

void ServiceWorkerUpdatedScriptLoader::WriteData(
    scoped_refptr<network::MojoToNetPendingBuffer> pending_buffer,
    uint32_t bytes_available) {
  auto buffer = base::MakeRefCounted<WrappedIOBuffer>(
      pending_buffer ? pending_buffer->buffer() : nullptr,
      pending_buffer ? pending_buffer->size() : 0);

  // Cap the buffer size up to |kReadBufferSize|. The remaining will be written
  // next time.
  base::span<const uint8_t> bytes = buffer->span();
  bytes = bytes.first(std::min(kReadBufferSize, size_t{bytes_available}));

  size_t actually_written_bytes = 0;
  MojoResult result = client_producer_->WriteData(
      bytes, MOJO_WRITE_DATA_FLAG_NONE, actually_written_bytes);
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
  // successfully wrote to the data pipe (i.e., |actually_written_bytes|).  A
  // null buffer and zero |actually_written_bytes| are passed when this is the
  // end of the body.
  net::Error error = cache_writer_->MaybeWriteData(
      buffer.get(), actually_written_bytes,
      base::BindOnce(&ServiceWorkerUpdatedScriptLoader::OnWriteDataComplete,
                     weak_factory_.GetWeakPtr(), pending_buffer,
                     actually_written_bytes));
  if (error == net::ERR_IO_PENDING) {
    // OnWriteDataComplete() will be called asynchronously.
    return;
  }
  // MaybeWriteData() doesn't run the callback if it finishes synchronously, so
  // explicitly call it here.
  OnWriteDataComplete(std::move(pending_buffer), actually_written_bytes, error);
}

void ServiceWorkerUpdatedScriptLoader::OnWriteDataComplete(
    scoped_refptr<network::MojoToNetPendingBuffer> pending_buffer,
    size_t bytes_written,
    net::Error error) {
  CHECK_NE(net::ERR_IO_PENDING, error);
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
    CHECK(!pending_buffer);
    body_writer_state_ = WriterState::kCompleted;
    if (network_loader_state_ == LoaderState::kCompleted) {
      CommitCompleted(network::URLLoaderCompletionStatus(net::OK),
                      std::string() /* status_message */);
    }
    return;
  }

  CHECK(pending_buffer);
  pending_buffer->CompleteRead(base::checked_cast<uint32_t>(bytes_written));
  // Get the consumer handle from a previous read operation if we have one.
  network_consumer_ = pending_buffer->ReleaseHandle();
  network_watcher_.ArmOrNotify();
}

void ServiceWorkerUpdatedScriptLoader::CommitCompleted(
    const network::URLLoaderCompletionStatus& status,
    const std::string& status_message) {
  net::Error error_code = static_cast<net::Error>(status.error_code);
  int bytes_written = -1;
  std::string sha256_checksum;
  if (error_code == net::OK) {
    CHECK(cache_writer_);
    CHECK_EQ(LoaderState::kCompleted, network_loader_state_);
    CHECK_EQ(WriterState::kCompleted, body_writer_state_);
    // If all the calls to WriteHeaders/WriteData succeeded, but the incumbent
    // entry wasn't actually replaced because the new entry was equivalent, the
    // new version didn't actually install because it already exists.
    if (!cache_writer_->did_replace()) {
      version_->SetStartWorkerStatusCode(
          blink::ServiceWorkerStatusCode::kErrorExists);
      error_code = net::ERR_FILE_EXISTS;
    }
    bytes_written = cache_writer_->bytes_written();
    sha256_checksum = cache_writer_->GetSha256Checksum();
  } else {
    // AddMessageConsole must be called before notifying that an error occurred
    // because the worker stops soon after receiving the error response.
    // TODO(nhiroki): Consider replacing this hacky way with the new error code
    // handling mechanism in URLLoader.
    version_->AddMessageToConsole(blink::mojom::ConsoleMessageLevel::kError,
                                  status_message);
  }

  // Cache writer could be nullptr when update checking observed a network error
  // and this loader hasn't started the caching yet.
  if (cache_writer_) {
    version_->script_cache_map()->NotifyFinishedCaching(
        request_url_, bytes_written, sha256_checksum, error_code,
        status_message);
  }

  client_->OnComplete(status);
  client_producer_.reset();
  client_producer_watcher_.Cancel();

  network_loader_.reset();
  network_client_remote_.reset();
  network_client_receiver_.reset();
  network_consumer_.reset();
  network_watcher_.Cancel();
  cache_writer_.reset();
  network_loader_state_ = LoaderState::kCompleted;
  body_writer_state_ = WriterState::kCompleted;
}

}  // namespace content
