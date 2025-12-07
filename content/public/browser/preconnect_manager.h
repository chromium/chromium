// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PRECONNECT_MANAGER_H_
#define CONTENT_PUBLIC_BROWSER_PRECONNECT_MANAGER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/public/browser/preconnect_request.h"
#include "content/public/browser/storage_partition_config.h"
#include "net/base/network_anonymization_key.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/mojom/connection_change_observer_client.mojom.h"
#include "url/gurl.h"

namespace network::mojom {
class NetworkContext;
}

namespace content {

// Holds information about a preconnect request for a given origin as part of
// a call to PreconnectManager::Start. Provided as part of PreconnectStats.
struct CONTENT_EXPORT PreconnectedRequestStats {
  PreconnectedRequestStats(const url::Origin& origin, bool was_preconnected);
  PreconnectedRequestStats(const PreconnectedRequestStats& other);
  ~PreconnectedRequestStats();

  url::Origin origin;
  bool was_preconnected;
};

// Details the results of a call to PreconnectManager::Start for the given URL
// and provided to PreconnectManager::Delegate::PreconnectFinished.
struct CONTENT_EXPORT PreconnectStats {
  explicit PreconnectStats(const GURL& url);

  // Disable copying for efficiency.
  PreconnectStats(const PreconnectStats&) = delete;
  PreconnectStats& operator=(const PreconnectStats&) = delete;

  ~PreconnectStats();

  GURL url;
  base::TimeTicks start_time;
  std::vector<PreconnectedRequestStats> requests_stats;
};

// PreconnectManager is responsible for preresolving and preconnecting to
// origins based on the input list of URLs.
//  - The input list of URLs is associated with a main frame url that can be
//    used for cancelling.
//  - Limits the total number of preresolves in flight.
//  - Preresolves an URL before preconnecting to it to have a better control on
//    number of speculative dns requests in flight.
//  - When stopped, waits for the pending preresolve requests to finish without
//    issuing preconnects for them.
//  - All methods of the class must be called on the UI thread.
class CONTENT_EXPORT PreconnectManager {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Called when a preconnect to |preconnect_url| is initiated for |url|.
    // Note: This is only called in response to Start (and not for methods such
    // as StartPreresolveHost or StartPreconnectUrl).
    virtual void PreconnectInitiated(const GURL& url,
                                     const GURL& preconnect_url) = 0;

    // Called when all preresolve jobs for the |stats->url| are finished. Note
    // that some preconnect jobs can be still in progress, because they are
    // fire-and-forget.
    // Is called on the UI thread.
    virtual void PreconnectFinished(std::unique_ptr<PreconnectStats> stats) = 0;

    // Returns true if the preconnect functionality is enabled.
    virtual bool IsPreconnectEnabled() = 0;
  };

  // An observer for testing.
  class Observer {
   public:
    virtual ~Observer() = default;

    virtual void OnPreconnectUrl(const GURL& url,
                                 int num_sockets,
                                 bool allow_credentials) {}

    virtual void OnPreresolveFinished(
        const GURL& url,
        const net::NetworkAnonymizationKey& network_anonymization_key,
        mojo::PendingRemote<network::mojom::ConnectionChangeObserverClient>&
            observer,
        bool success) {}
    virtual void OnProxyLookupFinished(
        const GURL& url,
        const net::NetworkAnonymizationKey& network_anonymization_key,
        bool success) {}
  };

  virtual ~PreconnectManager() = default;

  // Starts preconnect and preresolve jobs associated with |url|.
  virtual void Start(const GURL& url,
                     std::vector<PreconnectRequest> requests,
                     net::NetworkTrafficAnnotationTag traffic_annotation) = 0;

  // Starts special preconnect and preresolve jobs that are not cancellable and
  // don't report about their completion. They are considered more important
  // than trackable requests thus they are put in the front of the jobs queue.
  //
  // |network_anonymization_key| specifies the key that the corresponding
  // network requests are expected to use. If a request is issued with a
  // different key, it may not use the prefetched DNS entry or preconnected
  // socket.
  virtual void StartPreresolveHost(
      const GURL& url,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      net::NetworkTrafficAnnotationTag traffic_annotation,
      const StoragePartitionConfig* storage_partition_config) = 0;
  virtual void StartPreresolveHosts(
      const std::vector<GURL>& urls,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      net::NetworkTrafficAnnotationTag traffic_annotation,
      const StoragePartitionConfig* storage_partition_config) = 0;
  virtual void StartPreconnectUrl(
      const GURL& url,
      bool allow_credentials,
      net::NetworkAnonymizationKey network_anonymization_key,
      net::NetworkTrafficAnnotationTag traffic_annotation,
      const StoragePartitionConfig* storage_partition_config,
      std::optional<net::ConnectionKeepAliveConfig> keepalive_config,
      mojo::PendingRemote<network::mojom::ConnectionChangeObserverClient>
          connection_change_observer_client) = 0;

  // No additional jobs associated with the |url| will be queued after this.
  virtual void Stop(const GURL& url) = 0;

  virtual base::WeakPtr<PreconnectManager> GetWeakPtr() = 0;

  virtual void SetNetworkContextForTesting(
      network::mojom::NetworkContext* network_context) = 0;

  virtual void SetObserverForTesting(Observer* observer) = 0;

  static std::unique_ptr<PreconnectManager> Create(
      base::WeakPtr<PreconnectManager::Delegate> delegate,
      BrowserContext* browser_context);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PRECONNECT_MANAGER_H_
