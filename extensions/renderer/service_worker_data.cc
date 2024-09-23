// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/service_worker_data.h"

#include "extensions/common/extension.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/renderer/dispatcher.h"
#include "extensions/renderer/extension_interaction_provider.h"
#include "extensions/renderer/native_extension_bindings_system.h"
#include "extensions/renderer/renderer_extension_registry.h"
#include "extensions/renderer/worker_script_context_set.h"
#include "extensions/renderer/worker_thread_dispatcher.h"
#include "extensions/renderer/worker_thread_util.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace extensions {

ServiceWorkerData::ServiceWorkerData(
    blink::WebServiceWorkerContextProxy* proxy,
    int64_t service_worker_version_id,
    const std::optional<base::UnguessableToken>& activation_sequence,
    const blink::ServiceWorkerToken& service_worker_token,
    ScriptContext* context,
    std::unique_ptr<NativeExtensionBindingsSystem> bindings_system)
    : proxy_(proxy),
      service_worker_version_id_(service_worker_version_id),
      activation_sequence_(std::move(activation_sequence)),
      service_worker_token_(service_worker_token),
      context_(context),
      v8_schema_registry_(new V8SchemaRegistry),
      bindings_system_(std::move(bindings_system)) {
  CHECK(bindings_system_);
  proxy_->GetAssociatedInterfaceRegistry().AddInterface<mojom::ServiceWorker>(
      base::BindRepeating(&ServiceWorkerData::OnServiceWorkerRequest,
                          weak_ptr_factory_.GetWeakPtr()));
}

ServiceWorkerData::~ServiceWorkerData() = default;

void ServiceWorkerData::OnServiceWorkerRequest(
    mojo::PendingAssociatedReceiver<mojom::ServiceWorker> receiver) {
  CHECK(bindings_system_);
  receiver_.reset();
  receiver_.Bind(std::move(receiver));
}

void ServiceWorkerData::UpdatePermissions(PermissionSet active_permissions,
                                          PermissionSet withheld_permissions) {
  DCHECK(worker_thread_util::IsWorkerThread());
  CHECK(bindings_system_);

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
  CHECK(bindings_system_);
  if (!service_worker_host_.is_bound()) {
    proxy_->GetRemoteAssociatedInterface(
        service_worker_host_.BindNewEndpointAndPassReceiver());
  }
  return service_worker_host_.get();
}

mojom::EventRouter* ServiceWorkerData::GetEventRouter() {
  CHECK(bindings_system_);
  if (!event_router_remote_.is_bound()) {
    proxy_->GetRemoteAssociatedInterface(
        event_router_remote_.BindNewEndpointAndPassReceiver());
  }
  return event_router_remote_.get();
}

mojom::RendererAutomationRegistry* ServiceWorkerData::GetAutomationRegistry() {
  CHECK(bindings_system_);
  if (!renderer_automation_registry_remote_.is_bound()) {
    proxy_->GetRemoteAssociatedInterface(
        renderer_automation_registry_remote_.BindNewEndpointAndPassReceiver());
  }
  return renderer_automation_registry_remote_.get();
}

mojom::RendererHost* ServiceWorkerData::GetRendererHost() {
  // We allow access to mojom::RendererHost without a `bindings_system_`.
  if (!renderer_host_.is_bound()) {
    proxy_->GetRemoteAssociatedInterface(
        renderer_host_.BindNewEndpointAndPassReceiver());
  }
  return renderer_host_.get();
}

void ServiceWorkerData::Init() {
  // If we do not have bindings there is no additional init necessary
  if (!bindings_system_) {
    return;
  }
  const int thread_id = content::WorkerThread::GetCurrentId();
  GetServiceWorkerHost()->DidInitializeServiceWorkerContext(
      context_->GetExtensionID(), service_worker_version_id_, thread_id,
      service_worker_token_,
      event_dispatcher_receiver_.BindNewEndpointAndPassRemote());
}

void ServiceWorkerData::DispatchEvent(mojom::DispatchEventParamsPtr params,
                                      base::Value::List event_args,
                                      DispatchEventCallback callback) {
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

  std::move(callback).Run(
      // False since this is only possibly true for lazy background page.
      /*event_will_run_in_lazy_background_page_script=*/false);
}

void ServiceWorkerData::DispatchOnConnect(
    const PortId& port_id,
    extensions::mojom::ChannelType channel_type,
    const std::string& channel_name,
    extensions::mojom::TabConnectionInfoPtr tab_info,
    extensions::mojom::ExternalConnectionInfoPtr external_connection_info,
    mojo::PendingAssociatedReceiver<extensions::mojom::MessagePort> port,
    mojo::PendingAssociatedRemote<extensions::mojom::MessagePortHost> port_host,
    DispatchOnConnectCallback callback) {
  WorkerThreadDispatcher::GetBindingsSystem()
      ->messaging_service()
      ->DispatchOnConnect(Dispatcher::GetWorkerScriptContextSet(), port_id,
                          channel_type, channel_name, *tab_info,
                          *external_connection_info, std::move(port),
                          std::move(port_host),
                          // Render frames do not matter.
                          nullptr, std::move(callback));
}

}  // namespace extensions
