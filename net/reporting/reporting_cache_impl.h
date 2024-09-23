// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_REPORTING_REPORTING_CACHE_IMPL_H_
#define NET_REPORTING_REPORTING_CACHE_IMPL_H_

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "base/values.h"
#include "net/base/isolation_info.h"
#include "net/base/network_anonymization_key.h"
#include "net/reporting/reporting_cache.h"
#include "net/reporting/reporting_context.h"
#include "net/reporting/reporting_endpoint.h"
#include "net/reporting/reporting_header_parser.h"
#include "net/reporting/reporting_report.h"
#include "net/reporting/reporting_target_type.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net {

class ReportingCacheImpl : public ReportingCache {
 public:
  explicit ReportingCacheImpl(
      ReportingContext* context,
      const base::flat_map<std::string, GURL>& enterprise_reporting_endpoints);

  ReportingCacheImpl(const ReportingCacheImpl&) = delete;
  ReportingCacheImpl& operator=(const ReportingCacheImpl&) = delete;

  ~ReportingCacheImpl() override;

  // ReportingCache implementation
  void AddReport(const std::optional<base::UnguessableToken>& reporting_source,
                 const NetworkAnonymizationKey& network_anonymization_key,
                 const GURL& url,
                 const std::string& user_agent,
                 const std::string& group_name,
                 const std::string& type,
                 base::Value::Dict body,
                 int depth,
                 base::TimeTicks queued,
                 int attempts,
                 ReportingTargetType target_type) override;
  void GetReports(
      std::vector<raw_ptr<const ReportingReport, VectorExperimental>>*
          reports_out) const override;
  base::Value GetReportsAsValue() const override;
  std::vector<raw_ptr<const ReportingReport, VectorExperimental>>
  GetReportsToDeliver() override;
  std::vector<raw_ptr<const ReportingReport, VectorExperimental>>
  GetReportsToDeliverForSource(
      const base::UnguessableToken& reporting_source) override;
  void ClearReportsPending(
      const std::vector<raw_ptr<const ReportingReport, VectorExperimental>>&
          reports) override;
  void IncrementReportsAttempts(
      const std::vector<raw_ptr<const ReportingReport, VectorExperimental>>&
          reports) override;
  base::flat_map<url::Origin, std::vector<ReportingEndpoint>>
  GetV1ReportingEndpointsByOrigin() const override;
  void IncrementEndpointDeliveries(const ReportingEndpointGroupKey& group_key,
                                   const GURL& url,
                                   int reports_delivered,
                                   bool successful) override;
  void SetExpiredSource(
      const base::UnguessableToken& reporting_source) override;
  const base::flat_set<base::UnguessableToken>& GetExpiredSources()
      const override;
  void RemoveReports(
      const std::vector<raw_ptr<const ReportingReport, VectorExperimental>>&
          reports) override;
  void RemoveReports(
      const std::vector<raw_ptr<const ReportingReport, VectorExperimental>>&
          reports,
      bool delivery_success) override;
  void RemoveAllReports() override;
  size_t GetFullReportCountForTesting() const override;
  size_t GetReportCountWithStatusForTesting(
      ReportingReport::Status status) const override;
  bool IsReportPendingForTesting(const ReportingReport* report) const override;
  bool IsReportDoomedForTesting(const ReportingReport* report) const override;
  void OnParsedHeader(
      const NetworkAnonymizationKey& network_anonymization_key,
      const url::Origin& origin,
      std::vector<ReportingEndpointGroup> parsed_header) override;
  void OnParsedReportingEndpointsHeader(
      const base::UnguessableToken& reporting_source,
      const IsolationInfo& isolation_info,
      std::vector<ReportingEndpoint> parsed_header) override;
  void SetEnterpriseReportingEndpoints(
      const base::flat_map<std::string, GURL>& endpoints) override;
  std::set<url::Origin> GetAllOrigins() const override;
  void RemoveClient(const NetworkAnonymizationKey& network_anonymization_key,
                    const url::Origin& origin) override;
  void RemoveClientsForOrigin(const url::Origin& origin) override;
  void RemoveAllClients() override;
  void RemoveEndpointGroup(const ReportingEndpointGroupKey& group_key) override;
  void RemoveEndpointsForUrl(const GURL& url) override;
  void RemoveSourceAndEndpoints(
      const base::UnguessableToken& reporting_source) override;
  void AddClientsLoadedFromStore(
      std::vector<ReportingEndpoint> loaded_endpoints,
      std::vector<CachedReportingEndpointGroup> loaded_endpoint_groups)
      override;
  std::vector<ReportingEndpoint> GetCandidateEndpointsForDelivery(
      const ReportingEndpointGroupKey& group_key) override;
  base::Value GetClientsAsValue() const override;
  size_t GetEndpointCount() const override;
  void Flush() override;
  ReportingEndpoint GetV1EndpointForTesting(
      const base::UnguessableToken& reporting_source,
      const std::string& endpoint_name) const override;
  ReportingEndpoint GetEndpointForTesting(
      const ReportingEndpointGroupKey& group_key,
      const GURL& url) const override;
  std::vector<ReportingEndpoint> GetEnterpriseEndpointsForTesting()
      const override;
  bool EndpointGroupExistsForTesting(const ReportingEndpointGroupKey& group_key,
                                     OriginSubdomains include_subdomains,
                                     base::Time expires) const override;
  bool ClientExistsForTesting(
      const NetworkAnonymizationKey& network_anonymization_key,
      const url::Origin& origin) const override;
  size_t GetEndpointGroupCountForTesting() const override;
  size_t GetClientCountForTesting() const override;
  size_t GetReportingSourceCountForTesting() const override;
  void SetEndpointForTesting(const ReportingEndpointGroupKey& group_key,
                             const GURL& url,
                             OriginSubdomains include_subdomains,
                             base::Time expires,
                             int priority,
                             int weight) override;
  void SetV1EndpointForTesting(const ReportingEndpointGroupKey& group_key,
                               const base::UnguessableToken& reporting_source,
                               const IsolationInfo& isolation_info,
                               const GURL& url) override;
  void SetEnterpriseEndpointForTesting(
      const ReportingEndpointGroupKey& group_key,
      const GURL& url) override;
  IsolationInfo GetIsolationInfoForEndpoint(
      const ReportingEndpoint& endpoint) const override;

