// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_REPORTING_REPORTING_CACHE_H_
#define NET_REPORTING_REPORTING_CACHE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/stl_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "net/base/net_export.h"
#include "net/reporting/reporting_endpoint.h"
#include "net/reporting/reporting_header_parser.h"
#include "net/reporting/reporting_report.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net {

class NetworkIsolationKey;
class ReportingContext;

// The cache holds undelivered reports and clients (per-origin endpoint
// configurations) in memory. (It is not responsible for persisting them.)
//
// Each Reporting "endpoint" represents a report collector at some specified
// URL. Endpoints are organized into named "endpoint groups", each of which
// additionally specifes some properties such as expiration time.
// A "client" represents the entire endpoint configuration set by an origin via
// a Report-To header, which consists of multiple endpoint groups, each of which
// consists of multiple endpoints. An endpoint group is keyed by its name.  An
// endpoint is unkeyed except by the client and group structure tree above it.
//
// The cache implementation corresponds roughly to the "Reporting cache"
// described in the spec, except that endpoints and clients are stored in a more
// structurally-convenient way, and endpoint failures/retry-after are tracked in
// ReportingEndpointManager.
//
// The cache implementation has the notion of "pending" reports. These are
// reports that are part of an active delivery attempt, so they won't be
// actually deallocated. Any attempt to remove a pending report wil mark it
// "doomed", which will cause it to be deallocated once it is no longer pending.
class NET_EXPORT ReportingCache {
 public:
  class PersistentReportingStore;

  static std::unique_ptr<ReportingCache> Create(ReportingContext* context);

  virtual ~ReportingCache();

  // Adds a report to the cache.
  //
  // All parameters correspond to the desired values for the relevant fields in
  // ReportingReport.
  virtual void AddReport(const GURL& url,
                         const std::string& user_agent,
                         const std::string& group_name,
                         const std::string& type,
                         std::unique_ptr<const base::Value> body,
                         int depth,
                         base::TimeTicks queued,
                         int attempts) = 0;

  // Gets all reports in the cache. The returned pointers are valid as long as
  // either no calls to |RemoveReports| have happened or the reports' |pending|
  // flag has been set to true using |SetReportsPending|. Does not return
  // doomed reports (pending reports for which removal has been requested).
  //
  // (Clears any existing data in |*reports_out|.)
  virtual void GetReports(
      std::vector<const ReportingReport*>* reports_out) const = 0;

  // Gets all reports in the cache, including pending and doomed reports, as a
  // base::Value.
  virtual base::Value GetReportsAsValue() const = 0;

  // Gets all reports in the cache that aren't pending. The returned pointers
  // are valid as long as either no calls to |RemoveReports| have happened or
  // the reports' |pending| flag has been set to true using |SetReportsPending|.
  //
  // (Clears any existing data in |*reports_out|.)
  virtual void GetNonpendingReports(
      std::vector<const ReportingReport*>* reports_out) const = 0;

  // Marks a set of reports as pending. |reports| must not already be marked as
  // pending.
  virtual void SetReportsPending(
      const std::vector<const ReportingReport*>& reports) = 0;

  // Unmarks a set of reports as pending. |reports| must be previously marked as
  // pending.
  virtual void ClearReportsPending(
      const std::vector<const ReportingReport*>& reports) = 0;

  // Increments |attempts| on a set of reports.
  virtual void IncrementReportsAttempts(
      const std::vector<const ReportingReport*>& reports) = 0;

  // Records that we attempted (and possibly succeeded at) delivering
  // |reports_delivered| reports to the specified endpoint.
  virtual void IncrementEndpointDeliveries(const url::Origin& origin,
                                           const std::string& group_name,
                                           const GURL& url,
                                           int reports_delivered,
                                           bool successful) = 0;

  // Removes a set of reports. Any reports that are pending will not be removed
  // immediately, but rather marked doomed and removed once they are no longer
  // pending.
  virtual void RemoveReports(const std::vector<const ReportingReport*>& reports,
                             ReportingReport::Outcome outcome) = 0;

  // Removes all reports. Like |RemoveReports()|, pending reports are doomed
  // until no longer pending.
  virtual void RemoveAllReports(ReportingReport::Outcome outcome) = 0;

  // Gets the count of reports in the cache, *including* doomed reports.
  //
  // Needed to ensure that doomed reports are eventually deleted, since no
  // method provides a view of *every* report in the cache, just non-doomed
  // ones.
  virtual size_t GetFullReportCountForTesting() const = 0;

  virtual bool IsReportPendingForTesting(
      const ReportingReport* report) const = 0;

  virtual bool IsReportDoomedForTesting(
      const ReportingReport* report) const = 0;

  // Adds a new client to the cache for |origin|, or updates the existing one
  // to match the new header. All values are assumed to be valid as they have
  // passed through the ReportingHeaderParser.
  virtual void OnParsedHeader(
      const url::Origin& origin,
      std::vector<ReportingEndpointGroup> parsed_header) = 0;

  // Gets all the origins of clients in the cache.
  virtual std::vector<url::Origin> GetAllOrigins() const = 0;

  // Remove client for the given |origin|, if it exists in the cache.
  // All endpoint groups and endpoints for |origin| are also removed.
  virtual void RemoveClient(const url::Origin& origin) = 0;

  // Remove all clients, groups, and endpoints from the cache.
  virtual void RemoveAllClients() = 0;

  // Remove the endpoint group named |name| for the given |origin|, and remove
  // all endpoints for that group. May cause the client for |origin| to be
  // deleted if it becomes empty.
  virtual void RemoveEndpointGroup(const url::Origin& origin,
                                   const std::string& group_name) = 0;

