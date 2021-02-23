// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_FETCH_CONTEXT_IMPL_H_
#define CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_FETCH_CONTEXT_IMPL_H_

#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/blink/public/common/renderer_preferences/renderer_preferences.h"
#include "third_party/blink/public/mojom/renderer_preference_watcher.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker.mojom-forward.h"
#include "third_party/blink/public/mojom/worker/subresource_loader_updater.mojom.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_fetch_context.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "url/gurl.h"

namespace blink {
class InternetDisconnectedWebURLLoaderFactory;
}

namespace content {
class URLLoaderThrottleProvider;
class WebSocketHandshakeThrottleProvider;

class CONTENT_EXPORT ServiceWorkerFetchContextImpl final
    : public blink::WebServiceWorkerFetchContext,
      public blink::mojom::RendererPreferenceWatcher,
      public blink::mojom::SubresourceLoaderUpdater {
 public:
  // |pending_url_loader_factory| is used for regular loads from the service
  // worker (i.e., Fetch API). It typically goes to network, but it might
  // internally contain non-NetworkService factories for handling non-http(s)
  // URLs like chrome-extension://. |pending_script_loader_factory| is used for
  // importScripts() from the service worker when InstalledScriptsManager
  // doesn't have the requested script. It is a
  // ServiceWorkerScriptLoaderFactory, which loads and installs the script.
  // |script_url_to_skip_throttling| is a URL which is already throttled in the
  // browser process so that it doesn't need to be throttled in the renderer
  // again.
  ServiceWorkerFetchContextImpl(
      const blink::RendererPreferences& renderer_preferences,
      const GURL& worker_script_url,
      std::unique_ptr<network::PendingSharedURLLoaderFactory>
          pending_url_loader_factory,
      std::unique_ptr<network::PendingSharedURLLoaderFactory>
          pending_script_loader_factory,
      const GURL& script_url_to_skip_throttling,
      std::unique_ptr<URLLoaderThrottleProvider> throttle_provider,
      std::unique_ptr<WebSocketHandshakeThrottleProvider>
          websocket_handshake_throttle_provider,
      mojo::PendingReceiver<blink::mojom::RendererPreferenceWatcher>
          preference_watcher_receiver,
      mojo::PendingReceiver<blink::mojom::SubresourceLoaderUpdater>
          pending_subresource_loader_updater,
      int32_t service_worker_route_id,
      const std::vector<std::string>& cors_exempt_header_list);

  // blink::WebServiceWorkerFetchContext implementation:
  void SetTerminateSyncLoadEvent(base::WaitableEvent*) override;
  void InitializeOnWorkerThread(blink::AcceptLanguagesWatcher*) override;
  blink::WebURLLoaderFactory* GetURLLoaderFactory() override;
  std::unique_ptr<blink::WebURLLoaderFactory> WrapURLLoaderFactory(
      blink::CrossVariantMojoRemote<
          network::mojom::URLLoaderFactoryInterfaceBase> url_loader_factory)
      override;
  blink::WebURLLoaderFactory* GetScriptLoaderFactory() override;
  void WillSendRequest(blink::WebURLRequest&) override;
  blink::mojom::ControllerServiceWorkerMode GetControllerServiceWorkerMode()
      const override;
  net::SiteForCookies SiteForCookies() const override;
  base::Optional<blink::WebSecurityOrigin> TopFrameOrigin() const override;
  std::unique_ptr<blink::WebSocketHandshakeThrottle>
  CreateWebSocketHandshakeThrottle(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) override;
  blink::WebString GetAcceptLanguages() const override;
  blink::CrossVariantMojoReceiver<
      blink::mojom::WorkerTimingContainerInterfaceBase>
  TakePendingWorkerTimingReceiver(int request_id) override;
  void SetIsOfflineMode(bool) override;
  blink::mojom::SubresourceLoaderUpdater* GetSubresourceLoaderUpdater()
      override;

  // blink::mojom::SubresourceLoaderUpdater implementation:
  void UpdateSubresourceLoaderFactories(
      std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
          subresource_loader_factories) override;

 private:
  ~ServiceWorkerFetchContextImpl() override;

  // Implements blink::mojom::RendererPreferenceWatcher.
  void NotifyUpdate(const blink::RendererPreferences& new_prefs) override;

  blink::WebVector<blink::WebString> cors_exempt_header_list();

  blink::RendererPreferences renderer_preferences_;
  const GURL worker_script_url_;
  // Consumed on the worker thread to create |web_url_loader_factory_|.
  std::unique_ptr<network::PendingSharedURLLoaderFactory>
      pending_url_loader_factory_;
  // Consumed on the worker thread to create |web_script_loader_factory_|.
  std::unique_ptr<network::PendingSharedURLLoaderFactory>
      pending_script_loader_factory_;

  // A script URL that should skip throttling when loaded because it's already
  // being loaded in the browser process and went through throttles there. It's
  // valid only once and set to invalid GURL once the script is served.
  GURL script_url_to_skip_throttling_;

  // Responsible for regular loads from the service worker (i.e., Fetch API).
  std::unique_ptr<blink::WebURLLoaderFactory> web_url_loader_factory_;
  // Responsible for loads which always fail as INTERNET_DISCONNECTED
  // error, which is used in offline mode.
  std::unique_ptr<blink::InternetDisconnectedWebURLLoaderFactory>
      internet_disconnected_web_url_loader_factory_;
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
  std::vector<std::string> cors_exempt_header_list_;
  bool is_offline_mode_ = false;
};

}  // namespace content

#endif  // CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_FETCH_CONTEXT_IMPL_H_
