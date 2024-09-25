// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/reporting/reporting_cache_impl.h"

#include <algorithm>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "base/not_fatal_until.h"
#include "base/ranges/algorithm.h"
#include "base/stl_util.h"
#include "base/time/clock.h"
#include "base/time/tick_clock.h"
#include "net/base/features.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/url_util.h"
#include "net/log/net_log.h"
#include "net/reporting/reporting_target_type.h"

namespace net {

ReportingCacheImpl::ReportingCacheImpl(
    ReportingContext* context,
    const base::flat_map<std::string, GURL>& enterprise_reporting_endpoints)
    : context_(context) {
  DCHECK(context_);
  SetEnterpriseReportingEndpoints(enterprise_reporting_endpoints);
}

ReportingCacheImpl::~ReportingCacheImpl() = default;

void ReportingCacheImpl::AddReport(
    const std::optional<base::UnguessableToken>& reporting_source,
    const NetworkAnonymizationKey& network_anonymization_key,
    const GURL& url,
    const std::string& user_agent,
    const std::string& group_name,
    const std::string& type,
    base::Value::Dict body,
    int depth,
    base::TimeTicks queued,
    int attempts,
    ReportingTargetType target_type) {
  // If |reporting_source| is present, it must not be empty.
  DCHECK(!(reporting_source.has_value() && reporting_source->is_empty()));
  // Drop the report if its reporting source is already marked as expired.
  // This should only happen in testing as reporting source is only marked
  // expiring when the document that can generate report is gone.
  if (reporting_source.has_value() &&
      expired_sources_.find(reporting_source.value()) !=
          expired_sources_.end()) {
    return;
  }

  auto report = std::make_unique<ReportingReport>(
      reporting_source, network_anonymization_key, url, user_agent, group_name,
      type, std::move(body), depth, queued, attempts, target_type);

  auto inserted = reports_.insert(std::move(report));
  DCHECK(inserted.second);

  if (reports_.size() > context_->policy().max_report_count) {
    // There should be at most one extra report (the one added above).
    DCHECK_EQ(context_->policy().max_report_count + 1, reports_.size());
    ReportSet::const_iterator to_evict = FindReportToEvict();
    CHECK(to_evict != reports_.end(), base::NotFatalUntil::M130);
    // The newly-added report isn't pending, so even if all other reports are
    // pending, the cache should have a report to evict.
    DCHECK(!to_evict->get()->IsUploadPending());
    if (to_evict != inserted.first) {
      context_->NotifyReportAdded(inserted.first->get());
    }
    reports_.erase(to_evict);
  } else {
    context_->NotifyReportAdded(inserted.first->get());
  }

  context_->NotifyCachedReportsUpdated();
}

void ReportingCacheImpl::GetReports(
    std::vector<raw_ptr<const ReportingReport, VectorExperimental>>*
        reports_out) const {
  reports_out->clear();
  for (const auto& report : reports_) {
    if (report->status != ReportingReport::Status::DOOMED &&
        report->status != ReportingReport::Status::SUCCESS) {
      reports_out->push_back(report.get());
    }
  }
}

base::Value ReportingCacheImpl::GetReportsAsValue() const {
  // Sort all unsent reports by origin and timestamp.
  std::vector<const ReportingReport*> sorted_reports;
  sorted_reports.reserve(reports_.size());
  for (const auto& report : reports_) {
    sorted_reports.push_back(report.get());
  }
  std::sort(sorted_reports.begin(), sorted_reports.end(),
            [](const ReportingReport* report1, const ReportingReport* report2) {
              return std::tie(report1->queued, report1->url) <
                     std::tie(report2->queued, report2->url);
            });

  base::Value::List report_list;
  for (const ReportingReport* report : sorted_reports) {
    base::Value::Dict report_dict;
    report_dict.Set("network_anonymization_key",
                    report->network_anonymization_key.ToDebugString());
    report_dict.Set("url", report->url.spec());
    report_dict.Set("group", report->group);
    report_dict.Set("type", report->type);
    report_dict.Set("depth", report->depth);
    report_dict.Set("queued", NetLog::TickCountToString(report->queued));
    report_dict.Set("attempts", report->attempts);
    report_dict.Set("body", report->body.Clone());
    switch (report->status) {
      case ReportingReport::Status::DOOMED:
        report_dict.Set("status", "doomed");
        break;
      case ReportingReport::Status::PENDING:
        report_dict.Set("status", "pending");
        break;
      case ReportingReport::Status::QUEUED:
        report_dict.Set("status", "queued");
        break;
      case ReportingReport::Status::SUCCESS:
        report_dict.Set("status", "success");
        break;
    }
    report_list.Append(std::move(report_dict));
  }
  return base::Value(std::move(report_list));
}

std::vector<raw_ptr<const ReportingReport, VectorExperimental>>
ReportingCacheImpl::GetReportsToDeliver() {
  std::vector<raw_ptr<const ReportingReport, VectorExperimental>> reports_out;
  for (const auto& report : reports_) {
    if (report->IsUploadPending())
      continue;
    report->status = ReportingReport::Status::PENDING;
    context_->NotifyReportUpdated(report.get());
    reports_out.push_back(report.get());
  }
  return reports_out;
}

std::vector<raw_ptr<const ReportingReport, VectorExperimental>>
ReportingCacheImpl::GetReportsToDeliverForSource(
    const base::UnguessableToken& reporting_source) {
  DCHECK(!reporting_source.is_empty());
  std::vector<raw_ptr<const ReportingReport, VectorExperimental>> reports_out;
  for (const auto& report : reports_) {
    if (report->reporting_source == reporting_source) {
      if (report->IsUploadPending())
        continue;
      report->status = ReportingReport::Status::PENDING;
      context_->NotifyReportUpdated(report.get());
      reports_out.push_back(report.get());
    }
  }
  return reports_out;
}

void ReportingCacheImpl::ClearReportsPending(
    const std::vector<raw_ptr<const ReportingReport, VectorExperimental>>&
        reports) {
  for (const ReportingReport* report : reports) {
    auto it = reports_.find(report);
    CHECK(it != reports_.end(), base::NotFatalUntil::M130);
    if (it->get()->status == ReportingReport::Status::DOOMED ||
        it->get()->status == ReportingReport::Status::SUCCESS) {
      reports_.erase(it);
    } else {
      DCHECK_EQ(ReportingReport::Status::PENDING, it->get()->status);
      it->get()->status = ReportingReport::Status::QUEUED;
      context_->NotifyReportUpdated(it->get());
    }
  }
}

void ReportingCacheImpl::IncrementReportsAttempts(
    const std::vector<raw_ptr<const ReportingReport, VectorExperimental>>&
        reports) {
  for (const ReportingReport* report : reports) {
    auto it = reports_.find(report);
    CHECK(it != reports_.end(), base::NotFatalUntil::M130);
    it->get()->attempts++;
    context_->NotifyReportUpdated(it->get());
  }

  context_->NotifyCachedReportsUpdated();
}

std::vector<ReportingEndpoint> FilterEndpointsByOrigin(
    const std::map<base::UnguessableToken, std::vector<ReportingEndpoint>>&
        document_endpoints,
    const url::Origin& origin) {
  std::set<std::string> group_names;
  std::vector<ReportingEndpoint> result;
  for (const auto& token_and_endpoints : document_endpoints) {
    for (const auto& endpoint : token_and_endpoints.second) {
      if (endpoint.group_key.origin == origin) {
        if (group_names.insert(endpoint.group_key.group_name).second) {
          // Push the endpoint only when the insertion succeeds.
          result.push_back(endpoint);
        }
      }
    }
  }
  return result;
}

base::flat_map<url::Origin, std::vector<ReportingEndpoint>>
ReportingCacheImpl::GetV1ReportingEndpointsByOrigin() const {
  base::flat_map<url::Origin, std::vector<ReportingEndpoint>> result;
  base::flat_map<url::Origin, base::flat_set<std::string>> group_name_helper;
  for (const auto& token_and_endpoints : document_endpoints_) {
    for (const auto& endpoint : token_and_endpoints.second) {
      // Document endpoints should have an origin.
      DCHECK(endpoint.group_key.origin.has_value());
      auto origin = endpoint.group_key.origin.value();
      if (result.count(origin)) {
        if (group_name_helper.at(origin)
                .insert(endpoint.group_key.group_name)
                .second) {
          // Push the endpoint only when the insertion succeeds.
          result.at(origin).push_back(endpoint);
        }
      } else {
        std::vector<ReportingEndpoint> endpoints_for_origin;
        endpoints_for_origin.push_back(endpoint);
        result.emplace(origin, endpoints_for_origin);

        base::flat_set<std::string> group_names;
        group_names.insert(endpoint.group_key.group_name);
        group_name_helper.emplace(origin, group_names);
      }
    }
  }
  return result;
}

ReportingEndpoint::Statistics* ReportingCacheImpl::GetEndpointStats(
    const ReportingEndpointGroupKey& group_key,
    const GURL& url) {
  if (group_key.IsDocumentEndpoint()) {
    const auto document_endpoints_source_it =
        document_endpoints_.find(group_key.reporting_source.value());
    // The reporting source may have been removed while the upload was in
    // progress. In that case, we no longer care about the stats for the
    // endpoint associated with the destroyed reporting source.
    if (document_endpoints_source_it == document_endpoints_.end())
      return nullptr;
    const auto document_endpoint_it =
        base::ranges::find(document_endpoints_source_it->second, group_key,
                           &ReportingEndpoint::group_key);
    // The endpoint may have been removed while the upload was in progress. In
    // that case, we no longer care about the stats for the removed endpoint.
    if (document_endpoint_it == document_endpoints_source_it->second.end())
      return nullptr;
    return &document_endpoint_it->stats;
  } else {
    EndpointMap::iterator endpoint_it = FindEndpointIt(group_key, url);
    // The endpoint may have been removed while the upload was in progress. In
    // that case, we no longer care about the stats for the removed endpoint.
    if (endpoint_it == endpoints_.end())
      return nullptr;
    return &endpoint_it->second.stats;
  }
}

void ReportingCacheImpl::IncrementEndpointDeliveries(
    const ReportingEndpointGroupKey& group_key,
    const GURL& url,
    int reports_delivered,
    bool successful) {
  ReportingEndpoint::Statistics* stats = GetEndpointStats(group_key, url);
  if (!stats)
    return;

  ++stats->attempted_uploads;
  stats->attempted_reports += reports_delivered;
  if (successful) {
    ++stats->successful_uploads;
    stats->successful_reports += reports_delivered;
  }
}

void ReportingCacheImpl::SetExpiredSource(
    const base::UnguessableToken& reporting_source) {
  DCHECK(!reporting_source.is_empty());
  expired_sources_.insert(reporting_source);
}

const base::flat_set<base::UnguessableToken>&
ReportingCacheImpl::GetExpiredSources() const {
  return expired_sources_;
}

void ReportingCacheImpl::RemoveReports(
    const std::vector<raw_ptr<const ReportingReport, VectorExperimental>>&
        reports) {
  RemoveReports(reports, false);
}

void ReportingCacheImpl::RemoveReports(
    const std::vector<raw_ptr<const ReportingReport, VectorExperimental>>&
        reports,
    bool delivery_success) {
  for (const ReportingReport* report : reports) {
    auto it = reports_.find(report);
    CHECK(it != reports_.end(), base::NotFatalUntil::M130);

    switch (it->get()->status) {
      case ReportingReport::Status::DOOMED:
        if (delivery_success) {
          it->get()->status = ReportingReport::Status::SUCCESS;
          context_->NotifyReportUpdated(it->get());
        }
        break;
      case ReportingReport::Status::PENDING:
        it->get()->status = delivery_success ? ReportingReport::Status::SUCCESS
                                             : ReportingReport::Status::DOOMED;
        context_->NotifyReportUpdated(it->get());
        break;
      case ReportingReport::Status::QUEUED:
        it->get()->status = delivery_success ? ReportingReport::Status::SUCCESS
                                             : ReportingReport::Status::DOOMED;
        context_->NotifyReportUpdated(it->get());
        reports_.erase(it);
        break;
      case ReportingReport::Status::SUCCESS:
        break;
    }
  }
  context_->NotifyCachedReportsUpdated();
}

void ReportingCacheImpl::RemoveAllReports() {
  std::vector<raw_ptr<const ReportingReport, VectorExperimental>>
      reports_to_remove;
  GetReports(&reports_to_remove);
  RemoveReports(reports_to_remove);
}

size_t ReportingCacheImpl::GetFullReportCountForTesting() const {
  return reports_.size();
}

size_t ReportingCacheImpl::GetReportCountWithStatusForTesting(
    ReportingReport::Status status) const {
  size_t count = 0;
  for (const auto& report : reports_) {
    if (report->status == status)
      ++count;
  }
  return count;
}

bool ReportingCacheImpl::IsReportPendingForTesting(
    const ReportingReport* report) const {
  DCHECK(report);
  DCHECK(reports_.find(report) != reports_.end());
  return report->IsUploadPending();
}

bool ReportingCacheImpl::IsReportDoomedForTesting(
    const ReportingReport* report) const {
  DCHECK(report);
  DCHECK(reports_.find(report) != reports_.end());
  return report->status == ReportingReport::Status::DOOMED ||
         report->status == ReportingReport::Status::SUCCESS;
}

void ReportingCacheImpl::OnParsedHeader(
    const NetworkAnonymizationKey& network_anonymization_key,
    const url::Origin& origin,
    std::vector<ReportingEndpointGroup> parsed_header) {
  ConsistencyCheckClients();

  Client new_client(network_anonymization_key, origin);
  base::Time now = clock().Now();
  new_client.last_used = now;

  std::map<ReportingEndpointGroupKey, std::set<GURL>> endpoints_per_group;

  for (const auto& parsed_endpoint_group : parsed_header) {
    new_client.endpoint_group_names.insert(
        parsed_endpoint_group.group_key.group_name);

    // Creates an endpoint group and sets its |last_used| to |now|.
    CachedReportingEndpointGroup new_group(parsed_endpoint_group, now);

    // Consistency check: the new client should have the same NAK and origin as
    // all groups parsed from this header.
    DCHECK(new_group.group_key.network_anonymization_key ==
           new_client.network_anonymization_key);
    // V0 endpoints should have an origin.
    DCHECK(new_group.group_key.origin.has_value());
    DCHECK_EQ(new_group.group_key.origin.value(), new_client.origin);

    for (const auto& parsed_endpoint_info : parsed_endpoint_group.endpoints) {
      endpoints_per_group[new_group.group_key].insert(parsed_endpoint_info.url);
      ReportingEndpoint new_endpoint(new_group.group_key,
                                     std::move(parsed_endpoint_info));
      AddOrUpdateEndpoint(std::move(new_endpoint));
    }

    AddOrUpdateEndpointGroup(std::move(new_group));
  }

  // Compute the total endpoint count for this origin. We can't just count the
  // number of endpoints per group because there may be duplicate endpoint URLs,
  // which we ignore. See http://crbug.com/983000 for discussion.
  // TODO(crbug.com/40635629): Allow duplicate endpoint URLs.
  for (const auto& group_key_and_endpoint_set : endpoints_per_group) {
    new_client.endpoint_count += group_key_and_endpoint_set.second.size();

    // Remove endpoints that may have been previously configured for this group,
    // but which were not specified in the current header.
    // This must be done all at once after all the groups in the header have
    // been processed, rather than after each individual group, otherwise
    // headers with multiple groups of the same name will clobber previous parts
    // of themselves. See crbug.com/1116529.
    RemoveEndpointsInGroupOtherThan(group_key_and_endpoint_set.first,
                                    group_key_and_endpoint_set.second);
  }

  // Remove endpoint groups that may have been configured for an existing client
  // for |origin|, but which are not specified in the current header.
  RemoveEndpointGroupsForClientOtherThan(network_anonymization_key, origin,
                                         new_client.endpoint_group_names);

  EnforcePerClientAndGlobalEndpointLimits(
      AddOrUpdateClient(std::move(new_client)));
  ConsistencyCheckClients();

  context_->NotifyCachedClientsUpdated();
}

void ReportingCacheImpl::RemoveSourceAndEndpoints(
    const base::UnguessableToken& reporting_source) {
  DCHECK(!reporting_source.is_empty());
  // Sanity checks: The source must be in the list of expired sources, and
  // there must be no more cached reports for it (except reports already marked
  // as doomed, as they will be garbage collected soon).
  DCHECK(expired_sources_.contains(reporting_source));
  DCHECK(
      base::ranges::none_of(reports_, [reporting_source](const auto& report) {
        return report->reporting_source == reporting_source &&
               report->status != ReportingReport::Status::DOOMED &&
               report->status != ReportingReport::Status::SUCCESS;
      }));
  url::Origin origin;
  if (document_endpoints_.count(reporting_source) > 0) {
    // Document endpoints should have an origin.
    DCHECK(document_endpoints_.at(reporting_source)[0]
               .group_key.origin.has_value());
    origin =
        document_endpoints_.at(reporting_source)[0].group_key.origin.value();
  }
  document_endpoints_.erase(reporting_source);
  isolation_info_.erase(reporting_source);
  expired_sources_.erase(reporting_source);
  context_->NotifyEndpointsUpdatedForOrigin(
      FilterEndpointsByOrigin(document_endpoints_, origin));
}

void ReportingCacheImpl::OnParsedReportingEndpointsHeader(
    const base::UnguessableToken& reporting_source,
    const IsolationInfo& isolation_info,
    std::vector<ReportingEndpoint> endpoints) {
  DCHECK(!reporting_source.is_empty());
  DCHECK(!endpoints.empty());
  DCHECK_EQ(0u, document_endpoints_.count(reporting_source));
  DCHECK_EQ(0u, isolation_info_.count(reporting_source));
  // Document endpoints should have an origin.
  DCHECK(endpoints[0].group_key.origin.has_value());
  url::Origin origin = endpoints[0].group_key.origin.value();
  document_endpoints_.insert({reporting_source, std::move(endpoints)});
  isolation_info_.insert({reporting_source, isolation_info});
  context_->NotifyEndpointsUpdatedForOrigin(
      FilterEndpointsByOrigin(document_endpoints_, origin));
}

void ReportingCacheImpl::SetEnterpriseReportingEndpoints(
    const base::flat_map<std::string, GURL>& endpoints) {
  if (!base::FeatureList::IsEnabled(
          net::features::kReportingApiEnableEnterpriseCookieIssues)) {
    return;
  }
  std::vector<ReportingEndpoint> new_enterprise_endpoints;
  new_enterprise_endpoints.reserve(endpoints.size());
  for (const auto& [endpoint_name, endpoint_url] : endpoints) {
    ReportingEndpoint endpoint;
    endpoint.group_key = ReportingEndpointGroupKey(
        NetworkAnonymizationKey(), /*reporting_source=*/std::nullopt,
        /*origin=*/std::nullopt, endpoint_name,
        ReportingTargetType::kEnterprise);
    ReportingEndpoint::EndpointInfo endpoint_info;
    endpoint_info.url = endpoint_url;
    endpoint.info = endpoint_info;
    new_enterprise_endpoints.push_back(endpoint);
  }
  enterprise_endpoints_.swap(new_enterprise_endpoints);
}

std::set<url::Origin> ReportingCacheImpl::GetAllOrigins() const {
  ConsistencyCheckClients();
  std::set<url::Origin> origins_out;
  for (const auto& domain_and_client : clients_) {
    origins_out.insert(domain_and_client.second.origin);
  }
  return origins_out;
}

void ReportingCacheImpl::RemoveClient(
    const NetworkAnonymizationKey& network_anonymization_key,
    const url::Origin& origin) {
  ConsistencyCheckClients();
  ClientMap::iterator client_it =
      FindClientIt(network_anonymization_key, origin);
  if (client_it == clients_.end())
    return;
  RemoveClientInternal(client_it);
  ConsistencyCheckClients();
  context_->NotifyCachedClientsUpdated();
}

void ReportingCacheImpl::RemoveClientsForOrigin(const url::Origin& origin) {
  ConsistencyCheckClients();
  std::string domain = origin.host();
  const auto domain_range = clients_.equal_range(domain);
  ClientMap::iterator it = domain_range.first;
  while (it != domain_range.second) {
    if (it->second.origin == origin) {
      it = RemoveClientInternal(it);
      continue;
    }
    ++it;
  }
  ConsistencyCheckClients();
  context_->NotifyCachedClientsUpdated();
}

void ReportingCacheImpl::RemoveAllClients() {
  ConsistencyCheckClients();

  auto remove_it = clients_.begin();
  while (remove_it != clients_.end()) {
    remove_it = RemoveClientInternal(remove_it);
  }

  DCHECK(clients_.empty());
  DCHECK(endpoint_groups_.empty());
  DCHECK(endpoints_.empty());
  DCHECK(endpoint_its_by_url_.empty());

  ConsistencyCheckClients();
  context_->NotifyCachedClientsUpdated();
}

void ReportingCacheImpl::RemoveEndpointGroup(
    const ReportingEndpointGroupKey& group_key) {
  ConsistencyCheckClients();
  EndpointGroupMap::iterator group_it = FindEndpointGroupIt(group_key);
  if (group_it == endpoint_groups_.end())
    return;
  ClientMap::iterator client_it = FindClientIt(group_key);
  CHECK(client_it != clients_.end(), base::NotFatalUntil::M130);

  RemoveEndpointGroupInternal(client_it, group_it);
  ConsistencyCheckClients();
  context_->NotifyCachedClientsUpdated();
}

void ReportingCacheImpl::RemoveEndpointsForUrl(const GURL& url) {
  ConsistencyCheckClients();

  auto url_range = endpoint_its_by_url_.equal_range(url);
  if (url_range.first == url_range.second)
    return;

  // Make a copy of the EndpointMap::iterators matching |url|, to avoid deleting
  // while iterating
  std::vector<EndpointMap::iterator> endpoint_its_to_remove;
  for (auto index_it = url_range.first; index_it != url_range.second;
       ++index_it) {
    endpoint_its_to_remove.push_back(index_it->second);
  }
  DCHECK_GT(endpoint_its_to_remove.size(), 0u);

  // Delete from the index, since we have the |url_range| already. This saves
  // us from having to remove them one by one, which would involve
  // iterating over the |url_range| on each call to RemoveEndpointInternal().
  endpoint_its_by_url_.erase(url_range.first, url_range.second);

  for (EndpointMap::iterator endpoint_it : endpoint_its_to_remove) {
    DCHECK(endpoint_it->second.info.url == url);
    const ReportingEndpointGroupKey& group_key = endpoint_it->first;
    ClientMap::iterator client_it = FindClientIt(group_key);
    CHECK(client_it != clients_.end(), base::NotFatalUntil::M130);
    EndpointGroupMap::iterator group_it = FindEndpointGroupIt(group_key);
    CHECK(group_it != endpoint_groups_.end(), base::NotFatalUntil::M130);
    RemoveEndpointInternal(client_it, group_it, endpoint_it);
  }

  ConsistencyCheckClients();
  context_->NotifyCachedClientsUpdated();
}

// Reconstruct an Client from the loaded endpoint groups, and add the
// loaded endpoints and endpoint groups into the cache.
void ReportingCacheImpl::AddClientsLoadedFromStore(
    std::vector<ReportingEndpoint> loaded_endpoints,
    std::vector<CachedReportingEndpointGroup> loaded_endpoint_groups) {
  DCHECK(context_->IsClientDataPersisted());

  std::sort(loaded_endpoints.begin(), loaded_endpoints.end(),
            [](const ReportingEndpoint& a, const ReportingEndpoint& b) -> bool {
              return a.group_key < b.group_key;
            });
  std::sort(loaded_endpoint_groups.begin(), loaded_endpoint_groups.end(),
            [](const CachedReportingEndpointGroup& a,
               const CachedReportingEndpointGroup& b) -> bool {
              return a.group_key < b.group_key;
            });

  // If using a persistent store, cache should be empty before loading finishes.
  DCHECK(clients_.empty());
  DCHECK(endpoint_groups_.empty());
  DCHECK(endpoints_.empty());
  DCHECK(endpoint_its_by_url_.empty());

  // |loaded_endpoints| and |loaded_endpoint_groups| should both be sorted by
  // origin and group name.
  auto endpoints_it = loaded_endpoints.begin();
  auto endpoint_groups_it = loaded_endpoint_groups.begin();

  std::optional<Client> client;

  while (endpoint_groups_it != loaded_endpoint_groups.end() &&
         endpoints_it != loaded_endpoints.end()) {
    const CachedReportingEndpointGroup& group = *endpoint_groups_it;
    const ReportingEndpointGroupKey& group_key = group.group_key;

    // These things should probably never happen:
    if (group_key < endpoints_it->group_key) {
      // This endpoint group has no associated endpoints, so move on to the next
      // endpoint group.
      ++endpoint_groups_it;
      continue;
    } else if (group_key > endpoints_it->group_key) {
      // This endpoint has no associated endpoint group, so move on to the next
      // endpoint.
      ++endpoints_it;
      continue;
    }

    DCHECK_EQ(group_key, endpoints_it->group_key);

    size_t cur_group_endpoints_count = 0;

    // Insert the endpoints corresponding to this group.
    while (endpoints_it != loaded_endpoints.end() &&
           endpoints_it->group_key == group_key) {
      if (FindEndpointIt(group_key, endpoints_it->info.url) !=
          endpoints_.end()) {
        // This endpoint is duplicated in the store, so discard it and move on
        // to the next endpoint. This should not happen unless the store is
        // corrupted.
        ++endpoints_it;
        continue;
      }
      EndpointMap::iterator inserted =
          endpoints_.emplace(group_key, std::move(*endpoints_it));
      endpoint_its_by_url_.emplace(inserted->second.info.url, inserted);
      ++cur_group_endpoints_count;
      ++endpoints_it;
    }

    if (!client ||
        client->network_anonymization_key !=
            group_key.network_anonymization_key ||
        client->origin != group_key.origin) {
      // Store the old client and start a new one.
      if (client) {
        ClientMap::iterator client_it =
            clients_.emplace(client->origin.host(), std::move(*client));
        EnforcePerClientAndGlobalEndpointLimits(client_it);
      }
      DCHECK(FindClientIt(group_key) == clients_.end());
      // V0 endpoints should have an origin.
      DCHECK(group_key.origin.has_value());
      client = std::make_optional(Client(group_key.network_anonymization_key,
                                         group_key.origin.value()));
    }
    DCHECK(client.has_value());
    client->endpoint_group_names.insert(group_key.group_name);
    client->endpoint_count += cur_group_endpoints_count;
    client->last_used = std::max(client->last_used, group.last_used);

    endpoint_groups_.emplace(group_key, std::move(group));

    ++endpoint_groups_it;
  }

  if (client) {
    DCHECK(FindClientIt(client->network_anonymization_key, client->origin) ==
           clients_.end());
    ClientMap::iterator client_it =
        clients_.emplace(client->origin.host(), std::move(*client));
    EnforcePerClientAndGlobalEndpointLimits(client_it);
  }

  ConsistencyCheckClients();
}

// Until the V0 Reporting API is deprecated and removed, this method needs to
// handle endpoint groups configured by both the V0 Report-To header, which are
// persisted and used by any resource on the origin which defined them, as well
// as the V1 Reporting-Endpoints header, which defines ephemeral endpoints
// which can only be used by the resource which defines them.
// In order to properly isolate reports from different documents, any reports
// which can be sent to a V1 endpoint must be. V0 endpoints are selected only
// for those reports with no reporting source token, or when no matching V1
// endpoint has been configured.
// To achieve this, the reporting service continues to use the EndpointGroupKey
// structure, which uses the presence of an optional reporting source token to
// distinguish V1 endpoints from V0 endpoint groups.
std::vector<ReportingEndpoint>
ReportingCacheImpl::GetCandidateEndpointsForDelivery(
    const ReportingEndpointGroupKey& group_key) {
  base::Time now = clock().Now();
  ConsistencyCheckClients();

  if (group_key.IsEnterpriseEndpoint()) {
    std::vector<ReportingEndpoint> endpoints_out;
    for (const ReportingEndpoint& endpoint : enterprise_endpoints_) {
      if (endpoint.group_key == group_key) {
        endpoints_out.push_back(endpoint);
      }
    }
    return endpoints_out;
  }

  // If |group_key| has a defined |reporting_source| field, then this method is
  // being called for reports with an associated source. We need to first look
  // for a matching V1 endpoint, based on |reporting_source| and |group_name|.
  if (group_key.IsDocumentEndpoint()) {
    const auto it =
        document_endpoints_.find(group_key.reporting_source.value());
    if (it != document_endpoints_.end()) {
      for (const ReportingEndpoint& endpoint : it->second) {
        if (endpoint.group_key == group_key) {
          return {endpoint};
        }
      }
    }
  }

  // Either |group_key| does not have a defined |reporting_source|, which means
  // that this method was called for reports without a source (e.g. NEL), or
  // we tried and failed to find an appropriate V1 endpoint. In either case, we
  // now look for the appropriate V0 endpoints.

  // We need to clear out the |reporting_source| field to get a group key which
  // can be compared to any V0 endpoint groups.
  // V0 endpoints should have an origin.
  DCHECK(group_key.origin.has_value());
  ReportingEndpointGroupKey v0_lookup_group_key(
      group_key.network_anonymization_key, group_key.origin.value(),
      group_key.group_name, group_key.target_type);

  // Look for an exact origin match for |origin| and |group|.
  EndpointGroupMap::iterator group_it =
      FindEndpointGroupIt(v0_lookup_group_key);
  if (group_it != endpoint_groups_.end() && group_it->second.expires > now) {
    ClientMap::iterator client_it = FindClientIt(v0_lookup_group_key);
    MarkEndpointGroupAndClientUsed(client_it, group_it, now);
    ConsistencyCheckClients();
    context_->NotifyCachedClientsUpdated();
    return GetEndpointsInGroup(group_it->first);
  }

  // If no endpoints were found for an exact match, look for superdomain matches
  // TODO(chlily): Limit the number of labels to go through when looking for a
  // superdomain match.
  // V0 endpoints should have an origin.
  DCHECK(v0_lookup_group_key.origin.has_value());
  std::string domain = v0_lookup_group_key.origin.value().host();
  while (!domain.empty()) {
    const auto domain_range = clients_.equal_range(domain);
    for (auto client_it = domain_range.first; client_it != domain_range.second;
         ++client_it) {
      // Client for a superdomain of |origin|
      const Client& client = client_it->second;
      if (client.network_anonymization_key !=
          v0_lookup_group_key.network_anonymization_key) {
        continue;
      }
      ReportingEndpointGroupKey superdomain_lookup_group_key(
          v0_lookup_group_key.network_anonymization_key, client.origin,
          v0_lookup_group_key.group_name, v0_lookup_group_key.target_type);
      group_it = FindEndpointGroupIt(superdomain_lookup_group_key);

      if (group_it == endpoint_groups_.end())
        continue;

      const CachedReportingEndpointGroup& endpoint_group = group_it->second;
      // Check if the group is valid (unexpired and includes subdomains).
      if (endpoint_group.include_subdomains == OriginSubdomains::INCLUDE &&
          endpoint_group.expires > now) {
        MarkEndpointGroupAndClientUsed(client_it, group_it, now);
        ConsistencyCheckClients();
        context_->NotifyCachedClientsUpdated();
        return GetEndpointsInGroup(superdomain_lookup_group_key);
      }
    }
    domain = GetSuperdomain(domain);
  }
  return std::vector<ReportingEndpoint>();
}

base::Value ReportingCacheImpl::GetClientsAsValue() const {
  ConsistencyCheckClients();
  base::Value::List client_list;
  for (const auto& domain_and_client : clients_) {
    const Client& client = domain_and_client.second;
    client_list.Append(GetClientAsValue(client));
  }
  return base::Value(std::move(client_list));
}

size_t ReportingCacheImpl::GetEndpointCount() const {
  return endpoints_.size();
}

void ReportingCacheImpl::Flush() {
  if (context_->IsClientDataPersisted())
    store()->Flush();
}

ReportingEndpoint ReportingCacheImpl::GetV1EndpointForTesting(
    const base::UnguessableToken& reporting_source,
    const std::string& endpoint_name) const {
  DCHECK(!reporting_source.is_empty());
  const auto it = document_endpoints_.find(reporting_source);
  if (it != document_endpoints_.end()) {
    for (const ReportingEndpoint& endpoint : it->second) {
      if (endpoint_name == endpoint.group_key.group_name)
        return endpoint;
    }
  }
  return ReportingEndpoint();
}

ReportingEndpoint ReportingCacheImpl::GetEndpointForTesting(
    const ReportingEndpointGroupKey& group_key,
    const GURL& url) const {
  ConsistencyCheckClients();
  for (const auto& group_key_and_endpoint : endpoints_) {
    const ReportingEndpoint& endpoint = group_key_and_endpoint.second;
    if (endpoint.group_key == group_key && endpoint.info.url == url)
      return endpoint;
  }
  return ReportingEndpoint();
}

std::vector<ReportingEndpoint>
ReportingCacheImpl::GetEnterpriseEndpointsForTesting() const {
  return enterprise_endpoints_;
}

bool ReportingCacheImpl::EndpointGroupExistsForTesting(
    const ReportingEndpointGroupKey& group_key,
    OriginSubdomains include_subdomains,
    base::Time expires) const {
  ConsistencyCheckClients();
  for (const auto& key_and_group : endpoint_groups_) {
    const CachedReportingEndpointGroup& endpoint_group = key_and_group.second;
    if (endpoint_group.group_key == group_key &&
        endpoint_group.include_subdomains == include_subdomains) {
      if (expires != base::Time())
        return endpoint_group.expires == expires;
      return true;
    }
  }
  return false;
}

bool ReportingCacheImpl::ClientExistsForTesting(
    const NetworkAnonymizationKey& network_anonymization_key,
    const url::Origin& origin) const {
  ConsistencyCheckClients();
  for (const auto& domain_and_client : clients_) {
    const Client& client = domain_and_client.second;
    DCHECK_EQ(client.origin.host(), domain_and_client.first);
    if (client.network_anonymization_key == network_anonymization_key &&
        client.origin == origin) {
      return true;
    }
  }
  return false;
}

size_t ReportingCacheImpl::GetEndpointGroupCountForTesting() const {
  return endpoint_groups_.size();
}

size_t ReportingCacheImpl::GetClientCountForTesting() const {
  return clients_.size();
}

size_t ReportingCacheImpl::GetReportingSourceCountForTesting() const {
  return document_endpoints_.size();
}

void ReportingCacheImpl::SetV1EndpointForTesting(
    const ReportingEndpointGroupKey& group_key,
    const base::UnguessableToken& reporting_source,
    const IsolationInfo& isolation_info,
    const GURL& url) {
  DCHECK(!reporting_source.is_empty());
  DCHECK(group_key.IsDocumentEndpoint());
  DCHECK_EQ(reporting_source, group_key.reporting_source.value());
  DCHECK(group_key.network_anonymization_key ==
         isolation_info.network_anonymization_key());

  ReportingEndpoint::EndpointInfo info;
  info.url = url;
  ReportingEndpoint new_endpoint(group_key, info);
  if (document_endpoints_.count(reporting_source) > 0) {
    // The endpoints list is const, so remove and replace with an updated list.
    std::vector<ReportingEndpoint> endpoints =
        document_endpoints_.at(reporting_source);
    endpoints.push_back(std::move(new_endpoint));
    document_endpoints_.erase(reporting_source);
    document_endpoints_.insert({reporting_source, std::move(endpoints)});
  } else {
    document_endpoints_.insert({reporting_source, {std::move(new_endpoint)}});
  }
  // If this is the first time we've used this reporting_source, then add the
  // isolation info. Otherwise, ensure that it is the same as what was used
  // previously.
  if (isolation_info_.count(reporting_source) == 0) {
    isolation_info_.insert({reporting_source, isolation_info});
  } else {
    DCHECK(isolation_info_.at(reporting_source)
               .IsEqualForTesting(isolation_info));  // IN-TEST
  }
  // Document endpoints should have an origin.
  DCHECK(group_key.origin.has_value());
  context_->NotifyEndpointsUpdatedForOrigin(
      FilterEndpointsByOrigin(document_endpoints_, group_key.origin.value()));
}

void ReportingCacheImpl::SetEnterpriseEndpointForTesting(
    const ReportingEndpointGroupKey& group_key,
    const GURL& url) {
  DCHECK(group_key.IsEnterpriseEndpoint());

  ReportingEndpoint::EndpointInfo info;
  info.url = url;
  ReportingEndpoint new_endpoint(group_key, info);
  enterprise_endpoints_.push_back(std::move(new_endpoint));
}

void ReportingCacheImpl::SetEndpointForTesting(
    const ReportingEndpointGroupKey& group_key,
    const GURL& url,
    OriginSubdomains include_subdomains,
    base::Time expires,
    int priority,
    int weight) {
  ClientMap::iterator client_it = FindClientIt(group_key);
  // If the client doesn't yet exist, add it.
  if (client_it == clients_.end()) {
    // V0 endpoints should have an origin.
    DCHECK(group_key.origin.has_value());
    Client new_client(group_key.network_anonymization_key,
                      group_key.origin.value());
    std::string domain = group_key.origin.value().host();
    client_it = clients_.emplace(domain, std::move(new_client));
  }

  base::Time now = clock().Now();

  EndpointGroupMap::iterator group_it = FindEndpointGroupIt(group_key);
  // If the endpoint group doesn't yet exist, add it.
  if (group_it == endpoint_groups_.end()) {
    CachedReportingEndpointGroup new_group(group_key, include_subdomains,
                                           expires, now);
    group_it = endpoint_groups_.emplace(group_key, std::move(new_group)).first;
    client_it->second.endpoint_group_names.insert(group_key.group_name);
  } else {
    // Otherwise, update the existing entry
    group_it->second.include_subdomains = include_subdomains;
    group_it->second.expires = expires;
    group_it->second.last_used = now;
  }

  MarkEndpointGroupAndClientUsed(client_it, group_it, now);

  EndpointMap::iterator endpoint_it = FindEndpointIt(group_key, url);
  // If the endpoint doesn't yet exist, add it.
  if (endpoint_it == endpoints_.end()) {
    ReportingEndpoint::EndpointInfo info;
    info.url = url;
    info.priority = priority;
    info.weight = weight;
    ReportingEndpoint new_endpoint(group_key, info);
    endpoint_it = endpoints_.emplace(group_key, std::move(new_endpoint));
    AddEndpointItToIndex(endpoint_it);
    ++client_it->second.endpoint_count;
  } else {
    // Otherwise, update the existing entry
    endpoint_it->second.info.priority = priority;
    endpoint_it->second.info.weight = weight;
  }

  EnforcePerClientAndGlobalEndpointLimits(client_it);
  ConsistencyCheckClients();
  context_->NotifyCachedClientsUpdated();
}

IsolationInfo ReportingCacheImpl::GetIsolationInfoForEndpoint(
    const ReportingEndpoint& endpoint) const {
  // Enterprise endpoints do not use a NetworkAnonymizationKey or an
  // IsolationInfo, but they need a non-empty IsolationInfo for reports to be
  // uploaded. Enterprise endpoints are profile-bound and
  // not document-bound like web developer endpoints.
  if (endpoint.group_key.target_type == ReportingTargetType::kEnterprise) {
    return IsolationInfo::CreateTransient();
  }
  // V0 endpoint groups do not support credentials.
  if (!endpoint.group_key.reporting_source.has_value()) {
    // TODO(crbug.com/344943210): Remove this and have a better way to get a
    // correct IsolationInfo here.
    return IsolationInfo::DoNotUseCreatePartialFromNak(
        endpoint.group_key.network_anonymization_key);
  }
  const auto it =
      isolation_info_.find(endpoint.group_key.reporting_source.value());
  CHECK(it != isolation_info_.end(), base::NotFatalUntil::M130);
  return it->second;
}

ReportingCacheImpl::Client::Client(
    const NetworkAnonymizationKey& network_anonymization_key,
    const url::Origin& origin)
    : network_anonymization_key(network_anonymization_key), origin(origin) {}

ReportingCacheImpl::Client::Client(const Client& other) = default;

ReportingCacheImpl::Client::Client(Client&& other) = default;

ReportingCacheImpl::Client& ReportingCacheImpl::Client::operator=(
    const Client& other) = default;

ReportingCacheImpl::Client& ReportingCacheImpl::Client::operator=(
    Client&& other) = default;

ReportingCacheImpl::Client::~Client() = default;

ReportingCacheImpl::ReportSet::const_iterator
ReportingCacheImpl::FindReportToEvict() const {
  ReportSet::const_iterator to_evict = reports_.end();

  for (auto it = reports_.begin(); it != reports_.end(); ++it) {
    // Don't evict pending or doomed reports.
    if (it->get()->IsUploadPending())
      continue;
    if (to_evict == reports_.end() ||
        it->get()->queued < to_evict->get()->queued) {
      to_evict = it;
    }
  }

  return to_evict;
}

void ReportingCacheImpl::ConsistencyCheckClients() const {
  // TODO(crbug.com/40054414): Remove this CHECK once the investigation is done.
  CHECK_LE(endpoint_groups_.size(), context_->policy().max_endpoint_count);
#if DCHECK_IS_ON()
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  size_t total_endpoint_count = 0;
  size_t total_endpoint_group_count = 0;
  std::set<std::pair<NetworkAnonymizationKey, url::Origin>>
      nik_origin_pairs_in_cache;

  for (const auto& domain_and_client : clients_) {
    const std::string& domain = domain_and_client.first;
    const Client& client = domain_and_client.second;
    total_endpoint_count += client.endpoint_count;
    total_endpoint_group_count += ConsistencyCheckClient(domain, client);

    auto inserted = nik_origin_pairs_in_cache.emplace(
        client.network_anonymization_key, client.origin);
    // We have not seen a duplicate client with the same NAK and origin.
    DCHECK(inserted.second);
  }

  // Global endpoint cap is respected.
  DCHECK_LE(GetEndpointCount(), context_->policy().max_endpoint_count);
  // The number of endpoint groups must not exceed the number of endpoints.
  DCHECK_LE(endpoint_groups_.size(), GetEndpointCount());

  // All the endpoints and endpoint groups are accounted for.
  DCHECK_EQ(total_endpoint_count, endpoints_.size());
  DCHECK_EQ(total_endpoint_group_count, endpoint_groups_.size());

  // All the endpoints are indexed properly.
  DCHECK_EQ(total_endpoint_count, endpoint_its_by_url_.size());
  for (const auto& url_and_endpoint_it : endpoint_its_by_url_) {
    DCHECK_EQ(url_and_endpoint_it.first,
              url_and_endpoint_it.second->second.info.url);
  }
}

size_t ReportingCacheImpl::ConsistencyCheckClient(const std::string& domain,
                                                  const Client& client) const {
  // Each client is keyed by its domain name.
  DCHECK_EQ(domain, client.origin.host());
  // Client is not empty (has at least one group)
  DCHECK(!client.endpoint_group_names.empty());

  size_t endpoint_count_in_client = 0;
  size_t endpoint_group_count_in_client = 0;

  for (const std::string& group_name : client.endpoint_group_names) {
    size_t groups_with_name = 0;
    for (const auto& key_and_group : endpoint_groups_) {
      const ReportingEndpointGroupKey& key = key_and_group.first;
      // There should not be any V1 document endpoints; this is a V0 endpoint
      // group.
      DCHECK(!key_and_group.first.IsDocumentEndpoint());
      if (key.origin == client.origin &&
          key.network_anonymization_key == client.network_anonymization_key &&
          key.group_name == group_name) {
        ++endpoint_group_count_in_client;
        ++groups_with_name;
        endpoint_count_in_client +=
            ConsistencyCheckEndpointGroup(key, key_and_group.second);
      }
    }
    DCHECK_EQ(1u, groups_with_name);
  }
  // Client has the correct endpoint count.
  DCHECK_EQ(client.endpoint_count, endpoint_count_in_client);
  // Per-client endpoint cap is respected.
  DCHECK_LE(client.endpoint_count, context_->policy().max_endpoints_per_origin);

  // Note: Not checking last_used time here because base::Time is not
  // guaranteed to be monotonically non-decreasing.

  return endpoint_group_count_in_client;
}

size_t ReportingCacheImpl::ConsistencyCheckEndpointGroup(
    const ReportingEndpointGroupKey& key,
    const CachedReportingEndpointGroup& group) const {
  size_t endpoint_count_in_group = 0;

  // Each group is keyed by its origin and name.
  DCHECK(key == group.group_key);

  // Group is not empty (has at least one endpoint)
  DCHECK_LE(0u, GetEndpointCountInGroup(group.group_key));

  // Note: Not checking expiry here because expired groups are allowed to
  // linger in the cache until they are garbage collected.

  std::set<GURL> endpoint_urls_in_group;

  const auto group_range = endpoints_.equal_range(key);
  for (auto it = group_range.first; it != group_range.second; ++it) {
    const ReportingEndpoint& endpoint = it->second;

    ConsistencyCheckEndpoint(key, endpoint, it);

    auto inserted = endpoint_urls_in_group.insert(endpoint.info.url);
    // We have not seen a duplicate endpoint with the same URL in this
    // group.
    DCHECK(inserted.second);

    ++endpoint_count_in_group;
  }

  return endpoint_count_in_group;
}

void ReportingCacheImpl::ConsistencyCheckEndpoint(
    const ReportingEndpointGroupKey& key,
    const ReportingEndpoint& endpoint,
    EndpointMap::const_iterator endpoint_it) const {
  // Origin and group name match.
  DCHECK(key == endpoint.group_key);

  // Priority and weight are nonnegative integers.
  DCHECK_LE(0, endpoint.info.priority);
  DCHECK_LE(0, endpoint.info.weight);

  // The endpoint is in the |endpoint_its_by_url_| index.
  DCHECK(base::Contains(endpoint_its_by_url_, endpoint.info.url));
  auto url_range = endpoint_its_by_url_.equal_range(endpoint.info.url);
  std::vector<EndpointMap::iterator> endpoint_its_for_url;
  for (auto index_it = url_range.first; index_it != url_range.second;
       ++index_it) {
    endpoint_its_for_url.push_back(index_it->second);
  }
  DCHECK(base::Contains(endpoint_its_for_url, endpoint_it));
#endif  // DCHECK_IS_ON()
}

ReportingCacheImpl::ClientMap::iterator ReportingCacheImpl::FindClientIt(
    const NetworkAnonymizationKey& network_anonymization_key,
    const url::Origin& origin) {
  // TODO(chlily): Limit the number of clients per domain to prevent an attacker
  // from installing many Reporting policies for different port numbers on the
  // same host.
  const auto domain_range = clients_.equal_range(origin.host());
  for (auto it = domain_range.first; it != domain_range.second; ++it) {
    if (it->second.network_anonymization_key == network_anonymization_key &&
        it->second.origin == origin) {
      return it;
    }
  }
  return clients_.end();
}

ReportingCacheImpl::ClientMap::iterator ReportingCacheImpl::FindClientIt(
    const ReportingEndpointGroupKey& group_key) {
  // V0 endpoints should have an origin.
  DCHECK(group_key.origin.has_value());
  return FindClientIt(group_key.network_anonymization_key,
                      group_key.origin.value());
}

ReportingCacheImpl::EndpointGroupMap::iterator
ReportingCacheImpl::FindEndpointGroupIt(
    const ReportingEndpointGroupKey& group_key) {
  return endpoint_groups_.find(group_key);
}

ReportingCacheImpl::EndpointMap::iterator ReportingCacheImpl::FindEndpointIt(
    const ReportingEndpointGroupKey& group_key,
    const GURL& url) {
  const auto group_range = endpoints_.equal_range(group_key);
  for (auto it = group_range.first; it != group_range.second; ++it) {
    if (it->second.info.url == url)
      return it;
  }
  return endpoints_.end();
}

ReportingCacheImpl::ClientMap::iterator ReportingCacheImpl::AddOrUpdateClient(
    Client new_client) {
  ClientMap::iterator client_it =
      FindClientIt(new_client.network_anonymization_key, new_client.origin);

  // Add a new client for this NAK and origin.
  if (client_it == clients_.end()) {
    std::string domain = new_client.origin.host();
    client_it = clients_.emplace(std::move(domain), std::move(new_client));
  } else {
    // If an entry already existed, just update it.
    Client& old_client = client_it->second;
    old_client.endpoint_count = new_client.endpoint_count;
    old_client.endpoint_group_names =
        std::move(new_client.endpoint_group_names);
    old_client.last_used = new_client.last_used;
  }

  // Note: ConsistencyCheckClients() may fail here because we may be over the
  // global/per-origin endpoint limits.
  return client_it;
}

void ReportingCacheImpl::AddOrUpdateEndpointGroup(
    CachedReportingEndpointGroup new_group) {
  EndpointGroupMap::iterator group_it =
      FindEndpointGroupIt(new_group.group_key);

  // Add a new endpoint group for this origin and group name.
  if (group_it == endpoint_groups_.end()) {
    if (context_->IsClientDataPersisted())
      store()->AddReportingEndpointGroup(new_group);

    endpoint_groups_.emplace(new_group.group_key, std::move(new_group));
    return;
  }

  // If an entry already existed, just update it.
  CachedReportingEndpointGroup& old_group = group_it->second;
  old_group.include_subdomains = new_group.include_subdomains;
  old_group.expires = new_group.expires;
  old_group.last_used = new_group.last_used;

  if (context_->IsClientDataPersisted())
    store()->UpdateReportingEndpointGroupDetails(new_group);

  // Note: ConsistencyCheckClients() may fail here because we have not yet
  // added/updated the Client yet.
}

void ReportingCacheImpl::AddOrUpdateEndpoint(ReportingEndpoint new_endpoint) {
  EndpointMap::iterator endpoint_it =
      FindEndpointIt(new_endpoint.group_key, new_endpoint.info.url);

  // Add a new endpoint for this origin, group, and url.
  if (endpoint_it == endpoints_.end()) {
    if (context_->IsClientDataPersisted())
      store()->AddReportingEndpoint(new_endpoint);

    endpoint_it =
        endpoints_.emplace(new_endpoint.group_key, std::move(new_endpoint));
    AddEndpointItToIndex(endpoint_it);

    // If the client already exists, update its endpoint count.
    ClientMap::iterator client_it = FindClientIt(endpoint_it->second.group_key);
    if (client_it != clients_.end())
      ++client_it->second.endpoint_count;
    return;
  }

  // If an entry already existed, just update it.
  ReportingEndpoint& old_endpoint = endpoint_it->second;
  old_endpoint.info.priority = new_endpoint.info.priority;
  old_endpoint.info.weight = new_endpoint.info.weight;
  // |old_endpoint.stats| stays the same.

  if (context_->IsClientDataPersisted())
    store()->UpdateReportingEndpointDetails(new_endpoint);

  // Note: ConsistencyCheckClients() may fail here because we have not yet
  // added/updated the Client yet.
}

void ReportingCacheImpl::RemoveEndpointsInGroupOtherThan(
    const ReportingEndpointGroupKey& group_key,
    const std::set<GURL>& endpoints_to_keep_urls) {
  EndpointGroupMap::iterator group_it = FindEndpointGroupIt(group_key);
  if (group_it == endpoint_groups_.end())
    return;
  ClientMap::iterator client_it = FindClientIt(group_key);
  // Normally a group would not exist without a client for that origin, but
  // this can actually happen during header parsing if a header for an origin
  // without a pre-existing configuration erroneously contains multiple groups
  // with the same name. In that case, we assume here that they meant to set all
  // of those same-name groups as one group, so we don't remove anything.
  if (client_it == clients_.end())
    return;

  const auto group_range = endpoints_.equal_range(group_key);
  for (auto it = group_range.first; it != group_range.second;) {
    if (base::Contains(endpoints_to_keep_urls, it->second.info.url)) {
      ++it;
      continue;
    }

    // This may invalidate |group_it| (and also possibly |client_it|), but only
    // if we are processing the last remaining endpoint in the group.
    std::optional<EndpointMap::iterator> next_it =
        RemoveEndpointInternal(client_it, group_it, it);
    if (!next_it.has_value())
      return;
    it = next_it.value();
  }
}

void ReportingCacheImpl::RemoveEndpointGroupsForClientOtherThan(
    const NetworkAnonymizationKey& network_anonymization_key,
    const url::Origin& origin,
    const std::set<std::string>& groups_to_keep_names) {
  ClientMap::iterator client_it =
      FindClientIt(network_anonymization_key, origin);
  if (client_it == clients_.end())
    return;

  std::set<std::string>& old_group_names =
      client_it->second.endpoint_group_names;
  std::vector<std::string> groups_to_remove_names =
      base::STLSetDifference<std::vector<std::string>>(old_group_names,
                                                       groups_to_keep_names);

  for (const std::string& group_name : groups_to_remove_names) {
    // The target_type is set to kDeveloper because this function is used for
    // V0 reporting, which only includes web developer entities.
    EndpointGroupMap::iterator group_it = FindEndpointGroupIt(
        ReportingEndpointGroupKey(network_anonymization_key, origin, group_name,
                                  ReportingTargetType::kDeveloper));
    RemoveEndpointGroupInternal(client_it, group_it);
  }
}

std::vector<ReportingEndpoint> ReportingCacheImpl::GetEndpointsInGroup(
    const ReportingEndpointGroupKey& group_key) const {
  const auto group_range = endpoints_.equal_range(group_key);
  std::vector<ReportingEndpoint> endpoints_out;
  for (auto it = group_range.first; it != group_range.second; ++it) {
    endpoints_out.push_back(it->second);
  }
  return endpoints_out;
}

size_t ReportingCacheImpl::GetEndpointCountInGroup(
    const ReportingEndpointGroupKey& group_key) const {
  return endpoints_.count(group_key);
}

void ReportingCacheImpl::MarkEndpointGroupAndClientUsed(
    ClientMap::iterator client_it,
    EndpointGroupMap::iterator group_it,
    base::Time now) {
  group_it->second.last_used = now;
  client_it->second.last_used = now;
  if (context_->IsClientDataPersisted())
    store()->UpdateReportingEndpointGroupAccessTime(group_it->second);
}

std::optional<ReportingCacheImpl::EndpointMap::iterator>
ReportingCacheImpl::RemoveEndpointInternal(ClientMap::iterator client_it,
                                           EndpointGroupMap::iterator group_it,
                                           EndpointMap::iterator endpoint_it) {
  CHECK(client_it != clients_.end(), base::NotFatalUntil::M130);
  CHECK(group_it != endpoint_groups_.end(), base::NotFatalUntil::M130);
  CHECK(endpoint_it != endpoints_.end(), base::NotFatalUntil::M130);

  const ReportingEndpointGroupKey& group_key = endpoint_it->first;
  // If this is the only endpoint in the group, then removing it will cause the
  // group to become empty, so just remove the whole group. The client may also
  // be removed if it becomes empty.
  if (endpoints_.count(group_key) == 1) {
    RemoveEndpointGroupInternal(client_it, group_it);
    return std::nullopt;
  }
  // Otherwise, there are other endpoints in the group, so there is no chance
  // of needing to remove the group/client. Just remove this endpoint and
  // update the client's endpoint count.
  DCHECK_GT(client_it->second.endpoint_count, 1u);
  RemoveEndpointItFromIndex(endpoint_it);
  --client_it->second.endpoint_count;
  if (context_->IsClientDataPersisted())
    store()->DeleteReportingEndpoint(endpoint_it->second);
  return endpoints_.erase(endpoint_it);
}

std::optional<ReportingCacheImpl::EndpointGroupMap::iterator>
ReportingCacheImpl::RemoveEndpointGroupInternal(
    ClientMap::iterator client_it,
    EndpointGroupMap::iterator group_it,
    size_t* num_endpoints_removed) {
  CHECK(client_it != clients_.end(), base::NotFatalUntil::M130);
  CHECK(group_it != endpoint_groups_.end(), base::NotFatalUntil::M130);
  const ReportingEndpointGroupKey& group_key = group_it->first;

  // Remove the endpoints for this group.
  const auto group_range = endpoints_.equal_range(group_key);
  size_t endpoints_removed =
      std::distance(group_range.first, group_range.second);
  DCHECK_GT(endpoints_removed, 0u);
  if (num_endpoints_removed)
    *num_endpoints_removed += endpoints_removed;
  for (auto it = group_range.first; it != group_range.second; ++it) {
    if (context_->IsClientDataPersisted())
      store()->DeleteReportingEndpoint(it->second);

    RemoveEndpointItFromIndex(it);
  }
  endpoints_.erase(group_range.first, group_range.second);

  // Update the client's endpoint count.
  Client& client = client_it->second;
  client.endpoint_count -= endpoints_removed;

  // Remove endpoint group from client.
  size_t erased_from_client =
      client.endpoint_group_names.erase(group_key.group_name);
  DCHECK_EQ(1u, erased_from_client);

  if (context_->IsClientDataPersisted())
    store()->DeleteReportingEndpointGroup(group_it->second);

  EndpointGroupMap::iterator rv = endpoint_groups_.erase(group_it);

  // Delete client if empty.
  if (client.endpoint_count == 0) {
    DCHECK(client.endpoint_group_names.empty());
    clients_.erase(client_it);
    return std::nullopt;
  }
  return rv;
}

ReportingCacheImpl::ClientMap::iterator
ReportingCacheImpl::RemoveClientInternal(ClientMap::iterator client_it) {
  CHECK(client_it != clients_.end(), base::NotFatalUntil::M130);
  const Client& client = client_it->second;

  // Erase all groups in this client, and all endpoints in those groups.
  for (const std::string& group_name : client.endpoint_group_names) {
    // The target_type is set to kDeveloper because this function is used for
    // V0 reporting, which only includes web developer entities.
    ReportingEndpointGroupKey group_key(client.network_anonymization_key,
                                        client.origin, group_name,
                                        ReportingTargetType::kDeveloper);
    EndpointGroupMap::iterator group_it = FindEndpointGroupIt(group_key);
    if (context_->IsClientDataPersisted())
      store()->DeleteReportingEndpointGroup(group_it->second);
    endpoint_groups_.erase(group_it);

    const auto group_range = endpoints_.equal_range(group_key);
    for (auto it = group_range.first; it != group_range.second; ++it) {
      if (context_->IsClientDataPersisted())
        store()->DeleteReportingEndpoint(it->second);

      RemoveEndpointItFromIndex(it);
    }
    endpoints_.erase(group_range.first, group_range.second);
  }

  return clients_.erase(client_it);
}

void ReportingCacheImpl::EnforcePerClientAndGlobalEndpointLimits(
    ClientMap::iterator client_it) {
  CHECK(client_it != clients_.end(), base::NotFatalUntil::M130);
  size_t client_endpoint_count = client_it->second.endpoint_count;
  // TODO(chlily): This is actually a limit on the endpoints for a given client
  // (for a NAK, origin pair). Rename this.
  size_t max_endpoints_per_origin = context_->policy().max_endpoints_per_origin;
  if (client_endpoint_count > max_endpoints_per_origin) {
    EvictEndpointsFromClient(client_it,
                             client_endpoint_count - max_endpoints_per_origin);
  }

  size_t max_endpoint_count = context_->policy().max_endpoint_count;
  while (GetEndpointCount() > max_endpoint_count) {
    // Find the stalest client (arbitrarily pick the first one if there are
    // multiple).
    ClientMap::iterator to_evict = clients_.end();
    for (auto it = clients_.begin(); it != clients_.end(); ++it) {
      const Client& client = it->second;
      if (to_evict == clients_.end() ||
          client.last_used < to_evict->second.last_used) {
        to_evict = it;
      }
    }

    CHECK(to_evict != clients_.end(), base::NotFatalUntil::M130);

    // Evict endpoints from the chosen client.
    size_t num_to_evict = GetEndpointCount() - max_endpoint_count;
    EvictEndpointsFromClient(
        to_evict, std::min(to_evict->second.endpoint_count, num_to_evict));
  }
}

void ReportingCacheImpl::EvictEndpointsFromClient(ClientMap::iterator client_it,
                                                  size_t endpoints_to_evict) {
  DCHECK_GT(endpoints_to_evict, 0u);
  CHECK(client_it != clients_.end(), base::NotFatalUntil::M130);
  const Client& client = client_it->second;
  // Cache this value as |client| may be deleted.
  size_t client_endpoint_count = client.endpoint_count;
  const NetworkAnonymizationKey& network_anonymization_key =
      client.network_anonymization_key;
  const url::Origin& origin = client.origin;

  DCHECK_GE(client_endpoint_count, endpoints_to_evict);
  if (endpoints_to_evict == client_endpoint_count) {
    RemoveClientInternal(client_it);
    return;
  }

  size_t endpoints_removed = 0;
  bool client_deleted =
      RemoveExpiredOrStaleGroups(client_it, &endpoints_removed);
  // If we deleted the whole client, there is nothing left to do.
  if (client_deleted) {
    DCHECK_EQ(endpoints_removed, client_endpoint_count);
    return;
  }

  DCHECK(!client.endpoint_group_names.empty());

  while (endpoints_removed < endpoints_to_evict) {
    DCHECK_GT(client_it->second.endpoint_count, 0u);
    // Find the stalest group with the most endpoints.
    EndpointGroupMap::iterator stalest_group_it = endpoint_groups_.end();
    size_t stalest_group_endpoint_count = 0;
    for (const std::string& group_name : client.endpoint_group_names) {
      // The target_type is set to kDeveloper because enterprise endpoints
      // follow a different path.
      ReportingEndpointGroupKey group_key(network_anonymization_key, origin,
                                          group_name,
                                          ReportingTargetType::kDeveloper);
      EndpointGroupMap::iterator group_it = FindEndpointGroupIt(group_key);
      size_t group_endpoint_count = GetEndpointCountInGroup(group_key);

      const CachedReportingEndpointGroup& group = group_it->second;
      if (stalest_group_it == endpoint_groups_.end() ||
          group.last_used < stalest_group_it->second.last_used ||
          (group.last_used == stalest_group_it->second.last_used &&
           group_endpoint_count > stalest_group_endpoint_count)) {
        stalest_group_it = group_it;
        stalest_group_endpoint_count = group_endpoint_count;
      }
    }
    CHECK(stalest_group_it != endpoint_groups_.end(),
          base::NotFatalUntil::M130);

    // Evict the least important (lowest priority, lowest weight) endpoint.
    EvictEndpointFromGroup(client_it, stalest_group_it);
    ++endpoints_removed;
  }
}

void ReportingCacheImpl::EvictEndpointFromGroup(
    ClientMap::iterator client_it,
    EndpointGroupMap::iterator group_it) {
  const ReportingEndpointGroupKey& group_key = group_it->first;
  const auto group_range = endpoints_.equal_range(group_key);
  EndpointMap::iterator endpoint_to_evict_it = endpoints_.end();
  for (auto it = group_range.first; it != group_range.second; ++it) {
    const ReportingEndpoint& endpoint = it->second;
    if (endpoint_to_evict_it == endpoints_.end() ||
        // Lower priority = higher numerical value of |priority|.
        endpoint.info.priority > endpoint_to_evict_it->second.info.priority ||
        (endpoint.info.priority == endpoint_to_evict_it->second.info.priority &&
         endpoint.info.weight < endpoint_to_evict_it->second.info.weight)) {
      endpoint_to_evict_it = it;
    }
  }
  CHECK(endpoint_to_evict_it != endpoints_.end(), base::NotFatalUntil::M130);

  RemoveEndpointInternal(client_it, group_it, endpoint_to_evict_it);
}

bool ReportingCacheImpl::RemoveExpiredOrStaleGroups(
    ClientMap::iterator client_it,
    size_t* num_endpoints_removed) {
  base::Time now = clock().Now();
  // Make a copy of this because |client_it| may be invalidated.
  std::set<std::string> groups_in_client_names(
      client_it->second.endpoint_group_names);

  for (const std::string& group_name : groups_in_client_names) {
    // The target_type is set to kDeveloper because enterprise endpoints
    // follow a different path.
    EndpointGroupMap::iterator group_it = FindEndpointGroupIt(
        ReportingEndpointGroupKey(client_it->second.network_anonymization_key,
                                  client_it->second.origin, group_name,
                                  ReportingTargetType::kDeveloper));
    CHECK(group_it != endpoint_groups_.end(), base::NotFatalUntil::M130);
    const CachedReportingEndpointGroup& group = group_it->second;
    if (group.expires < now ||
        now - group.last_used > context_->policy().max_group_staleness) {
      // May delete the client, invalidating |client_it|, but only if we are
      // processing the last remaining group.
      if (!RemoveEndpointGroupInternal(client_it, group_it,
                                       num_endpoints_removed)
               .has_value()) {
        return true;
      }
    }
  }

  return false;
}

void ReportingCacheImpl::AddEndpointItToIndex(
    EndpointMap::iterator endpoint_it) {
  const GURL& url = endpoint_it->second.info.url;
  endpoint_its_by_url_.emplace(url, endpoint_it);
}

void ReportingCacheImpl::RemoveEndpointItFromIndex(
    EndpointMap::iterator endpoint_it) {
  const GURL& url = endpoint_it->second.info.url;
  auto url_range = endpoint_its_by_url_.equal_range(url);
  for (auto it = url_range.first; it != url_range.second; ++it) {
    if (it->second == endpoint_it) {
      endpoint_its_by_url_.erase(it);
      return;
    }
  }
}

base::Value ReportingCacheImpl::GetClientAsValue(const Client& client) const {
  base::Value::Dict client_dict;
  client_dict.Set("network_anonymization_key",
                  client.network_anonymization_key.ToDebugString());
  client_dict.Set("origin", client.origin.Serialize());

  base::Value::List group_list;
  for (const std::string& group_name : client.endpoint_group_names) {
    // The target_type is set to kDeveloper because enterprise endpoints
    // follow a different path.
    ReportingEndpointGroupKey group_key(client.network_anonymization_key,
                                        client.origin, group_name,
                                        ReportingTargetType::kDeveloper);
    const CachedReportingEndpointGroup& group = endpoint_groups_.at(group_key);
    group_list.Append(GetEndpointGroupAsValue(group));
  }

  client_dict.Set("groups", std::move(group_list));

  return base::Value(std::move(client_dict));
}

base::Value ReportingCacheImpl::GetEndpointGroupAsValue(
    const CachedReportingEndpointGroup& group) const {
  base::Value::Dict group_dict;
  group_dict.Set("name", group.group_key.group_name);
  group_dict.Set("expires", NetLog::TimeToString(group.expires));
  group_dict.Set("includeSubdomains",
                 group.include_subdomains == OriginSubdomains::INCLUDE);

  base::Value::List endpoint_list;

  const auto group_range = endpoints_.equal_range(group.group_key);
  for (auto it = group_range.first; it != group_range.second; ++it) {
    const ReportingEndpoint& endpoint = it->second;
    endpoint_list.Append(GetEndpointAsValue(endpoint));
  }

  group_dict.Set("endpoints", std::move(endpoint_list));

  return base::Value(std::move(group_dict));
}

base::Value ReportingCacheImpl::GetEndpointAsValue(
    const ReportingEndpoint& endpoint) const {
  base::Value::Dict endpoint_dict;
  endpoint_dict.Set("url", endpoint.info.url.spec());
  endpoint_dict.Set("priority", endpoint.info.priority);
  endpoint_dict.Set("weight", endpoint.info.weight);

  const ReportingEndpoint::Statistics& stats = endpoint.stats;
  base::Value::Dict successful_dict;
  successful_dict.Set("uploads", stats.successful_uploads);
  successful_dict.Set("reports", stats.successful_reports);
  endpoint_dict.Set("successful", std::move(successful_dict));

  base::Value::Dict failed_dict;
  failed_dict.Set("uploads",
                  stats.attempted_uploads - stats.successful_uploads);
  failed_dict.Set("reports",
                  stats.attempted_reports - stats.successful_reports);
  endpoint_dict.Set("failed", std::move(failed_dict));

  return base::Value(std::move(endpoint_dict));
}

}  // namespace net