  // Remove all endpoints for with |url|, regardless of origin or group. Used
  // when a delivery returns 410 Gone. May cause deletion of groups/clients if
  // they become empty.
  virtual void RemoveEndpointsForUrl(const GURL& url) = 0;

  // Insert endpoints and endpoint groups that have been loaded from the store.
  //
  // You must only call this method if context.store() was non-null when you
  // constructed the cache and persist_clients_across_restarts in your
  // ReportingPolicy is true.
  virtual void AddClientsLoadedFromStore(
      std::vector<ReportingEndpoint> loaded_endpoints,
      std::vector<CachedReportingEndpointGroup> loaded_endpoint_groups) = 0;

  // Gets endpoints that apply to a delivery for |origin| and |group|.
  //
  // First checks for |group| in a client exactly matching |origin|.
  // If none exists, then checks for |group| in clients for superdomains
  // of |origin| which have include_subdomains enabled, returning only the
  // endpoints for the most specific applicable parent origin of |origin|. If
  // there are multiple origins with that group within the most specific
  // applicable superdomain, gets endpoints for that group from only one of
  // them. The group must not be expired.
  //
  // For example, given the origin https://foo.bar.baz.com/, the cache
  // would prioritize returning each potential match below over the ones below
  // it, for groups with name |group| with include_subdomains enabled:
  // 1. https://foo.bar.baz.com/ (exact origin match)
  // 2. https://foo.bar.baz.com:444/ (technically, a superdomain)
  // 3. https://bar.baz.com/, https://bar.baz.com:444/, etc. (superdomain)
  // 4. https://baz.com/, https://baz.com:444/, etc. (superdomain)
  // If both https://bar.baz.com/ and https://bar.baz.com:444/ had a group with
  // name |group| with include_subdomains enabled, this method would return
  // endpoints from that group from the earliest-inserted origin.
  virtual std::vector<ReportingEndpoint> GetCandidateEndpointsForDelivery(
      const NetworkIsolationKey& network_isolation_key,
      const url::Origin& origin,
      const std::string& group_name) = 0;

  // Gets the status of all clients in the cache, including expired ones, as a
  // base::Value.
  virtual base::Value GetClientsAsValue() const = 0;

  // Gets the total number of endpoints in the cache across all origins.
  virtual size_t GetEndpointCount() const = 0;

  // Flush the contents of the cache to disk, if applicable.
  virtual void Flush() = 0;

  // Finds an endpoint for the given |origin|, |group_name|, and |url|,
  // otherwise returns an invalid ReportingEndpoint.
  virtual ReportingEndpoint GetEndpointForTesting(const url::Origin& origin,
                                                  const std::string& group_name,
                                                  const GURL& url) const = 0;

  // Returns whether an endpoint group with exactly the given properties exists
  // in the cache. If |expires| is base::Time(), it will not be checked.
  virtual bool EndpointGroupExistsForTesting(
      const url::Origin& origin,
      const std::string& group_name,
      OriginSubdomains include_subdomains,
      base::Time expires) const = 0;

  // Returns number of endpoint groups.
  virtual size_t GetEndpointGroupCountForTesting() const = 0;

  // Sets an endpoint with the given properties in a group with the given
  // properties, bypassing header parsing. Note that the endpoint is not
  // guaranteed to exist in the cache after calling this function, if endpoint
  // eviction is triggered. Unlike the AddOrUpdate*() methods used in header
  // parsing, this method inserts or updates a single endpoint while leaving the
  // exiting configuration for that origin intact.
  virtual void SetEndpointForTesting(const url::Origin& origin,
                                     const std::string& group_name,
                                     const GURL& url,
                                     OriginSubdomains include_subdomains,
                                     base::Time expires,
                                     int priority,
                                     int weight) = 0;
};

// Persistent storage for Reporting reports and clients.
class NET_EXPORT ReportingCache::PersistentReportingStore {
 public:
  using ReportingClientsLoadedCallback =
      base::OnceCallback<void(std::vector<ReportingEndpoint>,
                              std::vector<CachedReportingEndpointGroup>)>;

  PersistentReportingStore() = default;
  virtual ~PersistentReportingStore() = default;

  // Initializes the store and retrieves stored endpoints and endpoint groups.
  // Called only once at startup.
  virtual void LoadReportingClients(
      ReportingClientsLoadedCallback loaded_callback) = 0;

  // Adds an endpoint to the store.
  virtual void AddReportingEndpoint(const ReportingEndpoint& endpoint) = 0;
  // Adds an endpoint group to the store.
  virtual void AddReportingEndpointGroup(
      const CachedReportingEndpointGroup& group) = 0;

  // Updates the access time of an endpoint group in the store.
  virtual void UpdateReportingEndpointGroupAccessTime(
      const CachedReportingEndpointGroup& group) = 0;

  // Updates the details of an endpoint in the store.
  virtual void UpdateReportingEndpointDetails(
      const ReportingEndpoint& endpoint) = 0;
  // Updates the details of an endpoint group in the store.
  virtual void UpdateReportingEndpointGroupDetails(
      const CachedReportingEndpointGroup& group) = 0;

  // Deletes an endpoint from the store.
  virtual void DeleteReportingEndpoint(const ReportingEndpoint& endpoint) = 0;
  // Deletes an endpoint group from the store.
  virtual void DeleteReportingEndpointGroup(
      const CachedReportingEndpointGroup& group) = 0;

  // TODO(chlily): methods to load, add, and delete reports will be added.

  // Flushes the store.
  virtual void Flush() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(PersistentReportingStore);
};

}  // namespace net

#endif  // NET_REPORTING_REPORTING_CACHE_H_
