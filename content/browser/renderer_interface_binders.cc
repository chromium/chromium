// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_interface_binders.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/no_destructor.h"
#include "base/task/post_task.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/native_file_system/native_file_system_manager_impl.h"
#include "content/browser/notifications/platform_notification_context_impl.h"
#include "content/browser/permissions/permission_service_context.h"
#include "content/browser/quota_dispatcher_host.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/websockets/websocket_connector_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/cache_storage/cache_storage.mojom.h"
#include "third_party/blink/public/mojom/native_file_system/native_file_system_manager.mojom.h"
#include "third_party/blink/public/mojom/notifications/notification_service.mojom.h"
#include "url/origin.h"

namespace content {
namespace {

// A holder for a parameterized BinderRegistry for content-layer interfaces
// exposed to web workers.
class RendererInterfaceBinders {
 public:
  RendererInterfaceBinders() { InitializeParameterizedBinderRegistry(); }

  // Bind an interface request |interface_pipe| for |interface_name| received
  // from a web worker with origin |origin| hosted in the renderer |host|.
  void BindInterface(const std::string& interface_name,
                     mojo::ScopedMessagePipeHandle interface_pipe,
                     RenderProcessHost* host,
                     const url::Origin& origin) {
    if (parameterized_binder_registry_.TryBindInterface(
            interface_name, &interface_pipe, host, origin)) {
      return;
    }

    GetContentClient()->browser()->BindInterfaceRequestFromWorker(
        host, origin, interface_name, std::move(interface_pipe));
  }

  // Try binding an interface request |interface_pipe| for |interface_name|
  // received from |frame|.
  bool TryBindInterface(const std::string& interface_name,
                        mojo::ScopedMessagePipeHandle* interface_pipe,
                        RenderFrameHost* frame) {
    return parameterized_binder_registry_.TryBindInterface(
        interface_name, interface_pipe, frame->GetProcess(),
        frame->GetLastCommittedOrigin());
  }

 private:
  void InitializeParameterizedBinderRegistry();

  static void CreateWebSocketConnector(
      mojo::PendingReceiver<blink::mojom::WebSocketConnector> receiver,
      RenderProcessHost* host,
      const url::Origin& origin);

  service_manager::BinderRegistryWithArgs<RenderProcessHost*,
                                          const url::Origin&>
      parameterized_binder_registry_;
};

// Register renderer-exposed interfaces. Each registered interface binder is
// exposed to all renderer-hosted execution context types (document/frame,
// dedicated worker, shared worker and service worker) where the appropriate
// capability spec in the content_browser manifest includes the interface. For
// interface requests from frames, binders registered on the frame itself
// override binders registered here.
void RendererInterfaceBinders::InitializeParameterizedBinderRegistry() {
  // Used for shared workers and service workers to create a websocket.
  // In other cases, RenderFrameHostImpl for documents or DedicatedWorkerHost
  // for dedicated workers handles interface requests in order to associate
  // websockets with a frame. Shared workers and service workers don't have to
  // do it because they don't have a frame.
  // TODO(nhiroki): Consider moving this into SharedWorkerHost and
  // ServiceWorkerProviderHost.
  parameterized_binder_registry_.AddInterface(
      base::BindRepeating(CreateWebSocketConnector));

  parameterized_binder_registry_.AddInterface(base::BindRepeating(
      [](mojo::PendingReceiver<blink::mojom::CacheStorage> receiver,
         RenderProcessHost* host, const url::Origin& origin) {
        static_cast<RenderProcessHostImpl*>(host)->BindCacheStorage(
            std::move(receiver), origin);
      }));
  if (base::FeatureList::IsEnabled(blink::features::kNativeFileSystemAPI)) {
    parameterized_binder_registry_.AddInterface(base::BindRepeating(
        [](mojo::PendingReceiver<blink::mojom::NativeFileSystemManager>
               receiver,
           RenderProcessHost* host, const url::Origin& origin) {
          // This code path is only for workers, hence always pass in
          // MSG_ROUTING_NONE as frame ID. Frames themselves go through
          // RenderFrameHostImpl instead.
          auto* storage_partition =
              static_cast<StoragePartitionImpl*>(host->GetStoragePartition());
          auto* manager = storage_partition->GetNativeFileSystemManager();
          manager->BindReceiver(
              NativeFileSystemManagerImpl::BindingContext(
                  origin,
                  // TODO(https://crbug.com/989323): Obtain and use a better
                  // URL for workers instead of the origin as source url.
                  // This URL will be used for SafeBrowsing checks and for
                  // the Quarantine Service.
                  origin.GetURL(), host->GetID(), MSG_ROUTING_NONE),
              std::move(receiver));
        }));
  }

  parameterized_binder_registry_.AddInterface(base::BindRepeating(
      [](blink::mojom::NotificationServiceRequest request,
         RenderProcessHost* host, const url::Origin& origin) {
        static_cast<StoragePartitionImpl*>(host->GetStoragePartition())
            ->GetPlatformNotificationContext()
            ->CreateService(origin, std::move(request));
      }));
  parameterized_binder_registry_.AddInterface(
      base::BindRepeating(&QuotaDispatcherHost::CreateForWorker));
}

RendererInterfaceBinders& GetRendererInterfaceBinders() {
  static base::NoDestructor<RendererInterfaceBinders> binders;
  return *binders;
}

void RendererInterfaceBinders::CreateWebSocketConnector(
    mojo::PendingReceiver<blink::mojom::WebSocketConnector> receiver,
    RenderProcessHost* host,
    const url::Origin& origin) {
  // TODO(jam): is it ok to not send extraHeaders for sockets created from
  // shared and service workers?
  //
  // Shared Workers and service workers are not directly associated with a
  // frame, so the concept of "top-level frame" does not exist. Can use
  // (origin, origin) for the NetworkIsolationKey for requests because these
  // workers can only be created when the site has cookie access.
  mojo::MakeSelfOwnedReceiver(std::make_unique<WebSocketConnectorImpl>(
                                  host->GetID(), MSG_ROUTING_NONE, origin,
                                  net::NetworkIsolationKey(origin, origin)),
                              std::move(receiver));
}

}  // namespace

void BindWorkerInterface(const std::string& interface_name,
                         mojo::ScopedMessagePipeHandle interface_pipe,
                         RenderProcessHost* host,
                         const url::Origin& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  GetRendererInterfaceBinders().BindInterface(
      interface_name, std::move(interface_pipe), host, origin);
}

bool TryBindFrameInterface(const std::string& interface_name,
                           mojo::ScopedMessagePipeHandle* interface_pipe,
                           RenderFrameHost* frame) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  return GetRendererInterfaceBinders().TryBindInterface(interface_name,
                                                        interface_pipe, frame);
}

}  // namespace content
