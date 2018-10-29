// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SHARED_WORKER_SHARED_WORKER_SERVICE_IMPL_H_
#define CONTENT_BROWSER_SHARED_WORKER_SHARED_WORKER_SERVICE_IMPL_H_

#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/shared_worker/shared_worker_host.h"
#include "content/common/service_worker/service_worker_provider.mojom.h"
#include "content/common/shared_worker/shared_worker_connector.mojom.h"
#include "content/common/shared_worker/shared_worker_factory.mojom.h"
#include "content/public/browser/shared_worker_service.h"
#include "services/network/public/cpp/resource_response.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/blink/public/mojom/shared_worker/shared_worker_main_script_load_params.mojom.h"

namespace blink {
class MessagePortChannel;
}

namespace content {

class ChromeAppCacheService;
class SharedWorkerInstance;
class SharedWorkerHost;
class StoragePartitionImpl;
struct SubresourceLoaderParams;

// Shared helper function
bool IsShuttingDown(RenderProcessHost* host);

// Created per StoragePartition.
class CONTENT_EXPORT SharedWorkerServiceImpl : public SharedWorkerService {
 public:
  SharedWorkerServiceImpl(
      StoragePartitionImpl* storage_partition,
      scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
      scoped_refptr<ChromeAppCacheService> appcache_service);
  ~SharedWorkerServiceImpl() override;

  // SharedWorkerService implementation.
  bool TerminateWorker(const GURL& url,
                       const std::string& name,
                       const url::Origin& constructor_origin) override;

  void TerminateAllWorkersForTesting(base::OnceClosure callback);
  void SetWorkerTerminationCallbackForTesting(base::OnceClosure callback);

  // Creates the worker if necessary or connects to an already existing worker.
  void ConnectToWorker(
      int process_id,
      int frame_id,
      mojom::SharedWorkerInfoPtr info,
      mojom::SharedWorkerClientPtr client,
      blink::mojom::SharedWorkerCreationContextType creation_context_type,
      const blink::MessagePortChannel& port,
      scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory);

  void DestroyHost(SharedWorkerHost* host);

  StoragePartitionImpl* storage_partition() { return storage_partition_; }

 private:
  friend class SharedWorkerServiceImplTest;
  friend class SharedWorkerHostTest;
  FRIEND_TEST_ALL_PREFIXES(NetworkServiceRestartBrowserTest, SharedWorker);

  static void AddAdditionalRequestHeaders(
      network::ResourceRequest* resource_request,
      BrowserContext* browser_context);

  void CreateWorker(
      std::unique_ptr<SharedWorkerInstance> instance,
      mojom::SharedWorkerClientPtr client,
      int process_id,
      int frame_id,
      const blink::MessagePortChannel& message_port,
      scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory);
  void DidCreateScriptLoader(
      std::unique_ptr<SharedWorkerInstance> instance,
      base::WeakPtr<SharedWorkerHost> host,
      mojom::SharedWorkerClientPtr client,
      int process_id,
      int frame_id,
      const blink::MessagePortChannel& message_port,
      mojom::ServiceWorkerProviderInfoForSharedWorkerPtr
          service_worker_provider_info,
      network::mojom::URLLoaderFactoryAssociatedPtrInfo
          main_script_loader_factory,
      std::unique_ptr<URLLoaderFactoryBundleInfo> subresource_loader_factories,
      blink::mojom::SharedWorkerMainScriptLoadParamsPtr main_script_load_params,
      base::Optional<SubresourceLoaderParams> subresource_loader_params,
      bool success);
  void StartWorker(
      std::unique_ptr<SharedWorkerInstance> instance,
      base::WeakPtr<SharedWorkerHost> host,
      mojom::SharedWorkerClientPtr client,
      int process_id,
      int frame_id,
      const blink::MessagePortChannel& message_port,
      mojom::ServiceWorkerProviderInfoForSharedWorkerPtr
          service_worker_provider_info,
      network::mojom::URLLoaderFactoryAssociatedPtrInfo
          main_script_loader_factory,
      std::unique_ptr<URLLoaderFactoryBundleInfo> subresource_loader_factories,
      blink::mojom::SharedWorkerMainScriptLoadParamsPtr main_script_load_params,
      base::Optional<SubresourceLoaderParams> subresource_loader_params);

  // Returns nullptr if there is no such host.
  SharedWorkerHost* FindSharedWorkerHost(int process_id, int route_id);
  SharedWorkerHost* FindAvailableSharedWorkerHost(
      const SharedWorkerInstance& instance);

  std::set<std::unique_ptr<SharedWorkerHost>, base::UniquePtrComparator>
      worker_hosts_;
  base::OnceClosure terminate_all_workers_callback_;

  // |storage_partition_| owns |this|.
  StoragePartitionImpl* const storage_partition_;
  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context_;
  scoped_refptr<ChromeAppCacheService> appcache_service_;

  base::WeakPtrFactory<SharedWorkerServiceImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SharedWorkerServiceImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SHARED_WORKER_SHARED_WORKER_SERVICE_IMPL_H_
