// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_PREFETCH_URL_LOADER_SERVICE_CONTEXT_H_
#define CONTENT_BROWSER_LOADER_PREFETCH_URL_LOADER_SERVICE_CONTEXT_H_

#include "base/functional/callback.h"
#include "content/browser/loader/subresource_proxying_url_loader_service.h"
#include "content/common/content_export.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "net/base/load_flags.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/blink/public/mojom/renderer_preference_watcher.mojom.h"

namespace blink {
class URLLoaderThrottle;
}

namespace content {

class BrowserContext;

// Contains the data and functions to handle <link rel="prefetch"> requests.
class CONTENT_EXPORT PrefetchURLLoaderServiceContext final
    : public blink::mojom::RendererPreferenceWatcher {
 public:
  using BindContext = SubresourceProxyingURLLoaderService::BindContext;

  PrefetchURLLoaderServiceContext(
      BrowserContext* browser_context,
      mojo::ReceiverSet<network::mojom::URLLoaderFactory,
                        scoped_refptr<BindContext>>& loader_factory_receivers);
  ~PrefetchURLLoaderServiceContext() override;

  PrefetchURLLoaderServiceContext(const PrefetchURLLoaderServiceContext&) =
      delete;
  PrefetchURLLoaderServiceContext& operator=(
      const PrefetchURLLoaderServiceContext&) = delete;

  void CreatePrefetchLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& resource_request_in,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation);

  // Register a callback that is fired right before a prefetch load is started
  // by this service.
  void RegisterPrefetchLoaderCallbackForTest(
      const base::RepeatingClosure& prefetch_load_callback) {
    prefetch_load_callback_for_testing_ = prefetch_load_callback;
  }

  void SetAcceptLanguages(const std::string& accept_langs) {
    accept_langs_ = accept_langs;
  }

  static bool IsPrefetchRequest(const network::ResourceRequest& request) {
    return request.load_flags & net::LOAD_PREFETCH;
  }

 private:
  // This ensures that the BindContext's |cross_origin_factory| member
  // exists by setting it to a special URLLoaderFactory created by the current
  // context's RenderFrameHost.
  void EnsureCrossOriginFactory();
  bool IsValidCrossOriginPrefetch(
      const network::ResourceRequest& resource_request);

  base::UnguessableToken GenerateRecursivePrefetchToken(
      base::WeakPtr<BindContext> bind_context,
      const network::ResourceRequest& request);

  // blink::mojom::RendererPreferenceWatcher.
  void NotifyUpdate(const blink::RendererPreferences& new_prefs) override;

  // For URLLoaderThrottlesGetter.
  std::vector<std::unique_ptr<blink::URLLoaderThrottle>>
  CreateURLLoaderThrottles(const network::ResourceRequest& request,
                           FrameTreeNodeId frame_tree_node_id);

  BindContext* current_bind_context() const {
    return loader_factory_receivers_->current_context().get();
  }

  raw_ptr<BrowserContext> browser_context_ = nullptr;

  raw_ref<mojo::ReceiverSet<network::mojom::URLLoaderFactory,
                            scoped_refptr<BindContext>>>
      loader_factory_receivers_;

  mojo::ReceiverSet<network::mojom::URLLoader,
                    std::unique_ptr<network::mojom::URLLoader>>
      prefetch_loader_receivers_;

  // Used in the IO thread.
  mojo::Receiver<blink::mojom::RendererPreferenceWatcher>
      preference_watcher_receiver_{this};

  base::RepeatingClosure prefetch_load_callback_for_testing_;

  std::string accept_langs_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_PREFETCH_URL_LOADER_SERVICE_CONTEXT_H_
