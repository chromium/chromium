// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_LOADER_WEB_WORKER_FETCH_CONTEXT_IMPL_H_
#define CONTENT_RENDERER_LOADER_WEB_WORKER_FETCH_CONTEXT_IMPL_H_

#include <memory>
#include <string>

#include "base/strings/string_piece.h"
#include "base/synchronization/waitable_event.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"
#include "third_party/blink/public/mojom/loader/resource_load_info_notifier.mojom.h"
#include "third_party/blink/public/mojom/renderer_preference_watcher.mojom.h"
#include "third_party/blink/public/mojom/renderer_preferences.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_container.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom-forward.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_provider.mojom.h"
#include "third_party/blink/public/mojom/timing/worker_timing_container.mojom-forward.h"
#include "third_party/blink/public/mojom/worker/subresource_loader_updater.mojom.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_worker_fetch_context.h"
#include "url/gurl.h"

namespace blink {
class WebFrameRequestBlocker;
}  // namespace blink

namespace content {

class ResourceDispatcher;
class ServiceWorkerProviderContext;
class URLLoaderThrottleProvider;
class WebSocketHandshakeThrottleProvider;

// This class is used for fetching resource requests from workers (dedicated
// worker and shared worker). This class is created on the main thread and
// passed to the worker thread. This class is not used for service workers. For
// service workers, ServiceWorkerFetchContextImpl class is used instead.
class CONTENT_EXPORT WebWorkerFetchContextImpl
    : public blink::WebWorkerFetchContext,
      public blink::mojom::SubresourceLoaderUpdater,
      public blink::mojom::ServiceWorkerWorkerClient,
      public blink::mojom::RendererPreferenceWatcher {
 public:
  // Creates a new fetch context for a worker.
  //
  // |provider_context| is used to set up information for using service workers.
  // It can be null if the worker is not allowed to use service workers due to
  // security reasons like sandboxed iframes, insecure origins etc.
  // |pending_loader_factory| is used for regular loading by the worker.
  //
  // If the worker is controlled by a service worker, this class makes another
  // loader factory which sends requests to the service worker, and passes
  // |pending_fallback_factory| to that factory to use for network fallback.
  //
  // |pending_loader_factory| and |pending_fallback_factory| are different
  // because |pending_loader_factory| can possibly include a default factory
  // like AppCache, while |pending_fallback_factory| should not have such a
  // default factory and instead go directly to network for http(s) requests.
  // |pending_fallback_factory| might not be simply the direct network factory,
  // because it might additionally support non-NetworkService schemes (e.g.,
  // chrome-extension://).
  static scoped_refptr<WebWorkerFetchContextImpl> Create(
      ServiceWorkerProviderContext* provider_context,
      blink::mojom::RendererPreferences renderer_preferences,
      mojo::PendingReceiver<blink::mojom::RendererPreferenceWatcher>
          watcher_receiver,
      std::unique_ptr<network::PendingSharedURLLoaderFactory>
          pending_loader_factory,
      std::unique_ptr<network::PendingSharedURLLoaderFactory>
          pending_fallback_factory,
      mojo::PendingReceiver<blink::mojom::SubresourceLoaderUpdater>
          pending_subresource_loader_updater,
      const std::vector<std::string>& cors_exempt_header_list,
      mojo::PendingRemote<blink::mojom::ResourceLoadInfoNotifier>
          pending_resource_load_info_notifier);

  // Clones this fetch context for a nested worker.
  // For non-PlzDedicatedWorker. This will be removed once PlzDedicatedWorker is
  // enabled by default.
  scoped_refptr<WebWorkerFetchContextImpl> CloneForNestedWorkerDeprecated(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  // For PlzDedicatedWorker. The cloned fetch context does not inherit some
  // fields (e.g., ServiceWorkerProviderContext) from this fetch context, and
  // instead that takes values passed from the browser process.
  scoped_refptr<WebWorkerFetchContextImpl> CloneForNestedWorker(
      ServiceWorkerProviderContext* service_worker_provider_context,
      std::unique_ptr<network::PendingSharedURLLoaderFactory>
          pending_loader_factory,
      std::unique_ptr<network::PendingSharedURLLoaderFactory>
          pending_fallback_factory,
      mojo::PendingReceiver<blink::mojom::SubresourceLoaderUpdater>
          pending_subresource_loader_updater,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // blink::WebWorkerFetchContext implementation:
  void SetTerminateSyncLoadEvent(base::WaitableEvent*) override;
  void InitializeOnWorkerThread(blink::AcceptLanguagesWatcher*) override;
  blink::WebURLLoaderFactory* GetURLLoaderFactory() override;
  std::unique_ptr<blink::WebURLLoaderFactory> WrapURLLoaderFactory(
      blink::CrossVariantMojoRemote<
          network::mojom::URLLoaderFactoryInterfaceBase> url_loader_factory)
      override;
  std::unique_ptr<blink::WebCodeCacheLoader> CreateCodeCacheLoader() override;
  void WillSendRequest(blink::WebURLRequest&) override;
  blink::mojom::ControllerServiceWorkerMode GetControllerServiceWorkerMode()
      const override;
  void SetIsOnSubframe(bool) override;
  bool IsOnSubframe() const override;
  net::SiteForCookies SiteForCookies() const override;
  base::Optional<blink::WebSecurityOrigin> TopFrameOrigin() const override;
  void SetSubresourceFilterBuilder(
      std::unique_ptr<blink::WebDocumentSubresourceFilter::Builder>) override;
  std::unique_ptr<blink::WebDocumentSubresourceFilter> TakeSubresourceFilter()
      override;
  std::unique_ptr<blink::WebSocketHandshakeThrottle>
  CreateWebSocketHandshakeThrottle(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) override;
  blink::CrossVariantMojoReceiver<
      blink::mojom::WorkerTimingContainerInterfaceBase>
  TakePendingWorkerTimingReceiver(int request_id) override;
  void SetIsOfflineMode(bool is_offline_mode) override;

  // blink::mojom::ServiceWorkerWorkerClient implementation:
  void OnControllerChanged(blink::mojom::ControllerServiceWorkerMode) override;

  // Sets the controller service worker mode.
  // - For dedicated workers (non-PlzDedicatedWorker), they depend on the
  //   controller of the ancestor frame (directly for non-nested workers, or
  //   indirectly via its parent worker for nested workers), and inherit its
  //   controller mode.
  // - For dedicated workers (PlzDedicatedWorker) and shared workers, the
  //   controller mode is passed from the browser processw when starting the
  //   worker.
  void set_controller_service_worker_mode(
      blink::mojom::ControllerServiceWorkerMode mode);

  // Sets properties associated with frames.
  // - For dedicated workers, the property is copied from the ancestor frame
  //   (directly for non-nested workers, or indirectly via its parent worker for
  //   nested workers).
  // - For shared workers, there is no parent frame, so the default value, or a
  //   value calculated in some way is set.
  //
  // TODO(nhiroki): Add more comments about security/privacy implications to
  // each property, for example, site_for_cookies and top_frame_origin.
  void set_ancestor_frame_id(int id);
  void set_frame_request_blocker(
      scoped_refptr<blink::WebFrameRequestBlocker> frame_request_blocker);
  void set_site_for_cookies(const net::SiteForCookies& site_for_cookies);
  void set_top_frame_origin(const blink::WebSecurityOrigin& top_frame_origin);

  void set_client_id(const std::string& client_id);

  using RewriteURLFunction = blink::WebURL (*)(base::StringPiece, bool);
  static void InstallRewriteURLFunction(RewriteURLFunction rewrite_url);

  blink::WebString GetAcceptLanguages() const override;

  // Sets up |receiver| to receive resource performance timings for the given
  // |request_id|. This receiver will be taken later by
  // TakePendingWorkerTimingReceiver().
  void AddPendingWorkerTimingReceiver(
      int request_id,
      mojo::PendingReceiver<blink::mojom::WorkerTimingContainer> receiver);

  blink::CrossVariantMojoRemote<
      blink::mojom::ResourceLoadInfoNotifierInterfaceBase>
  CloneResourceLoadInfoNotifier() override;

 private:
  class Factory;
  using WorkerTimingContainerReceiverMap =
      std::map<int /* request_id */,
               mojo::PendingReceiver<blink::mojom::WorkerTimingContainer>>;

  // - |service_worker_client_receiver| receives OnControllerChanged()
  //   notifications.
  // - |service_worker_worker_client_registry| is used to register new
  //   ServiceWorkerWorkerClients, which is needed when creating a
  //   nested worker.
  //
  // Regarding the rest of params, see the comments on Create().
  WebWorkerFetchContextImpl(
      blink::mojom::RendererPreferences renderer_preferences,
      mojo::PendingReceiver<blink::mojom::RendererPreferenceWatcher>
          watcher_receiver,
      mojo::PendingReceiver<blink::mojom::ServiceWorkerWorkerClient>
          service_worker_client_receiver,
      mojo::PendingRemote<blink::mojom::ServiceWorkerWorkerClientRegistry>
          service_worker_worker_client_registry,
      mojo::PendingRemote<blink::mojom::ServiceWorkerContainerHost>
          service_worker_container_host,
      std::unique_ptr<network::PendingSharedURLLoaderFactory>
          pending_loader_factory,
      std::unique_ptr<network::PendingSharedURLLoaderFactory>
          pending_fallback_factory,
      mojo::PendingReceiver<blink::mojom::SubresourceLoaderUpdater>
          pending_subresource_loader_updater,
      std::unique_ptr<URLLoaderThrottleProvider> throttle_provider,
      std::unique_ptr<WebSocketHandshakeThrottleProvider>
          websocket_handshake_throttle_provider,
      const std::vector<std::string>& cors_exempt_header_list,
      mojo::PendingRemote<blink::mojom::ResourceLoadInfoNotifier>
          pending_resource_load_info_notifier);

  ~WebWorkerFetchContextImpl() override;

  scoped_refptr<WebWorkerFetchContextImpl> CloneForNestedWorkerInternal(
      mojo::PendingReceiver<blink::mojom::ServiceWorkerWorkerClient>
          service_worker_client_receiver,
      mojo::PendingRemote<blink::mojom::ServiceWorkerWorkerClientRegistry>
          service_worker_worker_client_registry,
      mojo::PendingRemote<blink::mojom::ServiceWorkerContainerHost>
          service_worker_container_host,
      std::unique_ptr<network::PendingSharedURLLoaderFactory>
          pending_loader_factory,
      std::unique_ptr<network::PendingSharedURLLoaderFactory>
          pending_fallback_factory,
      mojo::PendingReceiver<blink::mojom::SubresourceLoaderUpdater>
          pending_subresource_loader_updater,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // Resets the service worker url loader factory of a URLLoaderFactoryImpl
  // which was passed to Blink. The url loader factory is connected to the
  // controller service worker. Sets nullptr if the worker context is not
  // controlled by a service worker.
  void ResetServiceWorkerURLLoaderFactory();

  // Implements blink::mojom::SubresourceLoaderUpdater.
  void UpdateSubresourceLoaderFactories(
      std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
          subresource_loader_factories) override;

  // Implements blink::mojom::RendererPreferenceWatcher.
  void NotifyUpdate(blink::mojom::RendererPreferencesPtr new_prefs) override;

  // |receiver_| and |service_worker_worker_client_registry_| may be null if
  // this context can't use service workers. See comments for Create().
  mojo::Receiver<blink::mojom::ServiceWorkerWorkerClient> receiver_{this};
  mojo::Remote<blink::mojom::ServiceWorkerWorkerClientRegistry>
      service_worker_worker_client_registry_;

  // Bound to |this| on the worker thread.
  mojo::PendingReceiver<blink::mojom::ServiceWorkerWorkerClient>
      service_worker_client_receiver_;
  // Consumed on the worker thread to create
  // |service_worker_worker_client_registry_|.
  mojo::PendingRemote<blink::mojom::ServiceWorkerWorkerClientRegistry>
      pending_service_worker_worker_client_registry_;
  // Consumed on the worker thread to create |service_worker_container_host_|.
  mojo::PendingRemote<blink::mojom::ServiceWorkerContainerHost>
      pending_service_worker_container_host_;
  // Consumed on the worker thread to create |loader_factory_|.
  std::unique_ptr<network::PendingSharedURLLoaderFactory>
      pending_loader_factory_;
  // Consumed on the worker thread to create |fallback_factory_|.
  std::unique_ptr<network::PendingSharedURLLoaderFactory>
      pending_fallback_factory_;

  blink::mojom::ControllerServiceWorkerMode controller_service_worker_mode_ =
      blink::mojom::ControllerServiceWorkerMode::kNoController;

  // Initialized on the worker thread when InitializeOnWorkerThread() is called.
  // This can be null if the |provider_context| passed to Create() was null or
  // already being destructed (see
  // ServiceWorkerProviderContext::OnNetworkProviderDestroyed()).
  mojo::Remote<blink::mojom::ServiceWorkerContainerHost>
      service_worker_container_host_;

  // The Client#id value of the shared worker or dedicated worker (since
  // dedicated workers are not yet service worker clients, it is the parent
  // document's id in that case). Passed to ControllerServiceWorkerConnector.
  std::string client_id_;

  // Initialized on the worker thread when InitializeOnWorkerThread() is called.
  std::unique_ptr<ResourceDispatcher> resource_dispatcher_;

  // Initialized on the worker thread when InitializeOnWorkerThread() is called.
  // |loader_factory_| is used for regular loading by the worker. In
  // If the worker is controlled by a service worker, it creates a
  // ServiceWorkerSubresourceLoaderFactory instead.
  scoped_refptr<network::SharedURLLoaderFactory> loader_factory_;

  // Initialized on the worker thread when InitializeOnWorkerThread() is called.
  // If the worker is controlled by a service worker, it passes this factory to
  // ServiceWorkerSubresourceLoaderFactory to use for network fallback.
  scoped_refptr<network::SharedURLLoaderFactory> fallback_factory_;

  // Initialized on the worker thread when InitializeOnWorkerThread() is called.
  // Used to reconnect to the Network Service after the Network Service crash.
  // This is only used for dedicated workers when PlzDedicatedWorker is enabled.
  // When PlzDedicatedWorker is disabled, the ancestor render frame updates the
  // loaders via Host/TrackedChildURLLoaderFactoryBundle. For shared workers,
  // the renderer process detects the crash, and terminates the worker instead
  // of recovery.
  mojo::PendingReceiver<blink::mojom::SubresourceLoaderUpdater>
      pending_subresource_loader_updater_;
  mojo::Receiver<blink::mojom::SubresourceLoaderUpdater>
      subresource_loader_updater_{this};

  std::unique_ptr<blink::WebDocumentSubresourceFilter::Builder>
      subresource_filter_builder_;
  // For dedicated workers, this is the ancestor frame (the parent frame for
  // non-nested workers, the closest ancestor for nested workers). For shared
  // workers, this is the shadow page.
  bool is_on_sub_frame_ = false;
  int ancestor_frame_id_ = MSG_ROUTING_NONE;
  // Set to non-null if the ancestor frame has an associated RequestBlocker,
  // which blocks requests from this worker too when the ancestor frame is
  // blocked.
  scoped_refptr<blink::WebFrameRequestBlocker> frame_request_blocker_;
  net::SiteForCookies site_for_cookies_;
  base::Optional<url::Origin> top_frame_origin_;

  blink::mojom::RendererPreferences renderer_preferences_;

  // |preference_watcher_receiver_| and |child_preference_watchers_| are for
  // keeping track of updates in the renderer preferences.
  mojo::Receiver<blink::mojom::RendererPreferenceWatcher>
      preference_watcher_receiver_{this};
  // Kept while staring up the worker thread. Valid until
  // InitializeOnWorkerThread().
  mojo::PendingReceiver<blink::mojom::RendererPreferenceWatcher>
      preference_watcher_pending_receiver_;
  mojo::RemoteSet<blink::mojom::RendererPreferenceWatcher>
      child_preference_watchers_;

  // This is owned by ThreadedMessagingProxyBase on the main thread.
  base::WaitableEvent* terminate_sync_load_event_ = nullptr;

  // The blink::WebURLLoaderFactory which was created and passed to
  // Blink by GetURLLoaderFactory().
  std::unique_ptr<Factory> web_loader_factory_;

  std::unique_ptr<URLLoaderThrottleProvider> throttle_provider_;
  std::unique_ptr<WebSocketHandshakeThrottleProvider>
      websocket_handshake_throttle_provider_;

  std::vector<std::string> cors_exempt_header_list_;

  mojo::PendingRemote<blink::mojom::ResourceLoadInfoNotifier>
      pending_resource_load_info_notifier_;

  // Used to send the ResourceLoadInfo of the main script for dedicated
  // workers only when PlzDedicatedWorker is enabled.
  mojo::Remote<blink::mojom::ResourceLoadInfoNotifier>
      resource_load_info_notifier_;

  blink::AcceptLanguagesWatcher* accept_languages_watcher_ = nullptr;

  // Contains pending receivers whose corresponding requests are still
  // in-flight. The pending receivers are taken by
  // TakePendingWorkerTimingReceiver() when the request is completed.
  WorkerTimingContainerReceiverMap worker_timing_container_receivers_;

  base::WeakPtrFactory<WebWorkerFetchContextImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_RENDERER_LOADER_WEB_WORKER_FETCH_CONTEXT_IMPL_H_
