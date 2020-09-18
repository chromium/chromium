// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_host.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/task/post_task.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/service_worker/service_worker_consts.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/browser/webtransport/quic_transport_connector_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/origin_util.h"
#include "mojo/public/cpp/bindings/message.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/messaging/message_port_channel.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_client.mojom.h"

namespace content {

namespace {

void CreateQuicTransportConnectorImpl(
    int process_id,
    const url::Origin& origin,
    mojo::PendingReceiver<blink::mojom::QuicTransportConnector> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* process = RenderProcessHost::FromID(process_id);
  if (!process)
    return;

  mojo::MakeSelfOwnedReceiver(std::make_unique<QuicTransportConnectorImpl>(
                                  process_id, /*frame=*/nullptr, origin,
                                  net::NetworkIsolationKey(origin, origin)),
                              std::move(receiver));
}

}  // anonymous namespace

ServiceWorkerHost::ServiceWorkerHost(
    mojo::PendingAssociatedReceiver<blink::mojom::ServiceWorkerContainerHost>
        host_receiver,
    ServiceWorkerVersion* version,
    base::WeakPtr<ServiceWorkerContextCore> context)
    : version_(version),
      container_host_(std::make_unique<content::ServiceWorkerContainerHost>(
          std::move(context))),
      host_receiver_(container_host_.get(), std::move(host_receiver)) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  DCHECK(version_);

  container_host_->set_service_worker_host(this);
  container_host_->UpdateUrls(
      version_->script_url(),
      net::SiteForCookies::FromUrl(version_->script_url()), version_->origin());
}

ServiceWorkerHost::~ServiceWorkerHost() {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  // Explicitly destroy the ServiceWorkerContainerHost to release
  // ServiceWorkerObjectHosts and ServiceWorkerRegistrationObjectHosts owned by
  // that. Otherwise, this destructor can trigger their Mojo connection error
  // handlers, which would call back into halfway destroyed |this|. This is
  // because they are associated with the ServiceWorker interface, which can be
  // destroyed while in this destructor (|version_|'s |event_dispatcher_|).
  // See https://crbug.com/854993.
  container_host_.reset();
}

void ServiceWorkerHost::CompleteStartWorkerPreparation(
    int process_id,
    mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker>
        broker_receiver) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  DCHECK_EQ(ChildProcessHost::kInvalidUniqueID, worker_process_id_);
  DCHECK_NE(ChildProcessHost::kInvalidUniqueID, process_id);
  worker_process_id_ = process_id;
  broker_receiver_.Bind(std::move(broker_receiver));
}

void ServiceWorkerHost::CreateQuicTransportConnector(
    mojo::PendingReceiver<blink::mojom::QuicTransportConnector> receiver) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  RunOrPostTaskOnThread(
      FROM_HERE, BrowserThread::UI,
      base::BindOnce(&CreateQuicTransportConnectorImpl, worker_process_id_,
                     version_->origin(), std::move(receiver)));
}

void ServiceWorkerHost::BindCacheStorage(
    mojo::PendingReceiver<blink::mojom::CacheStorage> receiver) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  DCHECK(!base::FeatureList::IsEnabled(
      blink::features::kEagerCacheStorageSetupForServiceWorkers));
  version_->embedded_worker()->BindCacheStorage(std::move(receiver));
}

base::WeakPtr<ServiceWorkerHost> ServiceWorkerHost::GetWeakPtr() {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  return weak_factory_.GetWeakPtr();
}

void ServiceWorkerHost::ReportNoBinderForInterface(const std::string& error) {
  broker_receiver_.ReportBadMessage(error + " for the service worker scope");
}

}  // namespace content