 private:
  // Represents the entire Report-To configuration for a (NAK, origin) pair.
  struct Client {
    Client(const NetworkAnonymizationKey& network_anonymization_key,
           const url::Origin& origin);

    Client(const Client& other);
    Client(Client&& other);

    Client& operator=(const Client& other);
    Client& operator=(Client&& other);

    ~Client();

    // NAK of the context associated with this client. Needed to prevent leaking
    // third party contexts across sites.
    NetworkAnonymizationKey network_anonymization_key;

    // Origin that configured this client.
    url::Origin origin;

    // Total number of endpoints for this origin. Should stay in sync with the
    // sum of endpoint counts for all the groups within this client.
    size_t endpoint_count = 0;

    // Last time that any of the groups for this origin was accessed for a
    // delivery or updated via a new header. Should stay in sync with the latest
    // |last_used| of all the groups within this client.
    base::Time last_used;

    // Set of endpoint group names for this origin.
    std::set<std::string> endpoint_group_names;
  };

  using ReportSet = base::flat_set<std::unique_ptr<ReportingReport>,
                                   base::UniquePtrComparator>;
  using ClientMap = std::multimap<std::string, Client>;
  using EndpointGroupMap =
      std::map<ReportingEndpointGroupKey, CachedReportingEndpointGroup>;
  using EndpointMap =
      std::multimap<ReportingEndpointGroupKey, ReportingEndpoint>;

  ReportSet::const_iterator FindReportToEvict() const;

  // Consistency-checks the entire data structure of clients, groups, and
  // endpoints, if DCHECK is on. The cached clients should pass this consistency
  // check after completely parsing a header (i.e. not after the intermediate
  // steps), and before and after any of the public methods that remove or
  // retrieve client info. Also calls |sequence_checker_| to DCHECK that we are
  // being called on a valid sequence.
  void ConsistencyCheckClients() const;

  // Helper methods for ConsistencyCheckClients():
#if DCHECK_IS_ON()
  // Returns number of endpoint groups found in |client|.
  size_t ConsistencyCheckClient(const std::string& domain,
                                const Client& client) const;

  // Returns the number of endpoints found in |group|.
  size_t ConsistencyCheckEndpointGroup(
      const ReportingEndpointGroupKey& key,
      const CachedReportingEndpointGroup& group) const;

  void ConsistencyCheckEndpoint(const ReportingEndpointGroupKey& key,
                                const ReportingEndpoint& endpoint,
                                EndpointMap::const_iterator endpoint_it) const;
#endif  // DCHECK_IS_ON()

  // Finds iterator to the client with the given |network_anonymization_key| and
  // |origin|, if one exists. Returns |clients_.end()| if none is found.
  ClientMap::iterator FindClientIt(
      const NetworkAnonymizationKey& network_anonymization_key,
      const url::Origin& origin);

