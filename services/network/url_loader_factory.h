// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_URL_LOADER_FACTORY_H_
#define SERVICES_NETWORK_URL_LOADER_FACTORY_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/network_handle.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/orb/orb_api.h"
#include "services/network/public/mojom/cookie_access_observer.mojom.h"
#include "services/network/public/mojom/devtools_observer.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/trust_token_access_observer.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_loader_network_service_observer.mojom.h"
#include "services/network/url_loader_context.h"

namespace network {

class NetworkContext;
class ResourceSchedulerClient;

namespace cors {
class CorsURLLoaderFactory;
}  // namespace cors

namespace mojom {
class URLLoader;
}  // namespace mojom

// This class is an implementation of mojom::URLLoaderFactory that
// creates a mojom::URLLoader.
// A URLLoaderFactory has a pointer to ResourceSchedulerClient. A
// ResourceSchedulerClient is associated with cloned
// NetworkServiceURLLoaderFactories. Roughly one URLLoaderFactory
// is created for one frame in render process, so it means ResourceScheduler
// works on each frame.
// A URLLoaderFactory can be created with null ResourceSchedulerClient, in which
// case requests constructed by the factory will not be throttled.
// Note that the CORS related part is implemented in CorsURLLoader[Factory]
// and NetworkContext::CreateURLLoaderFactory returns a CorsURLLoaderFactory,
// instead of a URLLoaderFactory.
class URLLoaderFactory : public mojom::URLLoaderFactory,
                         public URLLoaderContext {
 public:
  // NOTE: |context| must outlive this instance.
  URLLoaderFactory(
      NetworkContext* context,
      mojom::URLLoaderFactoryParamsPtr params,
      scoped_refptr<ResourceSchedulerClient> resource_scheduler_client,
      cors::CorsURLLoaderFactory* cors_url_loader_factory);

  URLLoaderFactory(const URLLoaderFactory&) = delete;
  URLLoaderFactory& operator=(const URLLoaderFactory&) = delete;

  ~URLLoaderFactory() override;

  // mojom::URLLoaderFactory implementation.
  void CreateLoaderAndStart(mojo::PendingReceiver<mojom::URLLoader> receiver,
                            int32_t request_id,
                            uint32_t options,
                            const ResourceRequest& resource_request,
                            mojo::PendingRemote<mojom::URLLoaderClient> client,
                            const net::MutableNetworkTrafficAnnotationTag&
                                traffic_annotation) override;
  void Clone(mojo::PendingReceiver<mojom::URLLoaderFactory> receiver) override;

  // URLLoaderContext implementation.
  bool ShouldRequireIsolationInfo() const override;
  const cors::OriginAccessList& GetOriginAccessList() const override;
  const mojom::URLLoaderFactoryParams& GetFactoryParams() const override;
  mojom::CookieAccessObserver* GetCookieAccessObserver() const override;
  mojom::TrustTokenAccessObserver* GetTrustTokenAccessObserver() const override;
  mojom::CrossOriginEmbedderPolicyReporter* GetCoepReporter() const override;
  mojom::DevToolsObserver* GetDevToolsObserver() const override;
  mojom::NetworkContextClient* GetNetworkContextClient() const override;
  mojom::TrustedURLLoaderHeaderClient* GetUrlLoaderHeaderClient()
      const override;
  mojom::URLLoaderNetworkServiceObserver* GetURLLoaderNetworkServiceObserver()
      const override;
  net::URLRequestContext* GetUrlRequestContext() const override;
  scoped_refptr<ResourceSchedulerClient> GetResourceSchedulerClient()
      const override;
  orb::PerFactoryState& GetMutableOrbState() override;
  bool DataUseUpdatesEnabled() override;

  // Allows starting a URLLoader with a synchronous URLLoaderClient as an
  // optimization.
  void CreateLoaderAndStartWithSyncClient(
      mojo::PendingReceiver<mojom::URLLoader> receiver,
      int32_t request_id,
      uint32_t options,
      const ResourceRequest& resource_request,
      mojo::PendingRemote<mojom::URLLoaderClient> client,
      base::WeakPtr<mojom::URLLoaderClient> sync_client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation);

  // Returns the network that URLLoaders, created out of this factory, will
  // target. If == net::handles::kInvalidNetworkHandle, then no network is being
  // targeted and the system default network will be used (see
  // network.mojom.NetworkContextParams::bound_network for more info).
  net::handles::NetworkHandle GetBoundNetworkForTesting() const;

  static constexpr int kMaxKeepaliveConnections = 2048;
  static constexpr int kMaxKeepaliveConnectionsPerTopLevelFrame = 256;
  static constexpr int kMaxTotalKeepaliveRequestSize = 512 * 1024;

 private:
  // Starts the timer to call
  // URLLoaderNetworkServiceObserver::OnLoadingStateUpdate(), if
  // needed.
  void MaybeStartUpdateLoadInfoTimer();

  // Invoked once the browser has acknowledged receiving the previous LoadInfo.
  // Sets |waiting_on_load_state_ack_| to false, and calls
  // MaybeStartUpdateLoadeInfoTimer.
  void AckUpdateLoadInfo();

  // Finds the most relevant URLLoader that is outstanding and asks it to
  // send an update.
  void UpdateLoadInfo();

  // The NetworkContext that indirectly owns |this|.
  const raw_ptr<NetworkContext> context_;
  mojom::URLLoaderFactoryParamsPtr params_;
  scoped_refptr<ResourceSchedulerClient> resource_scheduler_client_;
  mojo::Remote<mojom::TrustedURLLoaderHeaderClient> header_client_;

  // |cors_url_loader_factory_| owns this.
  raw_ptr<cors::CorsURLLoaderFactory> const cors_url_loader_factory_;

  // To allow subsequent range requests, ORB stores URLs of non-range-request
  // responses that sniffed as an audio or video resource.  The lifetime of that
  // storage should cover the lifetime of media elements that are responsible
  // for the initial request and subsequent range requests.  The lifetime of
  // `orb_per_factory_state_` is slightly bigger (URLLoaderFactory is typically
  // associated with a single HTML document and covers all media documents
  // within) but this approach seems easiest to implement.
  orb::PerFactoryState orb_state_;

  mojo::Remote<mojom::CookieAccessObserver> cookie_observer_;
  mojo::Remote<mojom::TrustTokenAccessObserver> trust_token_observer_;
  mojo::Remote<mojom::DevToolsObserver> devtools_observer_;

  base::OneShotTimer update_load_info_timer_;
  bool waiting_on_load_state_ack_ = false;
};

}  // namespace network

#endif  // SERVICES_NETWORK_URL_LOADER_FACTORY_H_
