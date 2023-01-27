// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/network_service_memory_cache_url_loader.h"

#include "base/bit_cast.h"
#include "base/task/sequenced_task_runner.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/trace_event.h"
#include "net/http/http_log_util.h"
#include "services/network/network_service_memory_cache.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/http_raw_headers.mojom.h"
#include "services/network/public/mojom/ip_address_space.mojom.h"

namespace network {

NetworkServiceMemoryCacheURLLoader::NetworkServiceMemoryCacheURLLoader(
    NetworkServiceMemoryCache* memory_cache,
    uint64_t trace_id,
    const ResourceRequest& resource_request,
    const net::NetLogWithSource& net_log,
    mojo::PendingReceiver<mojom::URLLoader> receiver,
    mojo::PendingRemote<mojom::URLLoaderClient> client,
    scoped_refptr<base::RefCountedBytes> content,
    int64_t encoded_body_length,
    const absl::optional<net::CookiePartitionKey> cookie_partition_key)
    : memory_cache_(memory_cache),
      trace_id_(trace_id),
      net_log_(net_log),
      receiver_(this, std::move(receiver)),
      client_(std::move(client)),
      devtools_request_id_(resource_request.devtools_request_id),
      content_(std::move(content)),
      encoded_body_length_(encoded_body_length),
      cookie_partition_key_(std::move(cookie_partition_key)) {
  DCHECK(memory_cache_);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(
      "loading", "NetworkServiceMemoryCacheURLLoader",
      TRACE_ID_LOCAL(trace_id_), "url", resource_request.url.spec());

  if (resource_request.trusted_params &&
      resource_request.trusted_params->devtools_observer) {
    devtools_observer_.Bind(
        std::move(const_cast<mojo::PendingRemote<mojom::DevToolsObserver>&>(
            resource_request.trusted_params->devtools_observer)));
  }

  client_.set_disconnect_handler(
      base::BindOnce(&NetworkServiceMemoryCacheURLLoader::OnClientDisconnected,
                     base::Unretained(this)));
}

NetworkServiceMemoryCacheURLLoader::~NetworkServiceMemoryCacheURLLoader() {
  TRACE_EVENT_NESTABLE_ASYNC_END0("loading",
                                  "NetworkServiceMemoryCacheURLLoader",
                                  TRACE_ID_LOCAL(trace_id_));
}

void NetworkServiceMemoryCacheURLLoader::Start(
    const ResourceRequest& resource_request,
    mojom::URLResponseHeadPtr response_head) {
  UpdateResponseHead(resource_request, response_head);

  MaybeNotifyRawResponse(*response_head);

  // Create a data pipe.
  MojoCreateDataPipeOptions options;
  options.struct_size = sizeof(MojoCreateDataPipeOptions);
  options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
  options.element_num_bytes = 1;
  options.capacity_num_bytes =
      memory_cache_->GetDataPipeCapacity(content_->size());
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  MojoResult result =
      mojo::CreateDataPipe(&options, producer_handle_, consumer_handle);
  if (result != MOJO_RESULT_OK) {
    Finish(net::ERR_FAILED);
    return;
  }

  net::NetLogRequestHeaders(
      net_log_, net::NetLogEventType::IN_MEMORY_CACHE_READ_REQUEST_HEADERS,
      /*request_line=*/"", &resource_request.headers);
  net::NetLogResponseHeaders(
      net_log_, net::NetLogEventType::IN_MEMORY_CACHE_READ_RESPONSE_HEADERS,
      response_head->headers.get());

  // Start sending the response.
  client_->OnReceiveResponse(std::move(response_head),
                             std::move(consumer_handle), absl::nullopt);

  // Set up data pipe producer.
  producer_handle_watcher_ = std::make_unique<mojo::SimpleWatcher>(
      FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL,
      base::SequencedTaskRunner::GetCurrentDefault());
  producer_handle_watcher_->Watch(
      producer_handle_.get(), MOJO_HANDLE_SIGNAL_WRITABLE,
      MOJO_WATCH_CONDITION_SATISFIED,
      base::BindRepeating(
          &NetworkServiceMemoryCacheURLLoader::OnProducerHandleReady,
          weak_ptr_factory_.GetWeakPtr()));

  WriteMore();
}

void NetworkServiceMemoryCacheURLLoader::UpdateResponseHead(
    const ResourceRequest& resource_request,
    mojom::URLResponseHeadPtr& response_head) {
  response_head->network_accessed = false;
  response_head->is_validated = false;
  response_head->was_fetched_via_cache = true;

  const base::Time now_time = base::Time::Now();
  const base::TimeTicks now_ticks = base::TimeTicks::Now();

  response_head->request_start = now_ticks;
  response_head->response_start = now_ticks;

  net::LoadTimingInfo load_timing;
  if (resource_request.enable_load_timing) {
    load_timing.request_start_time = now_time;
    load_timing.request_start = now_ticks;

    load_timing.send_start = now_ticks;
    load_timing.send_end = now_ticks;
    load_timing.receive_headers_start = now_ticks;
    load_timing.receive_headers_end = now_ticks;
  }
  response_head->load_timing = load_timing;
}

void NetworkServiceMemoryCacheURLLoader::MaybeNotifyRawResponse(
    const mojom::URLResponseHead& response_head) {
  if (!devtools_observer_ || !devtools_request_id_)
    return;

  std::vector<network::mojom::HttpRawHeaderPairPtr> header_array;
  size_t iter = 0;
  std::string name, value;
  while (response_head.headers->EnumerateHeaderLines(&iter, &name, &value)) {
    network::mojom::HttpRawHeaderPairPtr pair =
        network::mojom::HttpRawHeaderPair::New();
    pair->key = name;
    pair->value = value;
    header_array.push_back(std::move(pair));
  }

  devtools_observer_->OnRawResponse(
      *devtools_request_id_, /*cookies_with_access_result=*/{},
      std::move(header_array), /*raw_response_headers=*/absl::nullopt,
      mojom::IPAddressSpace::kUnknown, response_head.headers->response_code(),
      cookie_partition_key_);
}

void NetworkServiceMemoryCacheURLLoader::WriteMore() {
  size_t original_write_position = write_position_;
  size_t total_write_size = 0;
  bool write_completed = false;
  while (true) {
    DCHECK_GE(content_->size(), write_position_);
    DCHECK_GE(std::numeric_limits<uint32_t>::max(),
              content_->size() - write_position_);
    uint32_t write_size =
        static_cast<uint32_t>(content_->size() - write_position_);
    if (write_size == 0) {
      write_completed = true;
      break;
    }

    MojoResult result =
        producer_handle_->WriteData(content_->data().data() + write_position_,
                                    &write_size, MOJO_WRITE_DATA_FLAG_NONE);
    if (result == MOJO_RESULT_SHOULD_WAIT) {
      producer_handle_watcher_->ArmOrNotify();
      break;
    }

    if (result != MOJO_RESULT_OK) {
      Finish(net::ERR_FAILED);
      return;
    }

    write_position_ += write_size;
    total_write_size += write_size;
  }

  TRACE_EVENT_NESTABLE_ASYNC_INSTANT2(
      "loading", "NetworkServiceMemoryCacheURLLoader::WriteMore",
      TRACE_ID_LOCAL(trace_id_), "write_position", write_position_,
      "total_write_bytes", total_write_size);

  if (net_log_.IsCapturing()) {
    net_log_.AddByteTransferEvent(
        net::NetLogEventType::IN_MEMORY_CACHE_BYTES_READ, total_write_size,
        base::bit_cast<const char*>(content_->data().data() +
                                    original_write_position));
  }

  if (write_completed) {
    Finish(net::OK);
  }
}

void NetworkServiceMemoryCacheURLLoader::OnProducerHandleReady(
    MojoResult result,
    const mojo::HandleSignalsState& state) {
  if (result == MOJO_RESULT_OK) {
    WriteMore();
  } else {
    Finish(net::ERR_FAILED);
  }
}

void NetworkServiceMemoryCacheURLLoader::Finish(int error_code) {
  TRACE_EVENT_NESTABLE_ASYNC_INSTANT0(
      "loading", "NetworkServiceMemoryCacheURLLoader::Finish",
      TRACE_ID_LOCAL(trace_id_));

  producer_handle_.reset();
  producer_handle_watcher_.reset();

  if (error_code == net::OK) {
    DCHECK(client_.is_connected());

    URLLoaderCompletionStatus status;
    status.error_code = net::OK;
    status.exists_in_cache = true;
    status.exists_in_memory_cache = true;
    status.completion_time = base::TimeTicks::Now();
    status.encoded_body_length = encoded_body_length_;
    status.decoded_body_length = content_->size();

    client_->OnComplete(status);
  } else if (client_.is_connected()) {
    URLLoaderCompletionStatus status;
    status.error_code = error_code;
    client_->OnComplete(status);
  }

  memory_cache_->OnLoaderCompleted(this);
  // `memory_cache_` deleted `this`.
}

void NetworkServiceMemoryCacheURLLoader::OnClientDisconnected() {
  Finish(net::ERR_FAILED);
}

void NetworkServiceMemoryCacheURLLoader::FollowRedirect(
    const std::vector<std::string>& removed_headers,
    const net::HttpRequestHeaders& modified_headers,
    const net::HttpRequestHeaders& modified_cors_exempt_headers,
    const absl::optional<GURL>& new_url) {
  NOTREACHED();
}

void NetworkServiceMemoryCacheURLLoader::SetPriority(
    net::RequestPriority priority,
    int32_t intra_priority_value) {}

void NetworkServiceMemoryCacheURLLoader::PauseReadingBodyFromNet() {}

void NetworkServiceMemoryCacheURLLoader::ResumeReadingBodyFromNet() {}

}  // namespace network