  // Overload that takes a ReportingEndpointGroupKey and finds the client
  // to which a group specified by the |group_key| would belong. The group name
  // of the key is ignored.
  ClientMap::iterator FindClientIt(const ReportingEndpointGroupKey& group_key);

  // Finds iterator to the endpoint group identified by |group_key| (origin and
  // name), if one exists. Returns |endpoint_groups_.end()| if none is found.
  EndpointGroupMap::iterator FindEndpointGroupIt(
      const ReportingEndpointGroupKey& group_key);

  // Finds iterator to the endpoint for the given |group_key| (origin and group
  // name) and |url|, if one exists. Returns |endpoints_.end()| if none is
  // found.
  EndpointMap::iterator FindEndpointIt(
      const ReportingEndpointGroupKey& group_key,
      const GURL& url);

  // Adds a new client, endpoint group, or endpoint to the cache, if none
  // exists. If one already exists, updates the existing entry to match the new
  // one. Returns iterator to newly added client.
  ClientMap::iterator AddOrUpdateClient(Client new_client);
  void AddOrUpdateEndpointGroup(CachedReportingEndpointGroup new_group);
  void AddOrUpdateEndpoint(ReportingEndpoint new_endpoint);

  // Remove all the endpoints configured for |origin| and |group| whose urls are
  // not in |endpoints_to_keep_urls|. Does not guarantee that all the endpoints
  // in |endpoints_to_keep_urls| exist in the cache for that group.
  void RemoveEndpointsInGroupOtherThan(
      const ReportingEndpointGroupKey& group_key,
      const std::set<GURL>& endpoints_to_keep_urls);

  // Remove all the endpoint groups for the NAK and origin whose names are not
  // in |groups_to_keep_names|. Does not guarantee that all the groups in
  // |groups_to_keep_names| exist in the cache for that client.
  void RemoveEndpointGroupsForClientOtherThan(
      const NetworkAnonymizationKey& network_anonymization_key,
      const url::Origin& origin,
      const std::set<std::string>& groups_to_keep_names);

  // Gets the endpoints in the given group.
  std::vector<ReportingEndpoint> GetEndpointsInGroup(
      const ReportingEndpointGroupKey& group_key) const;

  // Gets the number of endpoints for the given origin and group.
  size_t GetEndpointCountInGroup(
      const ReportingEndpointGroupKey& group_key) const;

  // Updates the last_used time for the given origin and endpoint group.
  void MarkEndpointGroupAndClientUsed(ClientMap::iterator client_it,
                                      EndpointGroupMap::iterator group_it,
                                      base::Time now);

  // Removes the endpoint at the given iterator, which must exist in the cache.
  // Also takes iterators to the client and endpoint group to avoid repeated
  // lookups. May cause the client and/or group to be removed if they become
  // empty, which would invalidate those iterators.
  // Returns the iterator following the endpoint removed, or std::nullopt if
  // either of |group_it| or |client_it| were invalidated. (If |client_it| is
  // invalidated, then so must |group_it|).
  std::optional<EndpointMap::iterator> RemoveEndpointInternal(
      ClientMap::iterator client_it,
      EndpointGroupMap::iterator group_it,
      EndpointMap::iterator endpoint_it);

  // Removes the endpoint group at the given iterator (which must exist in the
  // cache). Also takes iterator to the client to avoid repeated lookups. May
  // cause the client to be removed if it becomes empty, which would
  // invalidate |client_it|. If |num_endpoints_removed| is not null, then
  // |*num_endpoints_removed| is incremented by the number of endpoints
  // removed.
  // Returns the iterator following the endpoint group removed, or std::nullopt
  // if |client_it| was invalidated.
  std::optional<EndpointGroupMap::iterator> RemoveEndpointGroupInternal(
      ClientMap::iterator client_it,
      EndpointGroupMap::iterator group_it,
      size_t* num_endpoints_removed = nullptr);

  // Removes the client at the given iterator (which must exist in the cache),
  // along with all of its endpoint groups and endpoints. Invalidates
  // |client_it|.
  // Returns the iterator following the client removed.
  ClientMap::iterator RemoveClientInternal(ClientMap::iterator client_it);

  // Evict endpoints from the specified client and globally, if necessary to
  // obey the per-client and global endpoint limits set in the ReportingPolicy.
  //
  // To evict from a client: First evicts any stale or expired groups for that
  // origin. If that removes enough endpoints, then stop. Otherwise, find the
  // stalest group (which has not been accessed for a delivery in the longest
  // time) with the most endpoints, and evict the least important endpoints from
  // that group.
  // To evict globally: Find the stalest client with the most endpoints and do
  // the above.
  void EnforcePerClientAndGlobalEndpointLimits(ClientMap::iterator client_it);

