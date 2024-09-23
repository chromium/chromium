// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PREFETCH_URL_LOADER_CLIENT_H_
#define SERVICES_NETWORK_PREFETCH_URL_LOADER_CLIENT_H_

#include "base/containers/linked_list.h"
#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/network_isolation_key.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "url/gurl.h"

namespace network {

class PrefetchCache;

// An implementation of PrefetchURLLoaderClient that is used for prefetches. It
// can cache ongoing requests until they are requests by a render process, and
// then forward on the callbacks that were received.
class COMPONENT_EXPORT(NETWORK_SERVICE) PrefetchURLLoaderClient final
    : public mojom::URLLoaderClient,
      public base::LinkNode<PrefetchURLLoaderClient> {
 public:
  // Use PrefetchCache::Emplace() to create a PrefetchURLLoaderClient object.
  PrefetchURLLoaderClient(base::PassKey<PrefetchCache>,
                          const net::NetworkIsolationKey& nik,
                          const ResourceRequest& request,
                          base::TimeTicks expiry_time,
                          PrefetchCache* prefetch_cache);

  PrefetchURLLoaderClient(const PrefetchURLLoaderClient&) = delete;
  PrefetchURLLoaderClient& operator=(const PrefetchURLLoaderClient&) = delete;

  // PrefetchURLLoaderClient is owned by the PrefetchCache. This destructor
  // should never be called directly.
  ~PrefetchURLLoaderClient() final;

  // Accessors used by NetworkContext::PrefetchCache.
  const net::NetworkIsolationKey& network_isolation_key() const {
    return network_isolation_key_;
  }

  const GURL& url() const { return request_.url; }

  base::TimeTicks expiry_time() const { return expiry_time_; }

  // Returns a PendingReceiver that can be passed to a URLLoaderFactory. Should
  // only be called once.
  mojo::PendingReceiver<mojom::URLLoader> GetURLLoaderPendingReceiver();

  // Returns a PendingRemote that can be passed to a URLLoaderFactory. Should
  // only be called once.
  mojo::PendingRemote<mojom::URLLoaderClient> BindNewPipeAndPassRemote();

  // Sets `real_client_` and replays any callbacks that have been received up
  // until this point. Once this has been called, the object becomes
  // self-owning and will delete itself if it loses connection to `real_client_`
  // or the URLLoader. May delete `this`.
  void SetClient(mojo::PendingRemote<mojom::URLLoaderClient> client);

  // Implementation of mojom::URLLoaderClient. Each of these methods forwards
  // the arguments in `real_client_` if it has been set, otherwise caches the
  // arguments in `pending_callbacks_`.
  void OnReceiveEarlyHints(mojom::EarlyHintsPtr early_hints) override;
  void OnReceiveResponse(
      mojom::URLResponseHeadPtr head,
      mojo::ScopedDataPipeConsumerHandle body,
      std::optional<mojo_base::BigBuffer> cached_metadata) override;
  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         mojom::URLResponseHeadPtr head) override;
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback callback) override;
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
  void OnComplete(const URLLoaderCompletionStatus& status) override;

 private:
  // Deletes this object if `real_client_` has been initialized. Otherwise
  // sets the `disconnected_` flag to remind itself to delete itself later.
  void OnDisconnect();

  // Calls `method` with `args` if `real_client_` has been set, otherwise
  // records the call in `pending_callbacks_` to replay later.
  template <typename Method, typename... Args>
  void ForwardOrRecord(Method method, Args... args);

  // Calls `method` with `args` on `real_client_`.
  template <typename Method, typename... Args>
  void ForwardToRealClient(Method method, Args... args);

  const ResourceRequest request_;
  const net::NetworkIsolationKey network_isolation_key_;
  const base::TimeTicks expiry_time_;

  // Callbacks that have been called prior to a render process consuming this
  // request that need to be replayed.
  std::vector<base::OnceClosure> pending_callbacks_;

  // This is initialized once a render process consumes this request.
  mojo::Remote<mojom::URLLoaderClient> real_client_;

  // This is initialized at construction.
  mojo::Receiver<mojom::URLLoaderClient> receiver_{this};

  // This is initialized when the request is started.
  mojo::Remote<mojom::URLLoader> url_loader_;

  // A pointer to the PrefetchCache object that created and owns us.
  const raw_ptr<PrefetchCache> prefetch_cache_;

  // Set to true when we lose a mojo connection.
  bool disconnected_ = false;
};

}  // namespace network

#endif  // SERVICES_NETWORK_PREFETCH_URL_LOADER_CLIENT_H_
