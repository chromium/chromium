// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/service_worker/service_worker_host.h"

#include <vector>

#include "base/containers/unique_ptr_adapters.h"
#include "base/trace_event/typed_macros.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/service_worker_external_request_result.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/browser/bad_message.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/message_service_api.h"
#include "extensions/browser/process_map.h"
#include "extensions/browser/service_worker/service_worker_task_queue.h"
#include "extensions/common/api/messaging/port_context.h"
#include "extensions/common/constants.h"
#include "extensions/common/mojom/frame.mojom.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/trace_util.h"
#include "ipc/ipc_channel_proxy.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace extensions {

using perfetto::protos::pbzero::ChromeTrackEvent;

namespace {
const void* const kUserDataKey = &kUserDataKey;

class ServiceWorkerHostList : public base::SupportsUserData::Data {
 public:
  std::vector<std::unique_ptr<ServiceWorkerHost>> list;

  static ServiceWorkerHostList* Get(
      content::RenderProcessHost* render_process_host,
      bool create_if_not_exists) {
    auto* service_worker_host_list = static_cast<ServiceWorkerHostList*>(
        render_process_host->GetUserData(kUserDataKey));
    if (!service_worker_host_list && !create_if_not_exists) {
      return nullptr;
    }
    if (!service_worker_host_list) {
      auto new_host_list = std::make_unique<ServiceWorkerHostList>();
      service_worker_host_list = new_host_list.get();
      render_process_host->SetUserData(kUserDataKey, std::move(new_host_list));
    }
    return service_worker_host_list;
  }
};

}  // namespace

ServiceWorkerHost::ServiceWorkerHost(
    content::RenderProcessHost* render_process_host,
    mojo::PendingAssociatedReceiver<mojom::ServiceWorkerHost> receiver)
    : render_process_host_(render_process_host) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  dispatcher_ =
      std::make_unique<ExtensionFunctionDispatcher>(GetBrowserContext());
  receiver_.Bind(std::move(receiver));
  receiver_.set_disconnect_handler(base::BindOnce(
      &ServiceWorkerHost::RemoteDisconnected, base::Unretained(this)));

  render_process_host_->AddObserver(this);
}

ServiceWorkerHost::~ServiceWorkerHost() {
  render_process_host_->RemoveObserver(this);
}

// static
void ServiceWorkerHost::BindReceiver(
    int render_process_id,
    mojo::PendingAssociatedReceiver<mojom::ServiceWorkerHost> receiver) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto* render_process_host =
      content::RenderProcessHost::FromID(render_process_id);
  if (!render_process_host) {
    return;
  }
  auto* service_worker_host_list = ServiceWorkerHostList::Get(
      render_process_host, /*create_if_not_exists=*/true);
  service_worker_host_list->list.push_back(std::make_unique<ServiceWorkerHost>(
      render_process_host, std::move(receiver)));
}

// static
ServiceWorkerHost* ServiceWorkerHost::GetWorkerFor(const WorkerId& worker_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto* render_process_host =
      content::RenderProcessHost::FromID(worker_id.render_process_id);
  if (!render_process_host) {
    return nullptr;
  }

  auto* service_worker_host_list = ServiceWorkerHostList::Get(
      render_process_host, /*create_if_not_exists=*/false);
  if (!service_worker_host_list) {
    return nullptr;
  }
  for (auto& worker : service_worker_host_list->list) {
    if (worker->worker_id_ == worker_id) {
      return worker.get();
    }
  }
  return nullptr;
}

void ServiceWorkerHost::RemoteDisconnected() {
  receiver_.reset();
  permissions_observer_.Reset();
  Destroy();
  // This instance has now been destroyed.
}

