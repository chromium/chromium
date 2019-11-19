// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_FETCH_CONTEXT_IMPL_H_
#define CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_FETCH_CONTEXT_IMPL_H_

#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/blink/public/mojom/renderer_preference_watcher.mojom.h"
#include "third_party/blink/public/mojom/renderer_preferences.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker.mojom.h"
#include "third_party/blink/public/mojom/worker/subresource_loader_updater.mojom.h"
#include "third_party/blink/public/platform/web_worker_fetch_context.h"
#include "url/gurl.h"

namespace content {
class ResourceDispatcher;
class URLLoaderThrottleProvider;
class WebSocketHandshakeThrottleProvider;

class CONTENT_EXPORT ServiceWorkerFetchContextImpl final
    : public blink::WebWorkerFetchContext,
      public blink::mojom::SubresourceLoaderUpdater,
      public blink::mojom::RendererPreferenceWatcher {
 public:
  // |url_loader_factory_info| is used for regular loads from the service worker
  // (i.e., Fetch API). It typically goes to network, but it might internally
  // contain non-NetworkService factories for handling non-http(s) URLs like
  // chrome-extension://.
  // |script_loader_factory_info| is used for importScripts() from the service
  // worker when InstalledScriptsManager doesn't have the requested script. It
  // is a ServiceWorkerScriptLoaderFactory, which loads and installs the script.
  // |script_url_to_skip_throttling| is a URL which is already throttled in the
  // browser process so that it doesn't need to be throttled in the renderer
  // again.
  ServiceWorkerFetchContextImpl(
      const blink::mojom::RendererPreferences& renderer_preferences,
      const GURL& worker_script_url,
      std::unique_ptr<network::SharedURLLoaderFactoryInfo>
          url_loader_factory_info,
      std::unique_ptr<network::SharedURLLoaderFactoryInfo>
          script_loader_factory_info,
      const GURL& script_url_to_skip_throttling,
      std::unique_ptr<URLLoaderThrottleProvider> throttle_provider,
      std::unique_ptr<WebSocketHandshakeThrottleProvider>
          websocket_handshake_throttle_provider,
      mojo::PendingReceiver<blink::mojom::RendererPreferenceWatcher>
          preference_watcher_receiver,
      mojo::PendingReceiver<blink::mojom::SubresourceLoaderUpdater>
          pending_subresource_loader_updater,
      int32_t service_worker_route_id);

  // blink::WebWorkerFetchContext implementation:
  void SetTerminateSyncLoadEvent(base::WaitableEvent*) override;
  void InitializeOnWorkerThread(blink::AcceptLanguagesWatcher*) override;
  blink::WebURLLoaderFactory* GetURLLoaderFactory() override;
  std::unique_ptr<blink::WebURLLoaderFactory> WrapURLLoaderFactory(
      mojo::ScopedMessagePipeHandle url_loader_factory_handle) override;
  blink::WebURLLoaderFactory* GetScriptLoaderFactory() override;
  void WillSendRequest(blink::WebURLRequest&) override;
  blink::mojom::ControllerServiceWorkerMode GetControllerServiceWorkerMode()
      const override;
  blink::WebURL SiteForCookies() const override;
  base::Optional<blink::WebSecurityOrigin> TopFrameOrigin() const override;

  std::unique_ptr<blink::WebSocketHandshakeThrottle>
  CreateWebSocketHandshakeThrottle(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) override;
  blink::WebString GetAcceptLanguages() const override;

 private:
  ~ServiceWorkerFetchContextImpl() override;

  // Implements blink::mojom::ServiceWorkerFetchContext
  void UpdateSubresourceLoaderFactories(
      std::unique_ptr<blink::URLLoaderFactoryBundleInfo>
          subresource_loader_factories) override;

  // Implements blink::mojom::RendererPreferenceWatcher.
  void NotifyUpdate(blink::mojom::RendererPreferencesPtr new_prefs) override;

  blink::mojom::RendererPreferences renderer_preferences_;
  const GURL worker_script_url_;
  // Consumed on the worker thread to create |web_url_loader_factory_|.
  std::unique_ptr<network::SharedURLLoaderFactoryInfo> url_loader_factory_info_;
  // Consumed on the worker thread to create |web_script_loader_factory_|.
  std::unique_ptr<network::SharedURLLoaderFactoryInfo>
      script_loader_factory_info_;

  // A script URL that should skip throttling when loaded because it's already
  // being loaded in the browser process and went through throttles there. It's
  // valid only once and set to invalid GURL once the script is served.
  GURL script_url_to_skip_throttling_;

  // Initialized on the worker thread when InitializeOnWorkerThread() is called.
  std::unique_ptr<ResourceDispatcher> resource_dispatcher_;

  // Responsible for regular loads from the service worker (i.e., Fetch API).
  std::unique_ptr<blink::WebURLLoaderFactory> web_url_loader_factory_;
  // Responsible for script loads from the service worker (i.e., the
  // classic/module main script, module imported scripts, or importScripts()).
  std::unique_ptr<blink::WebURLLoaderFactory> web_script_loader_factory_;

  std::unique_ptr<URLLoaderThrottleProvider> throttle_provider_;
  std::unique_ptr<WebSocketHandshakeThrottleProvider>
      websocket_handshake_throttle_provider_;

  mojo::Receiver<blink::mojom::RendererPreferenceWatcher>
      preference_watcher_receiver_{this};
  mojo::Receiver<blink::mojom::SubresourceLoaderUpdater>
      subresource_loader_updater_{this};

  // These mojo objects are kept while starting up the worker thread. Valid
  // until InitializeOnWorkerThread().
  mojo::PendingReceiver<blink::mojom::RendererPreferenceWatcher>
      preference_watcher_pending_receiver_;
  mojo::PendingReceiver<blink::mojom::SubresourceLoaderUpdater>
      pending_subresource_loader_updater_;

  // This is owned by ThreadedMessagingProxyBase on the main thread.
  base::WaitableEvent* terminate_sync_load_event_ = nullptr;

  blink::AcceptLanguagesWatcher* accept_languages_watcher_ = nullptr;

  int32_t service_worker_route_id_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_FETCH_CONTEXT_IMPL_H_
