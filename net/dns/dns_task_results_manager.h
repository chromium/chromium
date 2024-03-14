// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_DNS_TASK_RESULTS_MANAGER_H_
#define NET_DNS_DNS_TASK_RESULTS_MANAGER_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_export.h"
#include "net/dns/host_resolver.h"
#include "net/dns/host_resolver_dns_task.h"
#include "net/dns/public/dns_query_type.h"
#include "net/dns/public/host_resolver_results.h"
#include "net/log/net_log_with_source.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/scheme_host_port.h"

namespace net {

// Creates and updates intermediate service endpoints while resolving a host.
// This class is designed to have a 1:1 relationship with a HostResolverDnsTask
// and expects to be notified every time a DnsTransaction is completed. When
// notified, tries to create and update service endpoints from DNS responses
// received so far.
//
// If the A response comes before the AAAA response, delays service endpoints
// creation/update until an AAAA response is received or the AAAA query is
// timed out.
class NET_EXPORT_PRIVATE DnsTaskResultsManager {
 public:
  // Time to wait for a AAAA response after receiving an A response.
  static constexpr base::TimeDelta kResolutionDelay = base::Milliseconds(50);

  // Interface for watching for intermediate service endpoints updates.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Called when service endpoints are updated.
    virtual void OnServiceEndpointsUpdated() = 0;
  };

  // TODO(crbug.com/41493696): Update HostResolverManager::JobKey to use
  // HostResolver::Host so that HostResolverManager::Job can create an instance
  // of this class.
  DnsTaskResultsManager(Delegate* delegate,
                        HostResolver::Host host,
                        DnsQueryTypeSet query_types,
                        const NetLogWithSource& net_log);
  ~DnsTaskResultsManager();

  DnsTaskResultsManager(const DnsTaskResultsManager&) = delete;
  DnsTaskResultsManager& operator=(const DnsTaskResultsManager&) = delete;

  // Processes a query response represented by HostResolverInternalResults.
  // Expected be called when a DnsTransaction is completed.
  void ProcessDnsTransactionResults(
      DnsQueryType query_type,
      const std::set<std::unique_ptr<HostResolverInternalResult>>& results);

  // Returns the current service endpoints. The results could change over time.
  // Use the delegate's OnServiceEndpointsUpdated() to watch for updates.
  const std::vector<ServiceEndpoint>& GetCurrentEndpoints() const;

  // Returns all DNS record aliases, found as a result of A, AAAA, and HTTPS
  // queries. The results could change over time.
  const std::set<std::string>& GetAliases() const;

  // True when a HTTP response is received. When true, call sites can start
  // cryptographic handshakes since Chrome doesn't support HTTPS follow-up
  // queries yet.
  bool IsMetadataReady() const;

  bool IsResolutionDelayTimerRunningForTest() {
    return resolution_delay_timer_.IsRunning();
  }

 private:
  struct PerDomainResult;

  PerDomainResult& GetOrCreatePerDomainResult(const std::string& domain_name);

  void OnAaaaResolutionTimedout();

  void UpdateEndpoints();

  // Checks all per domain results and return true when there is at least one
  // valid address.
  bool HasIpv4Addresses();

  void RecordResolutionDelayResult(bool timedout);

  const raw_ptr<Delegate> delegate_;
  const HostResolver::Host host_;
  const DnsQueryTypeSet query_types_;
  const NetLogWithSource net_log_;

  std::vector<ServiceEndpoint> current_endpoints_;

  bool is_metadata_ready_ = false;
  bool aaaa_response_received_ = false;

  std::set<std::string> aliases_;

  std::map</*domain_name=*/std::string, std::unique_ptr<PerDomainResult>>
      per_domain_results_;

  base::TimeTicks resolution_delay_start_time_;
  base::OneShotTimer resolution_delay_timer_;
};

}  // namespace net

#endif  // NET_DNS_DNS_TASK_RESULTS_MANAGER_H_
