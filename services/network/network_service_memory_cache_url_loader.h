// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_NETWORK_SERVICE_MEMORY_CACHE_URL_LOADER_H_
#define SERVICES_NETWORK_NETWORK_SERVICE_MEMORY_CACHE_URL_LOADER_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "net/log/net_log_with_source.h"
#include "services/network/public/mojom/devtools_observer.mojom.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace network {

class NetworkServiceMemoryCache;
struct ResourceRequest;

// A URLLoader that serves a response from the in-memory cache. Must be used
// only after CORS/CORP checks succeeded.
class NetworkServiceMemoryCacheURLLoader : public mojom::URLLoader {
 public:
  NetworkServiceMemoryCacheURLLoader(
      NetworkServiceMemoryCache* memory_cache,
      uint64_t trace_id,
      const ResourceRequest& resource_request,
      const net::NetLogWithSource& net_log,
      mojo::PendingReceiver<mojom::URLLoader> receiver,
      mojo::PendingRemote<mojom::URLLoaderClient> client,
      scoped_refptr<base::RefCountedBytes> content,
      int64_t encoded_body_length);

  ~NetworkServiceMemoryCacheURLLoader() override;

  // Starts sending the response. `response_head` is a cached response stored
  // in the in-memory cache.
  void Start(const ResourceRequest& resource_request,
             mojom::URLResponseHeadPtr response_head);

 private:
  // Adjusts load timings and cache related fields in `response_head`.
  void UpdateResponseHead(const ResourceRequest& resource_request,
                          mojom::URLResponseHeadPtr& response_head);

  void MaybeNotifyRawResponse(const mojom::URLResponseHead& response_head);

  // Sends response body to the client via a mojo data pipe.
  void WriteMore();

  // Called when the client can read response body.
  void OnProducerHandleReady(MojoResult result,
                             const mojo::HandleSignalsState& state);

  // Called when this loader finished. `this` becomes invalid after Finish()
  // because `memory_cache_` deletes `this` at the end.
  void Finish(int error_code);

  void OnClientDisconnected();

  // mojom::URLLoader implementations:
  void FollowRedirect(
      const std::vector<std::string>& removed_headers,
      const net::HttpRequestHeaders& modified_headers,
      const net::HttpRequestHeaders& modified_cors_exempt_headers,
      const absl::optional<GURL>& new_url) override;
  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override;
  void PauseReadingBodyFromNet() override;
  void ResumeReadingBodyFromNet() override;

  // `memory_cache_` owns `this`.
  const raw_ptr<NetworkServiceMemoryCache> memory_cache_;

  // For tracing.
  const uint64_t trace_id_;

  const net::NetLogWithSource net_log_;

  mojo::Receiver<mojom::URLLoader> receiver_;
  mojo::Remote<mojom::URLLoaderClient> client_;

  const absl::optional<std::string> devtools_request_id_;
  mojo::Remote<mojom::DevToolsObserver> devtools_observer_;

  // The response body to be served.
  scoped_refptr<base::RefCountedBytes> content_;
  const int64_t encoded_body_length_;

  mojo::ScopedDataPipeProducerHandle producer_handle_;
  std::unique_ptr<mojo::SimpleWatcher> producer_handle_watcher_;
  size_t write_position_ = 0;

  base::WeakPtrFactory<NetworkServiceMemoryCacheURLLoader> weak_ptr_factory_{
      this};
};

}  // namespace network

#endif  // SERVICES_NETWORK_NETWORK_SERVICE_MEMORY_CACHE_URL_LOADER_H_
