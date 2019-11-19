// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SERVICE_WORKER_SERVICE_WORKER_REGISTRATION_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SERVICE_WORKER_SERVICE_WORKER_REGISTRATION_H_

#include <memory>
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom-blink.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_registration_object_info.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/service_worker/navigation_preload_manager.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class ScriptPromise;
class ScriptState;

// The implementation of a service worker registration object in Blink.
class ServiceWorkerRegistration final
    : public EventTargetWithInlineData,
      public ActiveScriptWrappable<ServiceWorkerRegistration>,
      public ContextLifecycleObserver,
      public Supplementable<ServiceWorkerRegistration>,
      public mojom::blink::ServiceWorkerRegistrationObject {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(ServiceWorkerRegistration);
  USING_PRE_FINALIZER(ServiceWorkerRegistration, Dispose);

 public:
  // Called from CallbackPromiseAdapter.
  using WebType = WebServiceWorkerRegistrationObjectInfo;
  static ServiceWorkerRegistration* Take(
      ScriptPromiseResolver*,
      WebServiceWorkerRegistrationObjectInfo);

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
    return ContextLifecycleObserver::GetExecutionContext();
  }

  ServiceWorker* installing() { return installing_; }
  ServiceWorker* waiting() { return waiting_; }
  ServiceWorker* active() { return active_; }
  NavigationPreloadManager* navigationPreload();

  String scope() const;
  String updateViaCache() const;

  int64_t RegistrationId() const { return registration_id_; }

  void EnableNavigationPreload(bool enable, ScriptPromiseResolver* resolver);
  void GetNavigationPreloadState(ScriptPromiseResolver* resolver);
  void SetNavigationPreloadHeader(const String& value,
                                  ScriptPromiseResolver* resolver);

  ScriptPromise update(ScriptState*);
  ScriptPromise unregister(ScriptState*);

  DEFINE_ATTRIBUTE_EVENT_LISTENER(updatefound, kUpdatefound)

  ~ServiceWorkerRegistration() override;

  void Dispose();

  void Trace(blink::Visitor*) override;

 private:
  // ContextLifecycleObserver overrides.
  void ContextDestroyed(ExecutionContext*) override;

  // Implements mojom::blink::ServiceWorkerRegistrationObject.
  void SetServiceWorkerObjects(
      mojom::blink::ChangedServiceWorkerObjectsMaskPtr changed_mask,
      mojom::blink::ServiceWorkerObjectInfoPtr installing,
      mojom::blink::ServiceWorkerObjectInfoPtr waiting,
      mojom::blink::ServiceWorkerObjectInfoPtr active) override;
  void SetUpdateViaCache(
      mojom::blink::ServiceWorkerUpdateViaCache update_via_cache) override;
  void UpdateFound() override;

  Member<ServiceWorker> installing_;
  Member<ServiceWorker> waiting_;
  Member<ServiceWorker> active_;
  Member<NavigationPreloadManager> navigation_preload_;

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
  mojo::AssociatedRemote<mojom::blink::ServiceWorkerRegistrationObjectHost>
      host_;
  // |receiver_| receives messages from the ServiceWorkerRegistrationObjectHost
  // in the browser process. It is bound on the main thread for service worker
  // clients (document), and is bound on the service worker thread for service
  // worker execution contexts.
  mojo::AssociatedReceiver<mojom::blink::ServiceWorkerRegistrationObject>
      receiver_{this};

  bool stopped_;
};

class ServiceWorkerRegistrationArray {
  STATIC_ONLY(ServiceWorkerRegistrationArray);

 public:
  // Called from CallbackPromiseAdapter.
  using WebType = WebVector<WebServiceWorkerRegistrationObjectInfo>;
  static HeapVector<Member<ServiceWorkerRegistration>> Take(
      ScriptPromiseResolver* resolver,
      WebType web_service_worker_registrations) {
    HeapVector<Member<ServiceWorkerRegistration>> registrations;
    for (auto& registration : web_service_worker_registrations) {
      registrations.push_back(
          ServiceWorkerRegistration::Take(resolver, std::move(registration)));
    }
    return registrations;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SERVICE_WORKER_SERVICE_WORKER_REGISTRATION_H_
