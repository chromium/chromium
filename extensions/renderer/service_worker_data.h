// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_SERVICE_WORKER_DATA_H_
#define EXTENSIONS_RENDERER_SERVICE_WORKER_DATA_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/unguessable_token.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/mojom/automation_registry.mojom.h"
#include "extensions/common/mojom/event_dispatcher.mojom.h"
#include "extensions/common/mojom/event_router.mojom.h"
#include "extensions/common/mojom/service_worker_host.mojom.h"
#include "extensions/renderer/v8_schema_registry.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/web/modules/service_worker/web_service_worker_context_proxy.h"

namespace extensions {
class NativeExtensionBindingsSystem;
class ScriptContext;

// Per ServiceWorker data in worker thread.
// TODO(lazyboy): Also put worker ScriptContexts in this.
class ServiceWorkerData
#if !BUILDFLAG(ENABLE_EXTENSIONS_LEGACY_IPC)
    : public mojom::EventDispatcher
#endif
{
 public:
  ServiceWorkerData(
      blink::WebServiceWorkerContextProxy* proxy,
      int64_t service_worker_version_id,
      base::UnguessableToken activation_sequence,
      ScriptContext* context,
      std::unique_ptr<NativeExtensionBindingsSystem> bindings_system);

  ServiceWorkerData(const ServiceWorkerData&) = delete;
  ServiceWorkerData& operator=(const ServiceWorkerData&) = delete;

#if BUILDFLAG(ENABLE_EXTENSIONS_LEGACY_IPC)
  ~ServiceWorkerData();
#else
  ~ServiceWorkerData() override;
#endif

  void Init();

  V8SchemaRegistry* v8_schema_registry() { return v8_schema_registry_.get(); }
  NativeExtensionBindingsSystem* bindings_system() {
    return bindings_system_.get();
  }
  int64_t service_worker_version_id() const {
    return service_worker_version_id_;
  }
  const base::UnguessableToken& activation_sequence() const {
    return activation_sequence_;
  }
  ScriptContext* context() const { return context_; }

  blink::WebServiceWorkerContextProxy* worker_context_proxy() const {
    return proxy_;
  }

#if !BUILDFLAG(ENABLE_EXTENSIONS_LEGACY_IPC)
  mojom::ServiceWorkerHost* GetServiceWorkerHost();
  mojom::EventRouter* GetEventRouter();
  mojom::RendererAutomationRegistry* GetAutomationRegistry();

  // mojom::EventDispatcher overrides:
  void DispatchEvent(mojom::DispatchEventParamsPtr params,
                     base::Value::List event_args) override;
#endif

 private:
  blink::WebServiceWorkerContextProxy* proxy_;
  const int64_t service_worker_version_id_;
  const base::UnguessableToken activation_sequence_;
  const raw_ptr<ScriptContext, ExperimentalRenderer> context_ = nullptr;

  std::unique_ptr<V8SchemaRegistry> v8_schema_registry_;
  std::unique_ptr<NativeExtensionBindingsSystem> bindings_system_;
#if !BUILDFLAG(ENABLE_EXTENSIONS_LEGACY_IPC)
  mojo::AssociatedRemote<mojom::ServiceWorkerHost> service_worker_host_;
  mojo::AssociatedReceiver<mojom::EventDispatcher> event_dispatcher_receiver_{
      this};
  mojo::AssociatedRemote<mojom::EventRouter> event_router_remote_;
  mojo::AssociatedRemote<mojom::RendererAutomationRegistry>
      renderer_automation_registry_remote_;
#endif
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_SERVICE_WORKER_DATA_H_
