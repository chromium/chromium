// Copyright 2017 The Chromium Authors. All rights reserved.

// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_WORKER_FETCH_CONTEXT_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_WORKER_FETCH_CONTEXT_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "services/network/public/mojom/url_loader_factory.mojom-shared.h"
#include "third_party/blink/public/mojom/loader/resource_load_info_notifier.mojom-shared.h"
#include "third_party/blink/public/mojom/service_worker/controller_service_worker_mode.mojom-shared.h"
#include "third_party/blink/public/mojom/timing/worker_timing_container.mojom-shared.h"
#include "third_party/blink/public/platform/cross_variant_mojo_util.h"
#include "third_party/blink/public/platform/resource_load_info_notifier_wrapper.h"
#include "third_party/blink/public/platform/web_code_cache_loader.h"
#include "third_party/blink/public/platform/web_document_subresource_filter.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_loader_factory.h"
#include "third_party/blink/public/platform/websocket_handshake_throttle.h"

namespace base {
class WaitableEvent;
}  // namespace base

namespace net {
class SiteForCookies;
}  // namespace net

namespace blink {

class WebURLRequest;
class WebDocumentSubresourceFilter;

// Helper class allowing WebWorkerFetchContextImpl to notify blink upon an
// accept languages update. This class will be extended by WorkerNavigator.
class AcceptLanguagesWatcher {
 public:
  virtual void NotifyUpdate() = 0;
};

// WebWorkerFetchContext is a per-worker object created on the main thread,
// passed to a worker (dedicated, shared and service worker) and initialized on
// the worker thread by InitializeOnWorkerThread(). It contains information
// about the resource fetching context (ex: service worker provider id), and is
// used to create a new WebURLLoader instance in the worker thread.
//
// A single WebWorkerFetchContext is used for both worker
// subresource fetch (i.e. "insideSettings") and off-the-main-thread top-level
// worker script fetch (i.e. fetch as "outsideSettings"), as they both should be
// e.g. controlled by the same ServiceWorker (if any) and thus can share a
// single WebURLLoaderFactory.
//
// Note that WebWorkerFetchContext and WorkerFetchContext do NOT correspond 1:1
// as multiple WorkerFetchContext can be created after crbug.com/880027.
class WebWorkerFetchContext : public base::RefCounted<WebWorkerFetchContext> {
 public:
  REQUIRE_ADOPTION_FOR_REFCOUNTED_TYPE();

  virtual ~WebWorkerFetchContext() = default;

  // Set a raw pointer of a WaitableEvent which will be signaled from the main
  // thread when the worker's GlobalScope is terminated, which will terminate
  // sync loading requests on the worker thread. It is guaranteed that the
  // pointer is valid throughout the lifetime of this context.
  virtual void SetTerminateSyncLoadEvent(base::WaitableEvent*) = 0;

  virtual void InitializeOnWorkerThread(AcceptLanguagesWatcher*) = 0;

  // Returns a WebURLLoaderFactory which is associated with the worker context.
  // The returned WebURLLoaderFactory is owned by |this|.
  virtual WebURLLoaderFactory* GetURLLoaderFactory() = 0;

  // Returns a new WebURLLoaderFactory that wraps the given
  // network::mojom::URLLoaderFactory.
  virtual std::unique_ptr<WebURLLoaderFactory> WrapURLLoaderFactory(
      CrossVariantMojoRemote<network::mojom::URLLoaderFactoryInterfaceBase>
          url_loader_factory) = 0;

  // Returns a WebCodeCacheLoader that fetches data from code caches. If
  // a nullptr is returned then data would not be fetched from the code
  // cache.
  virtual std::unique_ptr<WebCodeCacheLoader> CreateCodeCacheLoader() {
    return nullptr;
  }

  // Returns a WebURLLoaderFactory for loading scripts in this worker context.
  // Unlike GetURLLoaderFactory(), this may return nullptr.
  // The returned WebURLLoaderFactory is owned by |this|.
  virtual WebURLLoaderFactory* GetScriptLoaderFactory() { return nullptr; }

  // Called when a request is about to be sent out to modify the request to
  // handle the request correctly in the loading stack later. (Example: service
  // worker)
  virtual void WillSendRequest(WebURLRequest&) = 0;

  // Returns whether a controller service worker exists and if it has fetch
  // handler.
  virtual blink::mojom::ControllerServiceWorkerMode
  GetControllerServiceWorkerMode() const = 0;

  // This flag is used to block all mixed content in subframes.
  virtual void SetIsOnSubframe(bool) {}
  virtual bool IsOnSubframe() const { return false; }

  // Will be consulted for the third-party cookie blocking policy, as defined in
  // Section 2.1.1 and 2.1.2 of
  // https://tools.ietf.org/html/draft-ietf-httpbis-cookie-same-site.
  // See content::URLRequest::site_for_cookies() for details.
  virtual net::SiteForCookies SiteForCookies() const = 0;

  // The top-frame-origin for the worker. For a dedicated worker this is the
  // top-frame origin of the page that created the worker. For a shared worker
  // or a service worker this is unset.
  virtual base::Optional<WebSecurityOrigin> TopFrameOrigin() const = 0;

  // Sets the builder object of WebDocumentSubresourceFilter on the main thread
  // which will be used in TakeSubresourceFilter() to create a
  // WebDocumentSubresourceFilter on the worker thread.
  virtual void SetSubresourceFilterBuilder(
      std::unique_ptr<WebDocumentSubresourceFilter::Builder>) {}

  // Creates a WebDocumentSubresourceFilter on the worker thread using the
  // WebDocumentSubresourceFilter::Builder which is set on the main thread.
  // This method should only be called once.
  virtual std::unique_ptr<WebDocumentSubresourceFilter>
  TakeSubresourceFilter() {
    return nullptr;
  }

  // Creates a WebSocketHandshakeThrottle on the worker thread. |task_runner| is
  // used for internal IPC handling of the throttle, and must be bound to the
  // same sequence to the current one (which is the worker thread).
  virtual std::unique_ptr<blink::WebSocketHandshakeThrottle>
  CreateWebSocketHandshakeThrottle(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
    return nullptr;
  }

  // Returns the current list of user prefered languages.
  virtual blink::WebString GetAcceptLanguages() const = 0;

  // Returns the blink::mojom::WorkerTimingContainer receiver for the
  // blink::ResourceResponse with the given |request_id|. Null if the
  // request has not been intercepted by a service worker.
  virtual CrossVariantMojoReceiver<mojom::WorkerTimingContainerInterfaceBase>
  TakePendingWorkerTimingReceiver(int request_id) = 0;

  // This flag is set to disallow all network accesses in the context. Used for
  // offline capability detection in service workers.
  virtual void SetIsOfflineMode(bool is_offline_mode) = 0;

  // Clones a valid notifier held by this context which is used to notify
  // loading status only when
  // IsLoadMainScriptForPlzDedicatedWorkerByParamsEnabled() is true.
  virtual CrossVariantMojoRemote<mojom::ResourceLoadInfoNotifierInterfaceBase>
  CloneResourceLoadInfoNotifier() {
    return CrossVariantMojoRemote<mojom::ResourceLoadInfoNotifierInterfaceBase>(
        mojo::NullRemote());
  }

  // Creates a notifier used to notify loading stats for workers.
  virtual std::unique_ptr<blink::ResourceLoadInfoNotifierWrapper>
  CreateResourceLoadInfoNotifierWrapper() {
    return std::make_unique<blink::ResourceLoadInfoNotifierWrapper>(
        /*resource_load_info_notifier=*/nullptr);
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_WORKER_FETCH_CONTEXT_H_
