// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_SERVICE_WORKER_DATA_H_
#define EXTENSIONS_RENDERER_SERVICE_WORKER_DATA_H_

#include <memory>

#include "base/containers/circular_deque.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/unguessable_token.h"
#include "extensions/common/mojom/automation_registry.mojom.h"
#include "extensions/common/mojom/event_dispatcher.mojom.h"
#include "extensions/common/mojom/event_router.mojom.h"
#include "extensions/common/mojom/renderer_host.mojom.h"
#include "extensions/common/mojom/service_worker.mojom.h"
#include "extensions/common/mojom/service_worker_host.mojom.h"
#include "extensions/common/mojom/web_request_host.mojom.h"
#include "extensions/renderer/v8_schema_registry.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/web/modules/service_worker/web_service_worker_context_proxy.h"

namespace extensions {
class NativeExtensionBindingsSystem;
class ScriptContext;

// Per ServiceWorker data in worker thread.
// TODO(lazyboy): Also put worker ScriptContexts in this.
class ServiceWorkerData
    : public mojom::EventDispatcher,
      public mojom::ServiceWorker
{
 public:
  ServiceWorkerData(
      blink::WebServiceWorkerContextProxy* proxy,
      int64_t service_worker_version_id,
      const std::optional<base::UnguessableToken>& activation_sequence,
      const blink::ServiceWorkerToken& service_worker_token,
      ScriptContext* context,
      std::unique_ptr<NativeExtensionBindingsSystem> bindings_system);

  ServiceWorkerData(const ServiceWorkerData&) = delete;
  ServiceWorkerData& operator=(const ServiceWorkerData&) = delete;

  ~ServiceWorkerData() override;

  void Init();

  // Completes the listener registration phase for this worker and posts a task
  // to flush queued events. Returns false if no registration phase is in
  // progress.
  bool MarkListenerRegistrationComplete();

  V8SchemaRegistry* v8_schema_registry() { return v8_schema_registry_.get(); }
  NativeExtensionBindingsSystem* bindings_system() {
    return bindings_system_.get();
  }
  int64_t service_worker_version_id() const {
    return service_worker_version_id_;
  }
  const std::optional<base::UnguessableToken>& activation_sequence() const {
    return activation_sequence_;
  }
  ScriptContext* context() const { return context_; }

  blink::WebServiceWorkerContextProxy* worker_context_proxy() const {
    return proxy_;
  }

  mojom::RendererHost* GetRendererHost();
  mojom::ServiceWorkerHost* GetServiceWorkerHost();
  mojom::EventRouter* GetEventRouter();
  mojom::RendererAutomationRegistry* GetAutomationRegistry();
  mojom::WebRequestHost* GetWebRequestHost();

  // mojom::ServiceWorker overrides:
  void UpdatePermissions(PermissionSet active_permissions,
                         PermissionSet withheld_permissions) override;
  void DispatchOnConnect(
      const PortId& port_id,
      extensions::mojom::ChannelType channel_type,
      const std::string& channel_name,
      extensions::mojom::TabConnectionInfoPtr tab_info,
      extensions::mojom::ExternalConnectionInfoPtr external_connection_info,
      mojo::PendingAssociatedReceiver<extensions::mojom::MessagePort> port,
      mojo::PendingAssociatedRemote<extensions::mojom::MessagePortHost>
          port_host,
      DispatchOnConnectCallback callback) override;

  // mojom::EventDispatcher overrides:
  void DispatchEvent(mojom::DispatchEventParamsPtr params,
                     const scoped_refptr<const EventArgs>& event_args,
                     DispatchEventCallback callback) override;

 private:
  // An event queued until the listener registration phase finishes and the
  // flush task runs.
  struct QueuedEvent {
    mojom::DispatchEventParamsPtr params;
    scoped_refptr<const EventArgs> event_args;
  };

  void OnServiceWorkerRequest(
      mojo::PendingAssociatedReceiver<mojom::ServiceWorker> receiver);

  // Dispatches an event to the JS listeners and runs the post-dispatch
  // bookkeeping.
  void DispatchEventToListeners(const mojom::DispatchEventParams& params,
                                const base::ListValue& event_args);

  // Flushes queued events in FIFO order.
  void FlushQueuedEvents();

  // For extensions with `background.async_listener_registration`, true from
  // worker initialization until `runtime.markListenerRegistrationComplete()`
  // is called. While true, incoming events are queued in `queued_events_`.
  bool in_listener_registration_phase_ = false;

  // Events queued until the flush task runs. They have no keepalive, so they
  // do not extend the worker's lifetime. If the worker stops before the flush
  // task runs, they are discarded.
  base::circular_deque<QueuedEvent> queued_events_;

  raw_ptr<blink::WebServiceWorkerContextProxy> proxy_;
  const int64_t service_worker_version_id_;
  const std::optional<base::UnguessableToken> activation_sequence_;
  const blink::ServiceWorkerToken service_worker_token_;
  const raw_ptr<ScriptContext, DanglingUntriaged> context_ = nullptr;

  std::unique_ptr<V8SchemaRegistry> v8_schema_registry_;
  std::unique_ptr<NativeExtensionBindingsSystem> bindings_system_;
  mojo::AssociatedRemote<mojom::RendererHost> renderer_host_;
  mojo::AssociatedRemote<mojom::ServiceWorkerHost> service_worker_host_;
  mojo::AssociatedReceiver<mojom::EventDispatcher> event_dispatcher_receiver_{
      this};
  mojo::AssociatedRemote<mojom::EventRouter> event_router_remote_;
  mojo::AssociatedRemote<mojom::RendererAutomationRegistry>
      renderer_automation_registry_remote_;
  mojo::AssociatedRemote<mojom::WebRequestHost> web_request_host_;
  mojo::AssociatedReceiver<mojom::ServiceWorker> receiver_{this};

  THREAD_CHECKER(thread_checker_);

  base::WeakPtrFactory<ServiceWorkerData> weak_ptr_factory_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_SERVICE_WORKER_DATA_H_