  // Evicts endpoints from a client until it has evicted |endpoints_to_evict|
  // endpoints. First tries to remove expired and stale groups. If that fails to
  // satisfy the limit, finds the stalest group with the most endpoints and
  // evicts the least important endpoints from it.
  void EvictEndpointsFromClient(ClientMap::iterator client_it,
                                size_t endpoints_to_evict);

  // Evicts the least important endpoint from a group (the endpoint with lowest
  // priority and lowest weight). May cause the group and/or client to be
  // deleted and the iterators invalidated.
  void EvictEndpointFromGroup(ClientMap::iterator client_it,
                              EndpointGroupMap::iterator group_it);

  // Removes all expired or stale groups from the given client. May delete the
  // client and invalidate |client_it| if it becomes empty.
  // Increments |*num_endpoints_removed| by the number of endpoints removed.
  // Returns true if |client_it| was invalidated.
  bool RemoveExpiredOrStaleGroups(ClientMap::iterator client_it,
                                  size_t* num_endpoints_removed);

  // Adds/removes (if it exists) |endpoint_it| from |endpoint_its_by_url_|.
  void AddEndpointItToIndex(EndpointMap::iterator endpoint_it);
  void RemoveEndpointItFromIndex(EndpointMap::iterator endpoint_it);

  // Helper method for IncrementEndpointDeliveries
  ReportingEndpoint::Statistics* GetEndpointStats(
      const ReportingEndpointGroupKey& group_key,
      const GURL& url);

  // Helper methods for GetClientsAsValue().
  base::Value GetClientAsValue(const Client& client) const;
  base::Value GetEndpointGroupAsValue(
      const CachedReportingEndpointGroup& group) const;
  base::Value GetEndpointAsValue(const ReportingEndpoint& endpoint) const;

  // Convenience methods for fetching things from the context_.
  const base::Clock& clock() const { return context_->clock(); }
  const base::TickClock& tick_clock() const { return context_->tick_clock(); }
  PersistentReportingStore* store() { return context_->store(); }

  raw_ptr<ReportingContext> context_;

  // Reports that have not yet been successfully uploaded.
  ReportSet reports_;

  // Reporting API V0 Cache:
  // The |clients_|, |endpoint_groups_| and |endpoints_| members all hold
  // endpoint group configuration for the V0 API. These endpoint groups are
  // configured through the Report-To HTTP header, and are currently used for
  // both document and network reports.

  // Map of clients for all configured origins and NAKs, keyed on domain name
  // (there may be multiple NAKs and origins per domain name).
  ClientMap clients_;

  // Map of endpoint groups, keyed on origin and group name. Keys and values
  // must only contain V0 endpoint group keys.
  EndpointGroupMap endpoint_groups_;

  // Map of endpoints, keyed on origin and group name (there may be multiple
  // endpoints for a given origin and group, with different urls). Keys must
  // only contain V0 endpoint group keys.
  EndpointMap endpoints_;

  // Index of endpoints stored in |endpoints_| keyed on URL, for easier lookup
  // during RemoveEndpointsForUrl(). Should stay in sync with |endpoints_|.
  std::multimap<GURL, EndpointMap::iterator> endpoint_its_by_url_;

  // Reporting API V1 Cache:
  // The `document_endpoints_` member holds endpoint configuration for the V1
  // API, configured through the Reporting-Endpoints HTTP header. These
  // endpoints are strongly associated with the resource which configured them,
  // and are only used for document reports.

  // Map of endpoints for each reporting source, keyed on the reporting source
  // token. This contains only V1 document endpoints.
  std::map<base::UnguessableToken, std::vector<ReportingEndpoint>>
      document_endpoints_;

  // Endpoints set by the enterprise policy.
  std::vector<ReportingEndpoint> enterprise_endpoints_;

  // Isolation info for each reporting source. Used for determining credentials
  // to send when delivering reports. This contains only V1 document endpoints.
  std::map<base::UnguessableToken, IsolationInfo> isolation_info_;

  // Reporting source tokens representing sources which have been destroyed.
  // The configuration in `document_endpoints_` and `isolation_info_` for these
  // sources can be removed once all outstanding reports are delivered (or
  // expired).
  base::flat_set<base::UnguessableToken> expired_sources_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace net

#endif  // NET_REPORTING_REPORTING_CACHE_IMPL_H_
