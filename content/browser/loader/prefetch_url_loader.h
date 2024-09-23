// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_PREFETCH_URL_LOADER_H_
#define CONTENT_BROWSER_LOADER_PREFETCH_URL_LOADER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/unguessable_token.h"
#include "content/browser/web_package/prefetched_signed_exchange_cache.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe_drainer.h"
#include "net/base/network_anonymization_key.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
}

namespace blink {
class URLLoaderThrottle;
}

namespace content {

class BrowserContext;
class PrefetchedSignedExchangeCacheAdapter;
class SignedExchangePrefetchHandler;

// A URLLoader for loading a prefetch request, including <link rel="prefetch">.
// It basically just keeps draining the data.
class PrefetchURLLoader : public network::mojom::URLLoader,
                          public network::mojom::URLLoaderClient,
                          public mojo::DataPipeDrainer::Client {
 public:
  using URLLoaderThrottlesGetter = base::RepeatingCallback<
      std::vector<std::unique_ptr<blink::URLLoaderThrottle>>()>;
  using RecursivePrefetchTokenGenerator =
      base::OnceCallback<base::UnguessableToken(
          const network::ResourceRequest&)>;

  // |network_anonymization_key| must be the NetworkAnonymizationKey that will
  // be used for the request (either matching
  // |resource_request.trusted_params|'s IsolationInfo, if trusted_params| is
  // non-null, or bound to |network_loader_factory|, otherwise).
  //
  // |url_loader_throttles_getter| may be used when a prefetch handler needs to
  // additionally create a request (e.g. for fetching certificate if the
  // prefetch was for a signed exchange).
  PrefetchURLLoader(
      int32_t request_id,
      uint32_t options,
      FrameTreeNodeId frame_tree_node_id,
      const network::ResourceRequest& resource_request,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      scoped_refptr<network::SharedURLLoaderFactory> network_loader_factory,
      URLLoaderThrottlesGetter url_loader_throttles_getter,
      BrowserContext* browser_context,
      scoped_refptr<PrefetchedSignedExchangeCache>
          prefetched_signed_exchange_cache,
      const std::string& accept_langs,
      RecursivePrefetchTokenGenerator recursive_prefetch_token_generator);

  PrefetchURLLoader(const PrefetchURLLoader&) = delete;
  PrefetchURLLoader& operator=(const PrefetchURLLoader&) = delete;

  ~PrefetchURLLoader() override;

  // Sends an empty response's body to |forwarding_client_|. If failed to create
  // a new data pipe, sends ERR_INSUFFICIENT_RESOURCES and closes the
  // connection, and returns false. Otherwise returns true.
  bool SendEmptyBody();

  void SendOnComplete(
      const network::URLLoaderCompletionStatus& completion_status);

 private:
  // network::mojom::URLLoader overrides:
  void FollowRedirect(
      const std::vector<std::string>& removed_headers,
      const net::HttpRequestHeaders& modified_headers,
      const net::HttpRequestHeaders& modified_cors_exempt_headers,
      const std::optional<GURL>& new_url) override;
  void SetPriority(net::RequestPriority priority,
                   int intra_priority_value) override;
  void PauseReadingBodyFromNet() override;
  void ResumeReadingBodyFromNet() override;

  // network::mojom::URLLoaderClient overrides:
  void OnReceiveEarlyHints(network::mojom::EarlyHintsPtr early_hints) override;
  void OnReceiveResponse(
      network::mojom::URLResponseHeadPtr head,
      mojo::ScopedDataPipeConsumerHandle body,
      std::optional<mojo_base::BigBuffer> cached_metadata) override;
  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         network::mojom::URLResponseHeadPtr head) override;
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        base::OnceCallback<void()> callback) override;
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
  void OnComplete(const network::URLLoaderCompletionStatus& status) override;

  // mojo::DataPipeDrainer::Client overrides:
  // This just does nothing but keep reading.
  void OnDataAvailable(base::span<const uint8_t> data) override {}
  void OnDataComplete() override {}

  void OnNetworkConnectionError();

  const FrameTreeNodeId frame_tree_node_id_;

  // Set in the constructor and updated when redirected.
  network::ResourceRequest resource_request_;

  network::mojom::URLResponseHeadPtr response_;

  const net::NetworkAnonymizationKey network_anonymization_key_;

  scoped_refptr<network::SharedURLLoaderFactory> network_loader_factory_;

  // For the actual request.
  mojo::Remote<network::mojom::URLLoader> loader_;
  mojo::Receiver<network::mojom::URLLoaderClient> client_receiver_{this};

  // To be a URLLoader for the client.
  mojo::Remote<network::mojom::URLLoaderClient> forwarding_client_;

  URLLoaderThrottlesGetter url_loader_throttles_getter_;

  std::unique_ptr<mojo::DataPipeDrainer> pipe_drainer_;

  std::unique_ptr<SignedExchangePrefetchHandler>
      signed_exchange_prefetch_handler_;

  // Used to store the prefetched signed exchanges to a
  // PrefetchedSignedExchangeCache.
  std::unique_ptr<PrefetchedSignedExchangeCacheAdapter>
      prefetched_signed_exchange_cache_adapter_;

  const std::string accept_langs_;

  RecursivePrefetchTokenGenerator recursive_prefetch_token_generator_;

  // TODO(kinuko): This value can become stale if the preference is updated.
  // Make this listen to the changes if it becomes a real concern.
  bool is_signed_exchange_handling_enabled_ = false;
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_PREFETCH_URL_LOADER_H_
