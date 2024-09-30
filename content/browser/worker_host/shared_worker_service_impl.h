// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WORKER_HOST_SHARED_WORKER_SERVICE_IMPL_H_
#define CONTENT_BROWSER_WORKER_HOST_SHARED_WORKER_SERVICE_IMPL_H_

#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/containers/unique_ptr_adapters.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/worker_host/shared_worker_host.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/shared_worker_service.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/storage_access_api/status.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/loader/fetch_client_settings_object.mojom.h"
#include "third_party/blink/public/mojom/worker/shared_worker_connector.mojom.h"
#include "third_party/blink/public/mojom/worker/shared_worker_factory.mojom.h"
#include "third_party/blink/public/mojom/worker/worker_main_script_load_params.mojom.h"

namespace blink {
class MessagePortChannel;
class StorageKey;
}  // namespace blink

namespace url {
class Origin;
}

namespace content {

class SharedWorkerHost;
class SharedWorkerInstance;
class StoragePartitionImpl;

// Created per StoragePartition.
class CONTENT_EXPORT SharedWorkerServiceImpl : public SharedWorkerService {
 public:
  SharedWorkerServiceImpl(
      StoragePartitionImpl* storage_partition,
      scoped_refptr<ServiceWorkerContextWrapper> service_worker_context);

  SharedWorkerServiceImpl(const SharedWorkerServiceImpl&) = delete;
  SharedWorkerServiceImpl& operator=(const SharedWorkerServiceImpl&) = delete;

  ~SharedWorkerServiceImpl() override;

  // SharedWorkerService implementation.
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void EnumerateSharedWorkers(Observer* observer) override;
  bool TerminateWorker(const GURL& url,
                       const std::string& name,
                       const blink::StorageKey& storage_key,
                       const blink::mojom::SharedWorkerSameSiteCookies
                           same_site_cookies) override;
  void Shutdown() override;

  // Uses |url_loader_factory| to load workers' scripts instead of
  // StoragePartition's ReconnectableURLLoaderFactoryForIOThread.
  void SetURLLoaderFactoryForTesting(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  // Creates the worker if necessary or connects to an already existing worker.
  // `storage_key_override` is used to grant access to unpartitioned workers
  // in a partitioned context.
  void ConnectToWorker(
      GlobalRenderFrameHostId client_render_frame_host_id,
      blink::mojom::SharedWorkerInfoPtr info,
      mojo::PendingRemote<blink::mojom::SharedWorkerClient> client,
      blink::mojom::SharedWorkerCreationContextType creation_context_type,
      const blink::MessagePortChannel& port,
      scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory,
      ukm::SourceId client_ukm_source_id,
      const std::optional<blink::StorageKey>& storage_key_override);

  // Returns the SharedWorkerHost associated with this token. Clients should
  // not hold on to the pointer, as it may become invalid when the worker exits.
  SharedWorkerHost* GetSharedWorkerHostFromToken(
      const blink::SharedWorkerToken& worker_token) const;

  // Virtual for testing.
  virtual void DestroyHost(SharedWorkerHost* host);

  void NotifyWorkerCreated(const blink::SharedWorkerToken& shared_worker_token,
                           int worker_process_id,
                           const url::Origin& security_origin,
                           const base::UnguessableToken& dev_tools_token);
  void NotifyBeforeWorkerDestroyed(
      const blink::SharedWorkerToken& shared_worker_token);
  void NotifyClientAdded(const blink::SharedWorkerToken& shared_worker_token,
                         GlobalRenderFrameHostId render_frame_host_id);
  void NotifyClientRemoved(const blink::SharedWorkerToken& shared_worker_token,
                           GlobalRenderFrameHostId render_frame_host_id);

  StoragePartitionImpl* storage_partition() { return storage_partition_; }

 private:
  friend class SharedWorkerHostTest;
  friend class SharedWorkerServiceImplTest;
  friend class TestSharedWorkerServiceImpl;
  friend class WorkerTest;
  FRIEND_TEST_ALL_PREFIXES(NetworkServiceRestartBrowserTest, SharedWorker);

  // Creates a new worker in the creator's renderer process.
  SharedWorkerHost* CreateWorker(
      RenderFrameHostImpl& creator,
      const SharedWorkerInstance& instance,
      std::vector<network::mojom::ContentSecurityPolicyPtr>
          content_security_policies,
      blink::mojom::FetchClientSettingsObjectPtr
          outside_fetch_client_settings_object,
      const std::string& storage_domain,
      const blink::MessagePortChannel& message_port,
      scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory,
      const std::optional<blink::StorageKey>& storage_key_override);

  void StartWorker(base::WeakPtr<SharedWorkerHost> host,
                   const blink::MessagePortChannel& message_port,
                   blink::mojom::FetchClientSettingsObjectPtr
                       outside_fetch_client_settings_object,
                   std::optional<WorkerScriptFetcherResult> result);

  // Returns nullptr if there is no such host.
  SharedWorkerHost* FindMatchingSharedWorkerHost(
      const GURL& url,
      const std::string& name,
      const blink::StorageKey& storage_key,
      const blink::mojom::SharedWorkerSameSiteCookies same_site_cookies);

  void ScriptLoadFailed(
      mojo::PendingRemote<blink::mojom::SharedWorkerClient> client,
      const std::string& error_message);

  std::set<std::unique_ptr<SharedWorkerHost>, base::UniquePtrComparator>
      worker_hosts_;
  base::flat_map<blink::SharedWorkerToken,
                 raw_ptr<SharedWorkerHost, CtnExperimental>>
      shared_worker_hosts_;

  // |storage_partition_| owns |this|.
  const raw_ptr<StoragePartitionImpl> storage_partition_;
  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_override_;

  // Keeps a reference count of each worker-client pair so as to not send
  // duplicate OnClientAdded() notifications if the same frame connects multiple
  // times to the same shared worker. Note that this is a situation unique to
  // shared worker and cannot happen with dedicated workers and service workers.
  base::flat_map<std::pair<blink::SharedWorkerToken, GlobalRenderFrameHostId>,
                 int>
      shared_worker_client_counts_;

  base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<SharedWorkerServiceImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_WORKER_HOST_SHARED_WORKER_SERVICE_IMPL_H_
