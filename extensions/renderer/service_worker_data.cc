// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/service_worker_data.h"

#include "extensions/common/extension.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/renderer/dispatcher.h"
#include "extensions/renderer/extension_interaction_provider.h"
#include "extensions/renderer/native_extension_bindings_system.h"
#include "extensions/renderer/renderer_extension_registry.h"
#include "extensions/renderer/worker_script_context_set.h"
#include "extensions/renderer/worker_thread_dispatcher.h"
#include "extensions/renderer/worker_thread_util.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"

namespace extensions {

ServiceWorkerData::ServiceWorkerData(
    blink::WebServiceWorkerContextProxy* proxy,
    int64_t service_worker_version_id,
    base::UnguessableToken activation_sequence,
    ScriptContext* context,
    std::unique_ptr<NativeExtensionBindingsSystem> bindings_system)
    : proxy_(proxy),
      service_worker_version_id_(service_worker_version_id),
      activation_sequence_(std::move(activation_sequence)),
      context_(context),
      v8_schema_registry_(new V8SchemaRegistry),
      bindings_system_(std::move(bindings_system)) {
#if !BUILDFLAG(ENABLE_EXTENSIONS_LEGACY_IPC)
  proxy_->GetAssociatedInterfaceRegistry().AddInterface<mojom::ServiceWorker>(
      base::BindRepeating(&ServiceWorkerData::OnServiceWorkerRequest,
                          weak_ptr_factory_.GetWeakPtr()));
#endif
}

ServiceWorkerData::~ServiceWorkerData() = default;

#if !BUILDFLAG(ENABLE_EXTENSIONS_LEGACY_IPC)
void ServiceWorkerData::OnServiceWorkerRequest(
    mojo::PendingAssociatedReceiver<mojom::ServiceWorker> receiver) {
  receiver_.reset();
  receiver_.Bind(std::move(receiver));
}

void ServiceWorkerData::UpdatePermissions(PermissionSet active_permissions,
                                          PermissionSet withheld_permissions) {
  DCHECK(worker_thread_util::IsWorkerThread());

  const ExtensionId& extension_id = context_->GetExtensionID();
  const Extension* extension =
      RendererExtensionRegistry::Get()->GetByID(extension_id);
  if (!extension) {
    return;
  }
  extension->permissions_data()->SetPermissions(
      std::make_unique<const PermissionSet>(std::move(active_permissions)),
      std::make_unique<const PermissionSet>(std::move(withheld_permissions)));

  bindings_system_->UpdateBindings(extension_id, /*permissions_changed=*/true,
                                   Dispatcher::GetWorkerScriptContextSet());
}

mojom::ServiceWorkerHost* ServiceWorkerData::GetServiceWorkerHost() {
  if (!service_worker_host_.is_bound()) {
    proxy_->GetRemoteAssociatedInterface(
        service_worker_host_.BindNewEndpointAndPassReceiver());
  }
  return service_worker_host_.get();
}

mojom::EventRouter* ServiceWorkerData::GetEventRouter() {
  if (!event_router_remote_.is_bound()) {
    proxy_->GetRemoteAssociatedInterface(
        event_router_remote_.BindNewEndpointAndPassReceiver());
  }
  return event_router_remote_.get();
}

mojom::RendererAutomationRegistry* ServiceWorkerData::GetAutomationRegistry() {
  if (!renderer_automation_registry_remote_.is_bound()) {
    proxy_->GetRemoteAssociatedInterface(
        renderer_automation_registry_remote_.BindNewEndpointAndPassReceiver());
  }
  return renderer_automation_registry_remote_.get();
}
#endif

void ServiceWorkerData::Init() {
#if BUILDFLAG(ENABLE_EXTENSIONS_LEGACY_IPC)
  WorkerThreadDispatcher::Get()->DidInitializeContext(
      service_worker_version_id_);
#else
  const int thread_id = content::WorkerThread::GetCurrentId();
  GetServiceWorkerHost()->DidInitializeServiceWorkerContext(
      context_->GetExtensionID(), service_worker_version_id_, thread_id,
      event_dispatcher_receiver_.BindNewEndpointAndPassRemote());
#endif
}

#if !BUILDFLAG(ENABLE_EXTENSIONS_LEGACY_IPC)
void ServiceWorkerData::DispatchEvent(mojom::DispatchEventParamsPtr params,
                                      base::Value::List event_args) {
  ScriptContext* script_context = context();
  // Note |scoped_extension_interaction| requires a HandleScope.
  v8::Isolate* isolate = script_context->isolate();
  v8::HandleScope handle_scope(isolate);
  std::unique_ptr<InteractionProvider::Scope> scoped_extension_interaction;
  if (params->is_user_gesture) {
    scoped_extension_interaction =
        ExtensionInteractionProvider::Scope::ForWorker(
            script_context->v8_context());
  }

  bindings_system()->DispatchEventInContext(params->event_name, event_args,
                                            std::move(params->filtering_info),
                                            context());
  const int worker_thread_id = content::WorkerThread::GetCurrentId();
  WorkerThreadDispatcher::Get()->Send(new ExtensionHostMsg_EventAckWorker(
      context()->GetExtensionID(), service_worker_version_id(),
      worker_thread_id, params->event_id));
}
#endif

}  // namespace extensions
