// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PRECONNECT_PRECONNECT_MANAGER_IMPL_H_
#define CONTENT_BROWSER_PRELOADING_PRECONNECT_PRECONNECT_MANAGER_IMPL_H_

#include <list>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/containers/id_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/browser/preloading/proxy_lookup_client_impl.h"
#include "content/browser/preloading/resolve_host_client_impl.h"
#include "content/public/browser/preconnect_manager.h"
#include "content/public/browser/preconnect_request.h"
#include "content/public/browser/storage_partition_config.h"
#include "net/base/network_anonymization_key.h"
#include "services/network/public/mojom/connection_change_observer_client.mojom.h"
#include "url/gurl.h"

namespace network::mojom {
class NetworkContext;
}  // namespace network::mojom

namespace content {

class BrowserContext;

// Stores the status of all preconnects associated with a given |url|.
struct CONTENT_EXPORT PreresolveInfo {
  PreresolveInfo(const GURL& url, size_t count);

  PreresolveInfo(const PreresolveInfo&) = delete;
  PreresolveInfo& operator=(const PreresolveInfo&) = delete;

  ~PreresolveInfo();

  bool is_done() const { return queued_count == 0 && inflight_count == 0; }

  GURL url;
  size_t queued_count;
  size_t inflight_count = 0;
  bool was_canceled = false;
  std::unique_ptr<PreconnectStats> stats;
};

// Stores all data need for running a preresolve and a subsequent optional
// preconnect for a |url|.
struct CONTENT_EXPORT PreresolveJob {
  PreresolveJob(
      const GURL& url,
      int num_sockets,
      bool allow_credentials,
      net::NetworkAnonymizationKey network_anonymization_key,
      net::NetworkTrafficAnnotationTag traffic_annotation_tag,
      std::optional<content::StoragePartitionConfig> storage_partition_config,
      std::optional<net::ConnectionKeepAliveConfig> keepalive_config,
      mojo::PendingRemote<network::mojom::ConnectionChangeObserverClient>
          connection_change_observer_client,
      PreresolveInfo* info);

  PreresolveJob(const PreresolveJob&) = delete;
  PreresolveJob& operator=(const PreresolveJob&) = delete;

  PreresolveJob(content::PreconnectRequest preconnect_request,
                PreresolveInfo* info,
                net::NetworkTrafficAnnotationTag traffic_annotation_tag);
  PreresolveJob(PreresolveJob&& other);

  ~PreresolveJob();

  bool need_preconnect() const {
    return num_sockets > 0 && !(info && info->was_canceled);
  }

  GURL url;
  int num_sockets;
  bool allow_credentials;
  net::NetworkAnonymizationKey network_anonymization_key;
  net::NetworkTrafficAnnotationTag traffic_annotation_tag;
  // The default for the profile is used if this is absent.
  std::optional<content::StoragePartitionConfig> storage_partition_config;

  std::optional<net::ConnectionKeepAliveConfig> keepalive_config;
  mojo::PendingRemote<network::mojom::ConnectionChangeObserverClient>
      connection_change_observer_client;
  // Raw pointer usage is fine here because even though PreresolveJob can
  // outlive PreresolveInfo. It's only accessed on PreconnectManager class
  // context and PreresolveInfo lifetime is tied to PreconnectManager.
  // May be equal to nullptr in case of detached job.
  raw_ptr<PreresolveInfo, DanglingUntriaged> info;
  std::unique_ptr<ResolveHostClientImpl> resolve_host_client;
  std::unique_ptr<ProxyLookupClientImpl> proxy_lookup_client;
  base::TimeTicks creation_time;
};

class CONTENT_EXPORT PreconnectManagerImpl : public PreconnectManager {
 public:
  static const size_t kMaxInflightPreresolves = 3;

  PreconnectManagerImpl(base::WeakPtr<PreconnectManager::Delegate> delegate,
                        content::BrowserContext* browser_context);

  PreconnectManagerImpl(const PreconnectManagerImpl&) = delete;
  PreconnectManagerImpl& operator=(const PreconnectManagerImpl&) = delete;

  ~PreconnectManagerImpl() override;

  void Start(const GURL& url,
             std::vector<content::PreconnectRequest> requests,
             net::NetworkTrafficAnnotationTag traffic_annotation) override;

  void StartPreresolveHost(
      const GURL& url,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      net::NetworkTrafficAnnotationTag traffic_annotation,
      const content::StoragePartitionConfig* storage_partition_config) override;
  void StartPreresolveHosts(
      const std::vector<GURL>& urls,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      net::NetworkTrafficAnnotationTag traffic_annotation,
      const content::StoragePartitionConfig* storage_partition_config) override;
  void StartPreconnectUrl(
      const GURL& url,
      bool allow_credentials,
      net::NetworkAnonymizationKey network_anonymization_key,
      net::NetworkTrafficAnnotationTag traffic_annotation,
      const content::StoragePartitionConfig* storage_partition_config,
      std::optional<net::ConnectionKeepAliveConfig> keepalive_config,
      mojo::PendingRemote<network::mojom::ConnectionChangeObserverClient>
          connection_change_observer_client) override;

  void Stop(const GURL& url) override;

  base::WeakPtr<PreconnectManager> GetWeakPtr() override;

  void SetNetworkContextForTesting(
      network::mojom::NetworkContext* network_context) override;
  void SetObserverForTesting(Observer* observer) override;

 private:
  using PreresolveJobMap = base::IDMap<std::unique_ptr<PreresolveJob>>;
  using PreresolveJobId = PreresolveJobMap::KeyType;
  friend class PreconnectManagerImplTest;

  void PreconnectUrl(
      const GURL& url,
      int num_sockets,
      bool allow_credentials,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      const content::StoragePartitionConfig* storage_partition_config,
      std::optional<net::ConnectionKeepAliveConfig> keepalive_config,
      mojo::PendingRemote<network::mojom::ConnectionChangeObserverClient>
          connection_change_observer_client) const;
  std::unique_ptr<ResolveHostClientImpl> PreresolveUrl(
      const GURL& url,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      const content::StoragePartitionConfig* storage_partition_config,
      ResolveHostCallback callback) const;
  std::unique_ptr<ProxyLookupClientImpl> LookupProxyForUrl(
      const GURL& url,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      const content::StoragePartitionConfig* storage_partition_config,
      ProxyLookupClientImpl::ProxyLookupCallback callback) const;

  void TryToLaunchPreresolveJobs();
  void OnPreresolveFinished(PreresolveJobId job_id, bool success);
  void OnProxyLookupFinished(PreresolveJobId job_id, bool success);
  void FinishPreresolveJob(PreresolveJobId job_id, bool success);
  void AllPreresolvesForUrlFinished(PreresolveInfo* info);

  // NOTE: Returns a non-null pointer outside of unittesting contexts.
  network::mojom::NetworkContext* GetNetworkContext(
      const content::StoragePartitionConfig* storage_partition_config) const;

  base::WeakPtr<Delegate> delegate_;
  const raw_ptr<content::BrowserContext> browser_context_;
  std::list<PreresolveJobId> queued_jobs_;
  PreresolveJobMap preresolve_jobs_;
  std::map<GURL, std::unique_ptr<PreresolveInfo>> preresolve_info_;
  size_t inflight_preresolves_count_ = 0;

  // Only used in tests.
  raw_ptr<network::mojom::NetworkContext> network_context_ = nullptr;
  raw_ptr<Observer> observer_ = nullptr;

  base::WeakPtrFactory<PreconnectManagerImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PRECONNECT_PRECONNECT_MANAGER_IMPL_H_
