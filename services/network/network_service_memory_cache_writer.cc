// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/network_service_memory_cache_writer.h"

#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/typed_macros.h"
#include "services/network/network_service_memory_cache.h"

namespace network {

NetworkServiceMemoryCacheWriter::NetworkServiceMemoryCacheWriter(
    base::WeakPtr<NetworkServiceMemoryCache> cache,
    uint64_t trace_id,
    std::string cache_key,
    net::URLRequest* url_request,
    mojom::RequestDestination request_destination,
    const mojom::URLResponseHeadPtr& response_head)
    : cache_(std::move(cache)),
      trace_id_(trace_id),
      cache_key_(std::move(cache_key)),
      url_request_(url_request),
      request_destination_(request_destination),
      response_head_(response_head.Clone()) {
  DCHECK(cache_);
  DCHECK(url_request_);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(
      "loading", "NetworkServiceMemoryCacheWriter", TRACE_ID_LOCAL(trace_id_),
      "key", cache_key_);
}

NetworkServiceMemoryCacheWriter::~NetworkServiceMemoryCacheWriter() {
  TRACE_EVENT_NESTABLE_ASYNC_END0("loading", "NetworkServiceMemoryCacheWriter",
                                  TRACE_ID_LOCAL(trace_id_));
}

void NetworkServiceMemoryCacheWriter::OnDataRead(const char* buf, int result) {
  TRACE_EVENT_NESTABLE_ASYNC_INSTANT1(
      "loading", "NetworkServiceMemoryCacheWriter::OnDataRead",
      TRACE_ID_LOCAL(trace_id_), "result", result);

  if (result > 0)
    received_data_.insert(received_data_.end(), buf, buf + result);
}

void NetworkServiceMemoryCacheWriter::OnCompleted(
    const URLLoaderCompletionStatus& status) {
  TRACE_EVENT_NESTABLE_ASYNC_INSTANT2(
      "loading", "NetworkServiceMemoryCacheWriter::OnCompleted",
      TRACE_ID_LOCAL(trace_id_), "result", status.error_code, "total_size",
      received_data_.size());

  if (cache_) {
    cache_->StoreResponse(cache_key_, status, request_destination_,
                          std::move(response_head_), std::move(received_data_));
  }
  // `this` will be deleted by the owner.
}

}  // namespace network
