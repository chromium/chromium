// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/reporting/reporting_header_parser.h"

#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "net/base/features.h"
#include "net/base/isolation_info.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/reporting/reporting_cache.h"
#include "net/reporting/reporting_context.h"
#include "net/reporting/reporting_delegate.h"
#include "net/reporting/reporting_endpoint.h"
#include "net/reporting/reporting_target_type.h"

namespace net {

namespace {

const char kUrlKey[] = "url";
const char kIncludeSubdomainsKey[] = "include_subdomains";
const char kEndpointsKey[] = "endpoints";
const char kGroupKey[] = "group";
const char kDefaultGroupName[] = "default";
const char kMaxAgeKey[] = "max_age";
const char kPriorityKey[] = "priority";
const char kWeightKey[] = "weight";

// Processes a single endpoint url string parsed from header.
//
// |endpoint_url_string| is the string value of the endpoint URL.
// |header_origin_url| is the origin URL that sent the header.
//
// |endpoint_url_out| is the endpoint URL parsed out of the string.
// Returns true on success or false if url was invalid.
bool ProcessEndpointURLString(const std::string& endpoint_url_string,
                              const url::Origin& header_origin,
                              GURL& endpoint_url_out) {
  // Support path-absolute-URL string with exactly one leading "/"
  if (std::strspn(endpoint_url_string.c_str(), "/") == 1) {
    endpoint_url_out = header_origin.GetURL().Resolve(endpoint_url_string);
  } else {
    endpoint_url_out = GURL(endpoint_url_string);
  }
  if (!endpoint_url_out.is_valid())
    return false;
  if (!endpoint_url_out.SchemeIsCryptographic())
    return false;
  return true;
}

// Processes a single endpoint tuple received in a Report-To header.
//
// |origin| is the origin that sent the Report-To header.
//
// |value| is the parsed JSON value of the endpoint tuple.
//
// |*endpoint_info_out| will contain the endpoint URL parsed out of the tuple.
// Returns true on success or false if endpoint was discarded.
bool ProcessEndpoint(ReportingDelegate* delegate,
                     const ReportingEndpointGroupKey& group_key,
                     const base::Value& value,
                     ReportingEndpoint::EndpointInfo* endpoint_info_out) {
  const base::Value::Dict* dict = value.GetIfDict();
  if (!dict)
    return false;

  const std::string* endpoint_url_string = dict->FindString(kUrlKey);
  if (!endpoint_url_string)
    return false;

  GURL endpoint_url;
  // V0 endpoints should have an origin.
  DCHECK(group_key.origin.has_value());
  if (!ProcessEndpointURLString(*endpoint_url_string, group_key.origin.value(),
                                endpoint_url)) {
    return false;
  }
  endpoint_info_out->url = std::move(endpoint_url);

  int priority = ReportingEndpoint::EndpointInfo::kDefaultPriority;
  if (const base::Value* priority_value = dict->Find(kPriorityKey)) {
    if (!priority_value->is_int())
      return false;
    priority = priority_value->GetInt();
  }
  if (priority < 0)
    return false;
  endpoint_info_out->priority = priority;

  int weight = ReportingEndpoint::EndpointInfo::kDefaultWeight;
  if (const base::Value* weight_value = dict->Find(kWeightKey)) {
    if (!weight_value->is_int())
      return false;
    weight = weight_value->GetInt();
  }
  if (weight < 0)
    return false;
  endpoint_info_out->weight = weight;

  return delegate->CanSetClient(group_key.origin.value(),
                                endpoint_info_out->url);
}

// Processes a single endpoint group tuple received in a Report-To header.
//
// |origin| is the origin that sent the Report-To header.
//
// |value| is the parsed JSON value of the endpoint group tuple.
// Returns true on successfully adding a non-empty group, or false if endpoint
// group was discarded or processed as a deletion.
bool ProcessEndpointGroup(
    ReportingDelegate* delegate,
    ReportingCache* cache,
    const NetworkAnonymizationKey& network_anonymization_key,
    const url::Origin& origin,
    const base::Value& value,
    ReportingEndpointGroup* parsed_endpoint_group_out) {
  const base::Value::Dict* dict = value.GetIfDict();
  if (!dict)
    return false;

  std::string group_name = kDefaultGroupName;
  if (const base::Value* maybe_group_name = dict->Find(kGroupKey)) {
    if (!maybe_group_name->is_string())
      return false;
    group_name = maybe_group_name->GetString();
  }
  // The target_type is set to kDeveloper because enterprise endpoints are
  // created on a different path.
  ReportingEndpointGroupKey group_key(network_anonymization_key, origin,
                                      group_name,
                                      ReportingTargetType::kDeveloper);
  parsed_endpoint_group_out->group_key = group_key;

  int ttl_sec = dict->FindInt(kMaxAgeKey).value_or(-1);
  if (ttl_sec < 0)
    return false;
  // max_age: 0 signifies removal of the endpoint group.
  if (ttl_sec == 0) {
    cache->RemoveEndpointGroup(group_key);
    return false;
  }
  parsed_endpoint_group_out->ttl = base::Seconds(ttl_sec);

  std::optional<bool> subdomains_bool = dict->FindBool(kIncludeSubdomainsKey);
  if (subdomains_bool && subdomains_bool.value()) {
    // Disallow eTLDs from setting include_subdomains endpoint groups.
    if (registry_controlled_domains::GetRegistryLength(
            origin.GetURL(),
            registry_controlled_domains::INCLUDE_UNKNOWN_REGISTRIES,
            registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES) == 0) {
      return false;
    }

    parsed_endpoint_group_out->include_subdomains = OriginSubdomains::INCLUDE;
  }

  const base::Value::List* endpoint_list = dict->FindList(kEndpointsKey);
  if (!endpoint_list)
    return false;

  std::vector<ReportingEndpoint::EndpointInfo> endpoints;

  for (const base::Value& endpoint : *endpoint_list) {
    ReportingEndpoint::EndpointInfo parsed_endpoint;
    if (ProcessEndpoint(delegate, group_key, endpoint, &parsed_endpoint))
      endpoints.push_back(std::move(parsed_endpoint));
  }

  // Remove the group if it is empty.
  if (endpoints.empty()) {
    cache->RemoveEndpointGroup(group_key);
    return false;
  }

  parsed_endpoint_group_out->endpoints = std::move(endpoints);

  return true;
}

// Processes a single endpoint tuple received in a Reporting-Endpoints header.
//
// |group_key| is the key for the endpoint group this endpoint belongs.
// |endpoint_url_string| is the endpoint url as received in the header.
//
// |endpoint_info_out| is the endpoint info parsed out of the value.
bool ProcessEndpoint(ReportingDelegate* delegate,
                     const ReportingEndpointGroupKey& group_key,
                     const std::string& endpoint_url_string,
                     ReportingEndpoint::EndpointInfo& endpoint_info_out) {
  if (endpoint_url_string.empty())
    return false;

  GURL endpoint_url;
  // Document endpoints should have an origin.
  DCHECK(group_key.origin.has_value());
  if (!ProcessEndpointURLString(endpoint_url_string, group_key.origin.value(),
                                endpoint_url)) {
    return false;
  }
  endpoint_info_out.url = std::move(endpoint_url);
  // Reporting-Endpoints endpoint doesn't have prioirty/weight so set to
  // default.
  endpoint_info_out.priority =
      ReportingEndpoint::EndpointInfo::kDefaultPriority;
  endpoint_info_out.weight = ReportingEndpoint::EndpointInfo::kDefaultWeight;

  return delegate->CanSetClient(group_key.origin.value(),
                                endpoint_info_out.url);
}

// Process a single endpoint received in a Reporting-Endpoints header.
bool ProcessV1Endpoint(ReportingDelegate* delegate,
                       ReportingCache* cache,
                       const base::UnguessableToken& reporting_source,
                       const NetworkAnonymizationKey& network_anonymization_key,
                       const url::Origin& origin,
                       const std::string& endpoint_name,
                       const std::string& endpoint_url_string,
                       ReportingEndpoint& parsed_endpoint_out) {
  DCHECK(!reporting_source.is_empty());
  // The target_type is set to kDeveloper because enterprise endpoints are
  // created on a different path.
  ReportingEndpointGroupKey group_key(network_anonymization_key,
                                      reporting_source, origin, endpoint_name,
                                      ReportingTargetType::kDeveloper);
  parsed_endpoint_out.group_key = group_key;

  ReportingEndpoint::EndpointInfo parsed_endpoint;

  if (!ProcessEndpoint(delegate, group_key, endpoint_url_string,
                       parsed_endpoint)) {
    return false;
  }
  parsed_endpoint_out.info = std::move(parsed_endpoint);
  return true;
}

}  // namespace

std::optional<base::flat_map<std::string, std::string>> ParseReportingEndpoints(
    const std::string& header) {
  // Ignore empty header values. Skip logging metric to maintain parity with
  // ReportingHeaderType::kReportToInvalid.
  if (header.empty())
    return std::nullopt;
  std::optional<structured_headers::Dictionary> header_dict =
      structured_headers::ParseDictionary(header);
  if (!header_dict) {
    ReportingHeaderParser::RecordReportingHeaderType(
        ReportingHeaderParser::ReportingHeaderType::kReportingEndpointsInvalid);
    return std::nullopt;
  }
  base::flat_map<std::string, std::string> parsed_header;
  for (const structured_headers::DictionaryMember& entry : *header_dict) {
    if (entry.second.member_is_inner_list ||
        !entry.second.member.front().item.is_string()) {
      ReportingHeaderParser::RecordReportingHeaderType(
          ReportingHeaderParser::ReportingHeaderType::
              kReportingEndpointsInvalid);
      return std::nullopt;
    }
    const std::string& endpoint_url_string =
        entry.second.member.front().item.GetString();
    parsed_header[entry.first] = endpoint_url_string;
  }
  return parsed_header;
}

// static
void ReportingHeaderParser::RecordReportingHeaderType(
    ReportingHeaderType header_type) {
  base::UmaHistogramEnumeration("Net.Reporting.HeaderType", header_type);
}

// static
void ReportingHeaderParser::ParseReportToHeader(
    ReportingContext* context,
    const NetworkAnonymizationKey& network_anonymization_key,
    const url::Origin& origin,
    const base::Value::List& list) {
  DCHECK(GURL::SchemeIsCryptographic(origin.scheme()));

  ReportingDelegate* delegate = context->delegate();
  ReportingCache* cache = context->cache();

  std::vector<ReportingEndpointGroup> parsed_header;

  for (const auto& group_value : list) {
    ReportingEndpointGroup parsed_endpoint_group;
    if (ProcessEndpointGroup(delegate, cache, network_anonymization_key, origin,
                             group_value, &parsed_endpoint_group)) {
      parsed_header.push_back(std::move(parsed_endpoint_group));
    }
  }

  if (parsed_header.empty() && list.size() > 0) {
    RecordReportingHeaderType(ReportingHeaderType::kReportToInvalid);
  }

  // Remove the client if it has no valid endpoint groups.
  if (parsed_header.empty()) {
    cache->RemoveClient(network_anonymization_key, origin);
    return;
  }

  RecordReportingHeaderType(ReportingHeaderType::kReportTo);

  cache->OnParsedHeader(network_anonymization_key, origin,
                        std::move(parsed_header));
}

// static
void ReportingHeaderParser::ProcessParsedReportingEndpointsHeader(
    ReportingContext* context,
    const base::UnguessableToken& reporting_source,
    const IsolationInfo& isolation_info,
    const NetworkAnonymizationKey& network_anonymization_key,
    const url::Origin& origin,
    base::flat_map<std::string, std::string> header) {
  DCHECK(base::FeatureList::IsEnabled(net::features::kDocumentReporting));
  DCHECK(GURL::SchemeIsCryptographic(origin.scheme()));
  DCHECK(!reporting_source.is_empty());
  DCHECK(network_anonymization_key.IsEmpty() ||
         network_anonymization_key ==
             isolation_info.network_anonymization_key());

  ReportingDelegate* delegate = context->delegate();
  ReportingCache* cache = context->cache();

  std::vector<ReportingEndpoint> parsed_header;

  for (const auto& member : header) {
    ReportingEndpoint parsed_endpoint;
    if (ProcessV1Endpoint(delegate, cache, reporting_source,
                          network_anonymization_key, origin, member.first,
                          member.second, parsed_endpoint)) {
      parsed_header.push_back(std::move(parsed_endpoint));
    }
  }

  if (parsed_header.empty()) {
    RecordReportingHeaderType(ReportingHeaderType::kReportingEndpointsInvalid);
    return;
  }

  RecordReportingHeaderType(ReportingHeaderType::kReportingEndpoints);
  cache->OnParsedReportingEndpointsHeader(reporting_source, isolation_info,
                                          std::move(parsed_header));
}

}  // namespace net