void ServiceWorkerHost::DidInitializeServiceWorkerContext(
    const ExtensionId& extension_id,
    int64_t service_worker_version_id,
    int worker_thread_id,
    const blink::ServiceWorkerToken& service_worker_token,
    mojo::PendingAssociatedRemote<mojom::EventDispatcher> event_dispatcher) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::BrowserContext* browser_context = GetBrowserContext();
  if (!browser_context) {
    return;
  }

  ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context);
  DCHECK(registry);
  if (!registry->enabled_extensions().GetByID(extension_id)) {
    // This can happen if the extension is unloaded at this point. Just
    // checking the extension process (as below) is insufficient because
    // tearing down processes is async and happens after extension unload.
    return;
  }

  int render_process_id = render_process_host_->GetID();
  auto* process_map = ProcessMap::Get(browser_context);
  if (!process_map || !process_map->Contains(extension_id, render_process_id)) {
    // We check the process in addition to the registry to guard against
    // situations in which an extension may still be enabled, but no longer
    // running in a given process.
    return;
  }
  worker_id_.extension_id = extension_id;
  worker_id_.version_id = service_worker_version_id;
  worker_id_.render_process_id = render_process_id;
  worker_id_.thread_id = worker_thread_id;
  permissions_observer_.Observe(PermissionsManager::Get(browser_context));

  ServiceWorkerTaskQueue::Get(browser_context)
      ->DidInitializeServiceWorkerContext(
          render_process_id, extension_id, service_worker_version_id,
          worker_thread_id, service_worker_token);
  EventRouter::Get(browser_context)
      ->BindServiceWorkerEventDispatcher(render_process_id, worker_thread_id,
                                         std::move(event_dispatcher));
}

void ServiceWorkerHost::DidStartServiceWorkerContext(
    const ExtensionId& extension_id,
    const base::UnguessableToken& activation_token,
    const GURL& service_worker_scope,
    int64_t service_worker_version_id,
    int worker_thread_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::BrowserContext* browser_context = GetBrowserContext();
  if (!browser_context) {
    return;
  }

  DCHECK_NE(kMainThreadId, worker_thread_id);
  int render_process_id = render_process_host_->GetID();
  auto* process_map = ProcessMap::Get(browser_context);
  if (!process_map || !process_map->Contains(extension_id, render_process_id)) {
    // We can legitimately get here if the extension was already unloaded.
    return;
  }
  CHECK(service_worker_scope.SchemeIs(kExtensionScheme) &&
        extension_id == service_worker_scope.host_piece());

  ServiceWorkerTaskQueue::Get(browser_context)
      ->DidStartServiceWorkerContext(
          render_process_id, extension_id, activation_token,
          service_worker_scope, service_worker_version_id, worker_thread_id);
}

void ServiceWorkerHost::DidStopServiceWorkerContext(
    const ExtensionId& extension_id,
    const base::UnguessableToken& activation_token,
    const GURL& service_worker_scope,
    int64_t service_worker_version_id,
    int worker_thread_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::BrowserContext* browser_context = GetBrowserContext();
  if (!browser_context) {
    return;
  }

  DCHECK_NE(kMainThreadId, worker_thread_id);
  int render_process_id = render_process_host_->GetID();
  auto* process_map = ProcessMap::Get(browser_context);
  if (!process_map || !process_map->Contains(extension_id, render_process_id)) {
    // We can legitimately get here if the extension was already unloaded.
    return;
  }
  CHECK(service_worker_scope.SchemeIs(kExtensionScheme) &&
        extension_id == service_worker_scope.host_piece());
  CHECK(!extension_id.empty());
  CHECK_NE(blink::mojom::kInvalidServiceWorkerVersionId,
           service_worker_version_id);
  ServiceWorkerTaskQueue::Get(browser_context)
      ->DidStopServiceWorkerContext(
          render_process_id, extension_id, activation_token,
          service_worker_scope, service_worker_version_id, worker_thread_id);
}

void ServiceWorkerHost::RequestWorker(mojom::RequestParamsPtr params,
                                      RequestWorkerCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!GetBrowserContext()) {
    std::move(callback).Run(ExtensionFunction::FAILED, base::Value::List(),
                            "No browser context", nullptr);
    return;
  }

  dispatcher_->DispatchForServiceWorker(
      std::move(params), render_process_host_->GetID(), std::move(callback));
}

void ServiceWorkerHost::WorkerResponseAck(const base::Uuid& request_uuid) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!GetBrowserContext()) {
    return;
  }

  dispatcher_->ProcessResponseAck(request_uuid);
}

content::BrowserContext* ServiceWorkerHost::GetBrowserContext() {
  return render_process_host_->GetBrowserContext();
}

mojom::ServiceWorker* ServiceWorkerHost::GetServiceWorker() {
  if (!remote_.is_bound()) {
    content::ServiceWorkerContext* context =
        util::GetServiceWorkerContextForExtensionId(worker_id_.extension_id,
                                                    GetBrowserContext());
    CHECK(context);
    if (!context->IsLiveRunningServiceWorker(worker_id_.version_id)) {
      return nullptr;
    }

    context->GetRemoteAssociatedInterfaces(worker_id_.version_id)
        .GetInterface(&remote_);
  }
  return remote_.get();
}

