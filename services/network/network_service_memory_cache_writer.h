// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_NETWORK_SERVICE_MEMORY_CACHE_WRITER_H_
#define SERVICES_NETWORK_NETWORK_SERVICE_MEMORY_CACHE_WRITER_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "net/base/transport_info.h"
#include "services/network/public/mojom/url_loader_completion_status.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace net {
class URLRequest;
}  // namespace net

namespace network {

class NetworkServiceMemoryCache;

// Duplicates an HTTP response that comes from the underlying layer, i.e., a
// URLLoader. The URLLoader owns an instance of this class.
//
// When the URLLoader completed successfully, the duplicated response is stored
// into the in-memory cache so that it can be served without disk access until
// it gets evicted.
class COMPONENT_EXPORT(NETWORK_SERVICE) NetworkServiceMemoryCacheWriter {
 public:
  NetworkServiceMemoryCacheWriter(
      base::WeakPtr<NetworkServiceMemoryCache> cache,
      uint64_t trace_id,
      size_t max_bytes,
      std::string cache_key,
      net::URLRequest* request,
      mojom::RequestDestination request_destination,
      const net::TransportInfo& transport_info,
      const mojom::URLResponseHeadPtr& response_head);

  ~NetworkServiceMemoryCacheWriter();

  // Called when the owner received response content. Returns false when `this`
  // doesn't want to duplicate the response any longer (e.g. the content is too
  // large).
  bool OnDataRead(const char* buf, int result);

  // Called when the owner completed.
  void OnCompleted(const URLLoaderCompletionStatus& status);

 private:
  base::WeakPtr<NetworkServiceMemoryCache> cache_;

  // Used for tracing.
  const uint64_t trace_id_;

  // The maximum size of `received_data_`.
  const size_t max_bytes_;

  std::string cache_key_;

  // `url_request_` must outlive `this`. The owner owns `url_request_`.
  const raw_ptr<net::URLRequest> url_request_;

  mojom::RequestDestination request_destination_;

  const net::TransportInfo transport_info_;
  mojom::URLResponseHeadPtr response_head_;
  std::vector<unsigned char> received_data_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_NETWORK_SERVICE_MEMORY_CACHE_WRITER_H_
