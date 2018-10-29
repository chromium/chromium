// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_PREFETCH_URL_LOADER_SERVICE_H_
#define CONTENT_BROWSER_LOADER_PREFETCH_URL_LOADER_SERVICE_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/browser/web_package/signed_exchange_prefetch_metric_recorder.h"
#include "content/common/content_export.h"
#include "content/common/url_loader_factory_bundle.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/strong_binding_set.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace net {
class URLRequestContextGetter;
}

namespace network {
class SharedURLLoaderFactory;
}

namespace content {

class ResourceContext;
class URLLoaderFactoryGetter;
class URLLoaderThrottle;

class CONTENT_EXPORT PrefetchURLLoaderService final
    : public base::RefCountedThreadSafe<PrefetchURLLoaderService,
                                        BrowserThread::DeleteOnIOThread>,
      public network::mojom::URLLoaderFactory {
 public:
  PrefetchURLLoaderService();

  // Must be called on the IO thread. The given |resource_context| will
  // be valid as far as request_context_getter returns non-null context.
  void InitializeResourceContext(
      ResourceContext* resource_context,
      scoped_refptr<net::URLRequestContextGetter> request_context_getter);

  void GetFactory(
      network::mojom::URLLoaderFactoryRequest request,
      int frame_tree_node_id,
      std::unique_ptr<network::SharedURLLoaderFactoryInfo> factory_info);

  // Used only when NetworkService is not enabled (or indirectly via the
  // other CreateLoaderAndStart when NetworkService is enabled).
  // This creates a loader and starts fetching using the given
  // |network_lader_factory|. |frame_tree_node_id| may be given and used to
  // create necessary throttles when Network Service is enabled when the
  // created loader internally makes additional requests.
  void CreateLoaderAndStart(
      network::mojom::URLLoaderRequest request,
      int32_t routing_id,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& resource_request,
      network::mojom::URLLoaderClientPtr client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      scoped_refptr<network::SharedURLLoaderFactory> network_loader_factory,
      base::RepeatingCallback<int(void)> frame_tree_node_id_getter);

  // Register a callback that is fired right before a prefetch load is started
  // by this service.
  void RegisterPrefetchLoaderCallbackForTest(
      const base::RepeatingClosure& prefetch_load_callback) {
    prefetch_load_callback_for_testing_ = prefetch_load_callback;
  }

  scoped_refptr<SignedExchangePrefetchMetricRecorder>
  signed_exchange_prefetch_metric_recorder() {
    return signed_exchange_prefetch_metric_recorder_;
  }

 private:
  friend class base::DeleteHelper<content::PrefetchURLLoaderService>;
  friend struct BrowserThread::DeleteOnThread<BrowserThread::IO>;
  struct BindContext;

  ~PrefetchURLLoaderService() override;

  // network::mojom::URLLoaderFactory:
  void CreateLoaderAndStart(network::mojom::URLLoaderRequest request,
                            int32_t routing_id,
                            int32_t request_id,
                            uint32_t options,
                            const network::ResourceRequest& resource_request,
                            network::mojom::URLLoaderClientPtr client,
                            const net::MutableNetworkTrafficAnnotationTag&
                                traffic_annotation) override;
  void Clone(network::mojom::URLLoaderFactoryRequest request) override;

  // For URLLoaderThrottlesGetter.
  std::vector<std::unique_ptr<content::URLLoaderThrottle>>
  CreateURLLoaderThrottles(
      const network::ResourceRequest& request,
      base::RepeatingCallback<int(void)> frame_tree_node_id_getter);

  scoped_refptr<URLLoaderFactoryGetter> loader_factory_getter_;
  ResourceContext* resource_context_ = nullptr;
  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;

  mojo::BindingSet<network::mojom::URLLoaderFactory,
                   std::unique_ptr<BindContext>>
      loader_factory_bindings_;

  base::RepeatingClosure prefetch_load_callback_for_testing_;

  scoped_refptr<SignedExchangePrefetchMetricRecorder>
      signed_exchange_prefetch_metric_recorder_;

  DISALLOW_COPY_AND_ASSIGN(PrefetchURLLoaderService);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_PREFETCH_URL_LOADER_SERVICE_H_