void ServiceWorkerHost::OnExtensionPermissionsUpdated(
    const Extension& extension,
    const PermissionSet& permissions,
    PermissionsManager::UpdateReason reason) {
  if (extension.id() != worker_id_.extension_id) {
    return;
  }
  content::BrowserContext* browser_context = GetBrowserContext();
  if (!browser_context) {
    return;
  }
  content::ServiceWorkerContext* context =
      util::GetServiceWorkerContextForExtensionId(worker_id_.extension_id,
                                                  browser_context);
  CHECK(context);
  auto* service_worker_remote = GetServiceWorker();
  if (!service_worker_remote) {
    return;
  }

  const PermissionsData* permissions_data = extension.permissions_data();
  service_worker_remote->UpdatePermissions(
      std::move(*permissions_data->active_permissions().Clone()),
      std::move(*permissions_data->withheld_permissions().Clone()));
}

void ServiceWorkerHost::OpenChannelToExtension(
    extensions::mojom::ExternalConnectionInfoPtr info,
    extensions::mojom::ChannelType channel_type,
    const std::string& channel_name,
    const PortId& port_id,
    mojo::PendingAssociatedRemote<extensions::mojom::MessagePort> port,
    mojo::PendingAssociatedReceiver<extensions::mojom::MessagePortHost>
        port_host) {
  TRACE_EVENT("extensions", "ServiceWorkerHost::OpenChannelToExtension",
              ChromeTrackEvent::kRenderProcessHost, *render_process_host_);
  content::BrowserContext* browser_context = GetBrowserContext();
  if (!browser_context) {
    return;
  }

  MessageServiceApi::GetMessageService()->OpenChannelToExtension(
      browser_context, worker_id_, port_id, *info, channel_type, channel_name,
      std::move(port), std::move(port_host));
}

void ServiceWorkerHost::OpenChannelToNativeApp(
    const std::string& native_app_name,
    const PortId& port_id,
    mojo::PendingAssociatedRemote<extensions::mojom::MessagePort> port,
    mojo::PendingAssociatedReceiver<extensions::mojom::MessagePortHost>
        port_host) {
  TRACE_EVENT("extensions", "ServiceWorkerHost::OnOpenChannelToNativeApp",
              ChromeTrackEvent::kRenderProcessHost, *render_process_host_);
  content::BrowserContext* browser_context = GetBrowserContext();
  if (!browser_context) {
    return;
  }

  MessageServiceApi::GetMessageService()->OpenChannelToNativeApp(
      browser_context, worker_id_, port_id, native_app_name, std::move(port),
      std::move(port_host));
}

void ServiceWorkerHost::OpenChannelToTab(
    int32_t tab_id,
    int32_t frame_id,
    const std::optional<std::string>& document_id,
    extensions::mojom::ChannelType channel_type,
    const std::string& channel_name,
    const PortId& port_id,
    mojo::PendingAssociatedRemote<extensions::mojom::MessagePort> port,
    mojo::PendingAssociatedReceiver<extensions::mojom::MessagePortHost>
        port_host) {
  TRACE_EVENT("extensions", "ServiceWorkerHost::OpenChannelToTab",
              ChromeTrackEvent::kRenderProcessHost, *render_process_host_);
  content::BrowserContext* browser_context = GetBrowserContext();
  if (!browser_context) {
    return;
  }

  MessageServiceApi::GetMessageService()->OpenChannelToTab(
      browser_context, worker_id_, port_id, tab_id, frame_id,
      document_id ? *document_id : std::string(), channel_type, channel_name,
      std::move(port), std::move(port_host));
}

void ServiceWorkerHost::Destroy() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto* service_worker_host_list = ServiceWorkerHostList::Get(
      render_process_host_, /*create_if_not_exists=*/false);
  CHECK(service_worker_host_list);
  // std::erase_if will lead to a call to the destructor for this object.
  std::erase_if(service_worker_host_list->list, base::MatchesUniquePtr(this));
}

void ServiceWorkerHost::RenderProcessExited(
    content::RenderProcessHost* host,
    const content::ChildProcessTerminationInfo& info) {
  CHECK_EQ(host, render_process_host_);
  // TODO(crbug.com/40062641): Investigate clearing the user data from
  // RenderProcessHostImpl::Cleanup.
  Destroy();
  // This instance has now been deleted.
}

}  // namespace extensions
