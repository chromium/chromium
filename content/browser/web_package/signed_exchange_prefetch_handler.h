// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_PREFETCH_HANDLER_H_
#define CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_PREFETCH_HANDLER_H_

#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/network/public/cpp/resource_response.h"
#include "services/network/public/mojom/url_loader.mojom.h"

namespace net {
struct SHA256HashValue;
}

namespace network {
class SharedURLLoaderFactory;
}

namespace blink {
class URLLoaderThrottle;
}

namespace content {

class SignedExchangeLoader;
class SignedExchangePrefetchMetricRecorder;

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
      int frame_tree_node_id,
      const network::ResourceRequest& resource_request,
      const network::ResourceResponseHead& response,
      mojo::ScopedDataPipeConsumerHandle response_body,
      network::mojom::URLLoaderPtr network_loader,
      mojo::PendingReceiver<network::mojom::URLLoaderClient>
          network_client_receiver,
      scoped_refptr<network::SharedURLLoaderFactory> network_loader_factory,
      URLLoaderThrottlesGetter loader_throttles_getter,
      network::mojom::URLLoaderClient* forwarding_client,
      scoped_refptr<SignedExchangePrefetchMetricRecorder> metric_recorder,
      const std::string& accept_langs);

  ~SignedExchangePrefetchHandler() override;

  // This connects |loader_receiver| to the SignedExchangeLoader, and returns
  // the pending client receiver to the loader. The returned client receiver can
  // be bound to the downstream client so that they can start directly receiving
  // upcalls from the SignedExchangeLoader. After this point |this| can be
  // destructed.
  mojo::PendingReceiver<network::mojom::URLLoaderClient> FollowRedirect(
      mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver);

  // Returns the header integrity value of the loaded signed exchange if
  // available. This is available after OnReceiveRedirect() of
  // |forwarding_client| is called and before FollowRedirect() of |this| is
  // called. Otherwise returns nullopt.
  base::Optional<net::SHA256HashValue> ComputeHeaderIntegrity() const;

  // Returns the signature expire time of the loaded signed exchange if
  // available. This is available after OnReceiveRedirect() of
  // |forwarding_client| is called and before FollowRedirect() of |this| is
  // called. Otherwise returns a null Time.
  base::Time GetSignatureExpireTime() const;

 private:
  // network::mojom::URLLoaderClient overrides:
  void OnReceiveResponse(network::mojom::URLResponseHeadPtr head) override;
  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         network::mojom::URLResponseHeadPtr head) override;
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        base::OnceCallback<void()> callback) override;
  void OnReceiveCachedMetadata(mojo_base::BigBuffer data) override;
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body) override;
  void OnComplete(const network::URLLoaderCompletionStatus& status) override;

  mojo::Receiver<network::mojom::URLLoaderClient> loader_client_receiver_{this};

  std::unique_ptr<SignedExchangeLoader> signed_exchange_loader_;

  network::mojom::URLLoaderClient* forwarding_client_;

  DISALLOW_COPY_AND_ASSIGN(SignedExchangePrefetchHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_PREFETCH_HANDLER_H_
