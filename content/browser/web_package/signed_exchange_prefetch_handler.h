// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_PREFETCH_HANDLER_H_
#define CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_PREFETCH_HANDLER_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/unguessable_token.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace net {
class NetworkAnonymizationKey;
}

namespace network {
class SharedURLLoaderFactory;
struct ResourceRequest;
}  // namespace network

namespace blink {
class URLLoaderThrottle;
}

namespace content {

class PrefetchedSignedExchangeCacheEntry;
class SignedExchangeLoader;

// Attached to each PrefetchURLLoader if the prefetch is for a signed exchange.
class SignedExchangePrefetchHandler final
    : public network::mojom::URLLoaderClient {
 public:
  using URLLoaderThrottlesGetter = base::RepeatingCallback<
      std::vector<std::unique_ptr<blink::URLLoaderThrottle>>()>;

  // This takes |network_loader| and |network_client| to set up the
  // SignedExchangeLoader (so that the loader can load data from the network).
  // |forwarding_client| is a pointer to the downstream client (typically who
  // creates this handler).
  SignedExchangePrefetchHandler(
      FrameTreeNodeId frame_tree_node_id,
      const network::ResourceRequest& resource_request,
      network::mojom::URLResponseHeadPtr response,
      mojo::ScopedDataPipeConsumerHandle response_body,
      mojo::PendingRemote<network::mojom::URLLoader> network_loader,
      mojo::PendingReceiver<network::mojom::URLLoaderClient>
          network_client_receiver,
      scoped_refptr<network::SharedURLLoaderFactory> network_loader_factory,
      URLLoaderThrottlesGetter loader_throttles_getter,
      network::mojom::URLLoaderClient* forwarding_client,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      const std::string& accept_langs,
      bool keep_entry_for_prefetch_cache);

  SignedExchangePrefetchHandler(const SignedExchangePrefetchHandler&) = delete;
  SignedExchangePrefetchHandler& operator=(
      const SignedExchangePrefetchHandler&) = delete;

  ~SignedExchangePrefetchHandler() override;

  // This connects |loader_receiver| to the SignedExchangeLoader, and returns
  // the pending client receiver to the loader. The returned client receiver can
  // be bound to the downstream client so that they can start directly receiving
  // upcalls from the SignedExchangeLoader. After this point |this| can be
  // destructed.
  mojo::PendingReceiver<network::mojom::URLLoaderClient> FollowRedirect(
      mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver);

  // Called to get the information about the prefetched signed exchange. To call
  // this method, |keep_entry_for_prefetch_cache| constructor argument must be
  // set.
  std::unique_ptr<PrefetchedSignedExchangeCacheEntry>
  TakePrefetchedSignedExchangeCacheEntry();

 private:
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

  mojo::Receiver<network::mojom::URLLoaderClient> loader_client_receiver_{this};

  std::unique_ptr<SignedExchangeLoader> signed_exchange_loader_;

  raw_ptr<network::mojom::URLLoaderClient> forwarding_client_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_PREFETCH_HANDLER_H_
