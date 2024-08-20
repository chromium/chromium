// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_HOST_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_HOST_H_

#include <memory>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "base/task/single_thread_task_runner.h"
#include "content/browser/browser_interface_broker_impl.h"
#include "content/browser/buckets/bucket_context.h"
#include "content/browser/renderer_host/code_cache_host_impl.h"
#include "content/common/content_export.h"
#include "content/public/browser/render_process_host.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "net/base/network_isolation_key.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom-forward.h"
#include "third_party/blink/public/mojom/blob/blob_url_store.mojom-forward.h"
#include "third_party/blink/public/mojom/broadcastchannel/broadcast_channel.mojom.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"
#include "third_party/blink/public/mojom/loader/code_cache.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_container.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_container_type.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_provider.mojom.h"
#include "third_party/blink/public/mojom/usb/web_usb_service.mojom-forward.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"
#include "third_party/blink/public/mojom/webtransport/web_transport_connector.mojom.h"
#include "url/origin.h"

#if !BUILDFLAG(IS_ANDROID)
#include "third_party/blink/public/mojom/hid/hid.mojom-forward.h"
#endif

namespace content {

class ServiceWorkerContainerHostForServiceWorker;
class ServiceWorkerContextCore;
class ServiceWorkerVersion;
struct ServiceWorkerVersionBaseInfo;

// ServiceWorkerHost is the host of a service worker execution context in the
// renderer process. One ServiceWorkerHost instance hosts one service worker
// execution context instance.
//
// Lives on the UI thread.
class CONTENT_EXPORT ServiceWorkerHost : public BucketContext,
                                         public base::SupportsUserData {
 public:
  ServiceWorkerHost(mojo::PendingAssociatedReceiver<
                        blink::mojom::ServiceWorkerContainerHost> host_receiver,
                    ServiceWorkerVersion& version,
                    base::WeakPtr<ServiceWorkerContextCore> context);

  ServiceWorkerHost(const ServiceWorkerHost&) = delete;
  ServiceWorkerHost& operator=(const ServiceWorkerHost&) = delete;

  ~ServiceWorkerHost() override;

  int worker_process_id() const { return worker_process_id_; }
  ServiceWorkerVersion* version() const { return version_; }
  const blink::ServiceWorkerToken& token() const { return token_; }

  service_manager::InterfaceProvider& remote_interfaces() {
    return remote_interfaces_;
  }

  // Completes initialization of this provider host. It is called once a
  // renderer process has been found to host the worker.
  void CompleteStartWorkerPreparation(
      int process_id,
      mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker>
          broker_receiver,
      mojo::PendingRemote<service_manager::mojom::InterfaceProvider>
          interface_provider_remote);

  void CreateWebTransportConnector(
      mojo::PendingReceiver<blink::mojom::WebTransportConnector> receiver);
  // Used when EagerCacheStorageSetupForServiceWorkers is disabled, or when
  // setup for eager cache storage has failed.
  void BindCacheStorage(
      mojo::PendingReceiver<blink::mojom::CacheStorage> receiver);

#if !BUILDFLAG(IS_ANDROID)
  void BindHidService(mojo::PendingReceiver<blink::mojom::HidService> receiver);
#endif  // !BUILDFLAG(IS_ANDROID)

  void BindUsbService(
      mojo::PendingReceiver<blink::mojom::WebUsbService> receiver);

  ServiceWorkerContainerHostForServiceWorker* container_host() {
    return container_host_.get();
  }

  net::NetworkIsolationKey GetNetworkIsolationKey() const;
  net::NetworkAnonymizationKey GetNetworkAnonymizationKey() const;
  const base::UnguessableToken& GetReportingSource() const;

  StoragePartition* GetStoragePartition() const;

  void CreateCodeCacheHost(
      mojo::PendingReceiver<blink::mojom::CodeCacheHost> receiver);

  void CreateBroadcastChannelProvider(
      mojo::PendingReceiver<blink::mojom::BroadcastChannelProvider> receiver);

  void CreateBlobUrlStoreProvider(
      mojo::PendingReceiver<blink::mojom::BlobURLStore> receiver);

  void CreateBucketManagerHost(
      mojo::PendingReceiver<blink::mojom::BucketManagerHost> receiver);

  base::WeakPtr<ServiceWorkerHost> GetWeakPtr();

  void ReportNoBinderForInterface(const std::string& error);

  // BucketContext:
  blink::StorageKey GetBucketStorageKey() override;
  blink::mojom::PermissionStatus GetPermissionStatus(
      blink::PermissionType permission_type) override;
  void BindCacheStorageForBucket(
      const storage::BucketInfo& bucket,
      mojo::PendingReceiver<blink::mojom::CacheStorage> receiver) override;
  void GetSandboxedFileSystemForBucket(
      const storage::BucketInfo& bucket,
      const std::vector<std::string>& directory_path_components,
      blink::mojom::FileSystemAccessManager::GetSandboxedFileSystemCallback
          callback) override;
  storage::BucketClientInfo GetBucketClientInfo() const override;

  void BindAIManager(mojo::PendingReceiver<blink::mojom::AIManager> receiver);

 private:
  RenderProcessHost* GetProcessHost() const;

  int worker_process_id_;

  // The service worker being hosted. Raw pointer is safe because the version
  // owns |this|.
  const raw_ptr<ServiceWorkerVersion> version_;

  // A unique identifier for this service worker instance. This is unique across
  // the browser process, but not persistent across service worker restarts.
  // This token is generated every time the worker starts (i.e.,
  // ServiceWorkerHost is created), and is plumbed through to the corresponding
  // ServiceWorkerGlobalScope in the renderer process.
  const blink::ServiceWorkerToken token_;

  // BrowserInterfaceBroker implementation through which this
  // ServiceWorkerHost exposes worker-scoped Mojo services to the corresponding
  // service worker in the renderer.
  //
  // The interfaces that can be requested from this broker are defined in the
  // content/browser/browser_interface_binders.cc file, in the functions which
  // take a `ServiceWorkerHost*` parameter.
  BrowserInterfaceBrokerImpl<ServiceWorkerHost,
                             const ServiceWorkerVersionBaseInfo&>
      broker_;
  mojo::Receiver<blink::mojom::BrowserInterfaceBroker> broker_receiver_{
      &broker_};

  std::unique_ptr<ServiceWorkerContainerHostForServiceWorker> container_host_;

  service_manager::InterfaceProvider remote_interfaces_{
      base::SingleThreadTaskRunner::GetCurrentDefault()};

  // CodeCacheHost processes requests to fetch / write generated code for
  // JavaScript / WebAssembly resources.
  std::unique_ptr<CodeCacheHostImpl::ReceiverSet> code_cache_host_receivers_;

  mojo::AssociatedReceiver<blink::mojom::ServiceWorkerContainerHost>
      host_receiver_;

  base::WeakPtrFactory<ServiceWorkerHost> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_HOST_H_
