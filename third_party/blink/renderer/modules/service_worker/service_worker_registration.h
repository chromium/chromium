// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SERVICE_WORKER_SERVICE_WORKER_REGISTRATION_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SERVICE_WORKER_SERVICE_WORKER_REGISTRATION_H_

#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom-blink.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_registration_object_info.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/service_worker/navigation_preload_manager.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker.h"
#include "third_party/blink/renderer/platform/forward_declared_member.h"
#include "third_party/blink/renderer/platform/heap/prefinalizer.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_associated_receiver.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_associated_remote.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/gc_plugin.h"

namespace blink {

class ExceptionState;
class ScriptState;
class V8ServiceWorkerUpdateViaCache;
class BackgroundFetchBridge;
class CookieStoreManager;
class PaymentAppServiceWorkerRegistration;
class PushMessagingBridge;
class PushProvider;
class ServiceWorkerRegistrationBackgroundFetch;
class ServiceWorkerRegistrationContentIndex;
class ServiceWorkerRegistrationNotifications;
class ServiceWorkerRegistrationPush;
class ServiceWorkerRegistrationSync;

// The implementation of a service worker registration object in Blink.
class ServiceWorkerRegistration final
    : public EventTarget,
      public ActiveScriptWrappable<ServiceWorkerRegistration>,
      public ExecutionContextLifecycleObserver,
      public mojom::blink::ServiceWorkerRegistrationObject {
  DEFINE_WRAPPERTYPEINFO();
  USING_PRE_FINALIZER(ServiceWorkerRegistration, Dispose);

 public:
  ServiceWorkerRegistration(ExecutionContext*,
                            WebServiceWorkerRegistrationObjectInfo);

  ServiceWorkerRegistration(
      ExecutionContext*,
      mojom::blink::ServiceWorkerRegistrationObjectInfoPtr);

  // Called in 2 scenarios:
  //   - when constructing |this|.
  //   - when the browser process sends a new
  //   WebServiceWorkerRegistrationObjectInfo and |this| already exists for the
  //   described ServiceWorkerRegistration, the new info may contain some
  //   information to be updated, e.g. {installing,waiting,active} objects.
  void Attach(WebServiceWorkerRegistrationObjectInfo);

  // ScriptWrappable overrides.
  bool HasPendingActivity() const final;

  // EventTarget overrides.
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override {
    return ExecutionContextLifecycleObserver::GetExecutionContext();
  }

  ServiceWorker* installing() { return installing_.Get(); }
  ServiceWorker* waiting() { return waiting_.Get(); }
  ServiceWorker* active() { return active_.Get(); }
  NavigationPreloadManager* navigationPreload();

  String scope() const;
  V8ServiceWorkerUpdateViaCache updateViaCache() const;

  int64_t RegistrationId() const { return registration_id_; }

  void EnableNavigationPreload(bool enable,
                               ScriptPromiseResolver<IDLUndefined>* resolver);
  void GetNavigationPreloadState(
      ScriptPromiseResolver<NavigationPreloadState>* resolver);
  void SetNavigationPreloadHeader(
      const String& value,
      ScriptPromiseResolver<IDLUndefined>* resolver);

  ScriptPromise<ServiceWorkerRegistration> update(ScriptState*,
                                                  ExceptionState&);
  ScriptPromise<IDLBoolean> unregister(ScriptState*, ExceptionState&);

  DEFINE_ATTRIBUTE_EVENT_LISTENER(updatefound, kUpdatefound)

  ~ServiceWorkerRegistration() override;

  void Dispose();

  void Trace(Visitor*) const override;

  ForwardDeclaredMember<BackgroundFetchBridge> GetBackgroundFetchBridge()
      const {
    return background_fetch_bridge_;
  }
  void SetBackgroundFetchBridge(
      ForwardDeclaredMember<BackgroundFetchBridge> background_fetch_bridge) {
    background_fetch_bridge_ = background_fetch_bridge;
  }

  ForwardDeclaredMember<CookieStoreManager> GetCookieStoreManager() const {
    return cookie_store_manager_;
  }
  void SetCookieStoreManager(
      ForwardDeclaredMember<CookieStoreManager> cookie_store_manager) {
    cookie_store_manager_ = cookie_store_manager;
  }

  ForwardDeclaredMember<PaymentAppServiceWorkerRegistration>
  GetPaymentAppServiceWorkerRegistration() const {
    return payment_app_service_worker_registration_;
  }
  void SetPaymentAppServiceWorkerRegistration(
      ForwardDeclaredMember<PaymentAppServiceWorkerRegistration>
          payment_app_service_worker_registration) {
    payment_app_service_worker_registration_ =
        payment_app_service_worker_registration;
  }

  ForwardDeclaredMember<PushMessagingBridge> GetPushMessagingBridge() const {
    return push_messaging_bridge_;
  }
  void SetPushMessagingBridge(
      ForwardDeclaredMember<PushMessagingBridge> push_messaging_bridge) {
    push_messaging_bridge_ = push_messaging_bridge;
  }

  ForwardDeclaredMember<PushProvider> GetPushProvider() const {
    return push_provider_;
  }
  void SetPushProvider(ForwardDeclaredMember<PushProvider> push_provider) {
    push_provider_ = push_provider;
  }

  ForwardDeclaredMember<ServiceWorkerRegistrationBackgroundFetch>
  GetServiceWorkerRegistrationBackgroundFetch() const {
    return service_worker_registration_background_fetch_;
  }
  void SetServiceWorkerRegistrationBackgroundFetch(
      ForwardDeclaredMember<ServiceWorkerRegistrationBackgroundFetch>
          service_worker_registration_background_fetch) {
    service_worker_registration_background_fetch_ =
        service_worker_registration_background_fetch;
  }

  ForwardDeclaredMember<ServiceWorkerRegistrationContentIndex>
  GetServiceWorkerRegistrationContentIndex() const {
    return service_worker_registration_content_index_;
  }
  void SetServiceWorkerRegistrationContentIndex(
      ForwardDeclaredMember<ServiceWorkerRegistrationContentIndex>
          service_worker_registration_content_index) {
    service_worker_registration_content_index_ =
        service_worker_registration_content_index;
  }

  ForwardDeclaredMember<ServiceWorkerRegistrationNotifications>
  GetServiceWorkerRegistrationNotifications() const {
    return service_worker_registration_notifications_;
  }
  void SetServiceWorkerRegistrationNotifications(
      ForwardDeclaredMember<ServiceWorkerRegistrationNotifications>
          service_worker_registration_notifications) {
    service_worker_registration_notifications_ =
        service_worker_registration_notifications;
  }

  ForwardDeclaredMember<ServiceWorkerRegistrationPush>
  GetServiceWorkerRegistrationPush() const {
    return service_worker_registration_push_;
  }
  void SetServiceWorkerRegistrationPush(
      ForwardDeclaredMember<ServiceWorkerRegistrationPush>
          service_worker_registration_push) {
    service_worker_registration_push_ = service_worker_registration_push;
  }

  ForwardDeclaredMember<ServiceWorkerRegistrationSync>
  GetServiceWorkerRegistrationSync() const {
    return service_worker_registration_sync_;
  }
  void SetServiceWorkerRegistrationSync(
      ForwardDeclaredMember<ServiceWorkerRegistrationSync>
          service_worker_registration_sync) {
    service_worker_registration_sync_ = service_worker_registration_sync;
  }

 private:
  // ExecutionContextLifecycleObserver overrides.
  void ContextDestroyed() override;

  // Implements mojom::blink::ServiceWorkerRegistrationObject.
  void SetServiceWorkerObjects(
      mojom::blink::ChangedServiceWorkerObjectsMaskPtr changed_mask,
      mojom::blink::ServiceWorkerObjectInfoPtr installing,
      mojom::blink::ServiceWorkerObjectInfoPtr waiting,
      mojom::blink::ServiceWorkerObjectInfoPtr active) override;
  void SetUpdateViaCache(
      mojom::blink::ServiceWorkerUpdateViaCache update_via_cache) override;
  void UpdateFound() override;

  void UpdateInternal(
      mojom::blink::FetchClientSettingsObjectPtr mojom_settings_object,
      ScriptPromiseResolver<ServiceWorkerRegistration>* resolver);
  void UnregisterInternal(ScriptPromiseResolver<IDLBoolean>* resolver);

  Member<ServiceWorker> installing_;
  Member<ServiceWorker> waiting_;
  Member<ServiceWorker> active_;
  Member<NavigationPreloadManager> navigation_preload_;

  ForwardDeclaredMember<BackgroundFetchBridge> background_fetch_bridge_;
  ForwardDeclaredMember<CookieStoreManager> cookie_store_manager_;
  ForwardDeclaredMember<PaymentAppServiceWorkerRegistration>
      payment_app_service_worker_registration_;
  ForwardDeclaredMember<PushMessagingBridge> push_messaging_bridge_;
  ForwardDeclaredMember<PushProvider> push_provider_;
  ForwardDeclaredMember<ServiceWorkerRegistrationBackgroundFetch>
      service_worker_registration_background_fetch_;
  ForwardDeclaredMember<ServiceWorkerRegistrationContentIndex>
      service_worker_registration_content_index_;
  ForwardDeclaredMember<ServiceWorkerRegistrationNotifications>
      service_worker_registration_notifications_;
  ForwardDeclaredMember<ServiceWorkerRegistrationPush>
      service_worker_registration_push_;
  ForwardDeclaredMember<ServiceWorkerRegistrationSync>
      service_worker_registration_sync_;

  const int64_t registration_id_;
  const KURL scope_;
  mojom::ServiceWorkerUpdateViaCache update_via_cache_;
  // Both |host_| and |receiver_| are associated with
  // blink.mojom.ServiceWorkerContainer interface for a Document, and
  // blink.mojom.ServiceWorker interface for a ServiceWorkerGlobalScope.
  //
  // |host_| keeps the Mojo connection to the
  // browser-side ServiceWorkerRegistrationObjectHost, whose lifetime is bound
  // to the Mojo connection. It is bound on the
  // main thread for service worker clients (document), and is bound on the
  // service worker thread for service worker execution contexts.
  HeapMojoAssociatedRemote<mojom::blink::ServiceWorkerRegistrationObjectHost>
      host_;
  // |receiver_| receives messages from the ServiceWorkerRegistrationObjectHost
  // in the browser process. It is bound on the main thread for service worker
  // clients (document), and is bound on the service worker thread for service
  // worker execution contexts.
  HeapMojoAssociatedReceiver<mojom::blink::ServiceWorkerRegistrationObject,
                             ServiceWorkerRegistration>
      receiver_;

  bool stopped_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SERVICE_WORKER_SERVICE_WORKER_REGISTRATION_H_
