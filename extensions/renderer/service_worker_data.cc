// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/service_worker_data.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/single_thread_task_runner.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/background_info.h"
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
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  CHECK(bindings_system_);
  proxy_->GetAssociatedInterfaceRegistry().AddInterface<mojom::ServiceWorker>(
      base::BindRepeating(&ServiceWorkerData::OnServiceWorkerRequest,
                          weak_ptr_factory_.GetWeakPtr()));
}

ServiceWorkerData::~ServiceWorkerData() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void ServiceWorkerData::OnServiceWorkerRequest(
    mojo::PendingAssociatedReceiver<mojom::ServiceWorker> receiver) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  CHECK(bindings_system_);
  receiver_.reset();
  receiver_.Bind(std::move(receiver));
}

void ServiceWorkerData::UpdatePermissions(PermissionSet active_permissions,
                                          PermissionSet withheld_permissions) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
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
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  CHECK(bindings_system_);
  if (!service_worker_host_.is_bound()) {
    proxy_->GetRemoteAssociatedInterface(
        service_worker_host_.BindNewEndpointAndPassReceiver());
  }
  return service_worker_host_.get();
}

mojom::EventRouter* ServiceWorkerData::GetEventRouter() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  CHECK(bindings_system_);
  if (!event_router_remote_.is_bound()) {
    proxy_->GetRemoteAssociatedInterface(
        event_router_remote_.BindNewEndpointAndPassReceiver());
  }
  return event_router_remote_.get();
}

mojom::RendererAutomationRegistry* ServiceWorkerData::GetAutomationRegistry() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  CHECK(bindings_system_);
  if (!renderer_automation_registry_remote_.is_bound()) {
    proxy_->GetRemoteAssociatedInterface(
        renderer_automation_registry_remote_.BindNewEndpointAndPassReceiver());
  }
  return renderer_automation_registry_remote_.get();
}

mojom::RendererHost* ServiceWorkerData::GetRendererHost() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // We allow access to mojom::RendererHost without a `bindings_system_`.
  if (!renderer_host_.is_bound()) {
    proxy_->GetRemoteAssociatedInterface(
        renderer_host_.BindNewEndpointAndPassReceiver());
  }
  return renderer_host_.get();
}

mojom::WebRequestHost* ServiceWorkerData::GetWebRequestHost() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  CHECK(bindings_system_);
  if (!web_request_host_.is_bound()) {
    proxy_->GetRemoteAssociatedInterface(
        web_request_host_.BindNewEndpointAndPassReceiver());
  }
  return web_request_host_.get();
}

void ServiceWorkerData::Init() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  CHECK(bindings_system_);
  const int thread_id = content::WorkerThread::GetCurrentId();
  const ExtensionId& extension_id = context_->GetExtensionID();
  CHECK(!extension_id.empty());
  CHECK(activation_sequence_.has_value());

  // Set `in_listener_registration_phase_` before binding
  // `event_dispatcher_receiver_` so incoming events are properly queued.
  in_listener_registration_phase_ =
      BackgroundInfo::HasAsyncListenerRegistration(context_->extension());

  GetServiceWorkerHost()->DidInitializeServiceWorkerContext(
      extension_id, *activation_sequence_, service_worker_version_id_,
      thread_id, service_worker_token_,
      event_dispatcher_receiver_.BindNewEndpointAndPassRemote());
}

bool ServiceWorkerData::MarkListenerRegistrationComplete() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!in_listener_registration_phase_) {
    return false;
  }
  in_listener_registration_phase_ = false;
  // Flush in a separate task. This method runs synchronously during
  // `runtime.markListenerRegistrationComplete()`, so flushing immediately would
  // run listeners reentrantly before the call returns.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&ServiceWorkerData::FlushQueuedEvents,
                                weak_ptr_factory_.GetWeakPtr()));
  return true;
}

void ServiceWorkerData::DispatchEvent(
    mojom::DispatchEventParamsPtr params,
    const scoped_refptr<const EventArgs>& event_args,
    DispatchEventCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  CHECK(event_args);
  // Queue events while registration is in progress or earlier events are
  // waiting to flush, preserving FIFO order.
  if (in_listener_registration_phase_ || !queued_events_.empty()) {
    // TODO(crbug.com/509627729): Bound the queue. When full, drop the oldest
    // event while still completing its webRequest bookkeeping
    // (`DidDispatchEvent()`).
    queued_events_.push_back(QueuedEvent{std::move(params), event_args});
  } else {
    DispatchEventToListeners(*params, event_args->data);
  }

  // Queued events do not keep the worker alive: they have no browser keepalive
  // and are acked at queue time. If the worker stops at the idle timeout
  // before registration completes, the phase aborts and the queued events are
  // lost.
  std::move(callback).Run(
      /*event_will_run_in_lazy_background_page_script=*/false);
}

void ServiceWorkerData::DispatchEventToListeners(
    const mojom::DispatchEventParams& params,
    const base::ListValue& event_args) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  ScriptContext* script_context = context();
  // Note |scoped_extension_interaction| requires a HandleScope.
  v8::Isolate* isolate = script_context->isolate();
  v8::HandleScope handle_scope(isolate);
  std::unique_ptr<InteractionProvider::Scope> scoped_extension_interaction;
  if (params.is_user_gesture) {
    scoped_extension_interaction =
        ExtensionInteractionProvider::Scope::ForWorker(
            script_context->v8_context());
  }

  bindings_system()->DispatchEventInContext(params.event_name, event_args,
                                            params.filtering_info, context());
  // The worker has a single context, so one dispatch notifies every listener.
  bindings_system()->DidDispatchEvent(*params.host_id, params.event_name,
                                      event_args);
}

void ServiceWorkerData::FlushQueuedEvents() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  CHECK(!in_listener_registration_phase_);
  base::UmaHistogramCounts10000(
      "Extensions.ServiceWorkerBackground.AsyncListenerRegistration."
      "QueuedEvents",
      queued_events_.size());
  // Flush queued events in FIFO order. Because this runs in a single task,
  // new Mojo events cannot interleave. Events without matching JS listeners
  // are discarded downstream in `APIEventHandler::FireEventInContext()`.
  base::WeakPtr<ServiceWorkerData> weak_this = weak_ptr_factory_.GetWeakPtr();
  while (!queued_events_.empty()) {
    if (!context_->is_valid()) {
      queued_events_.clear();
      break;
    }
    QueuedEvent event = std::move(queued_events_.front());
    queued_events_.pop_front();
    DispatchEventToListeners(*event.params, event.event_args->data);
    if (!weak_this) {
      return;
    }
  }
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
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
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
