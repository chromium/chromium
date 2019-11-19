// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/reporting/reporting_header_parser.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "base/values.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/reporting/reporting_cache.h"
#include "net/reporting/reporting_context.h"
#include "net/reporting/reporting_delegate.h"
#include "net/reporting/reporting_endpoint.h"

namespace net {

namespace {

using HeaderEndpointGroupOutcome =
    ReportingHeaderParser::HeaderEndpointGroupOutcome;
using HeaderEndpointOutcome = ReportingHeaderParser::HeaderEndpointOutcome;
using HeaderOutcome = ReportingHeaderParser::HeaderOutcome;

void RecordHeaderOutcome(HeaderOutcome outcome) {
  UMA_HISTOGRAM_ENUMERATION(ReportingHeaderParser::kHeaderOutcomeHistogram,
                            outcome, HeaderOutcome::MAX);
}

void RecordHeaderEndpointGroupOutcome(HeaderEndpointGroupOutcome outcome) {
  UMA_HISTOGRAM_ENUMERATION(
      ReportingHeaderParser::kHeaderEndpointGroupOutcomeHistogram, outcome,
      HeaderEndpointGroupOutcome::MAX);
}

void RecordHeaderEndpointOutcome(HeaderEndpointOutcome outcome) {
  UMA_HISTOGRAM_ENUMERATION(
      ReportingHeaderParser::kHeaderEndpointOutcomeHistogram, outcome,
      HeaderEndpointOutcome::MAX);
}

const char kUrlKey[] = "url";
const char kIncludeSubdomainsKey[] = "include_subdomains";
const char kEndpointsKey[] = "endpoints";
const char kGroupKey[] = "group";
const char kDefaultGroupName[] = "default";
const char kMaxAgeKey[] = "max_age";
const char kPriorityKey[] = "priority";
const char kWeightKey[] = "weight";

// Processes a single endpoint tuple received in a Report-To header.
//
// |origin| is the origin that sent the Report-To header.
//
// |value| is the parsed JSON value of the endpoint tuple.
//
// |*endpoint_out| will contain the endpoint URL parsed out of the tuple.
HeaderEndpointOutcome ProcessEndpoint(
    ReportingDelegate* delegate,
    const url::Origin& origin,
    const base::Value& value,
    ReportingEndpoint::EndpointInfo* endpoint_info_out) {
  const base::DictionaryValue* dict = nullptr;
  if (!value.GetAsDictionary(&dict))
    return HeaderEndpointOutcome::DISCARDED_NOT_DICTIONARY;
  DCHECK(dict);

  std::string endpoint_url_string;
  if (!dict->HasKey(kUrlKey))
    return HeaderEndpointOutcome::DISCARDED_URL_MISSING;
  if (!dict->GetString(kUrlKey, &endpoint_url_string))
    return HeaderEndpointOutcome::DISCARDED_URL_NOT_STRING;

  GURL endpoint_url(endpoint_url_string);
  if (!endpoint_url.is_valid())
    return HeaderEndpointOutcome::DISCARDED_URL_INVALID;
  if (!endpoint_url.SchemeIsCryptographic())
    return HeaderEndpointOutcome::DISCARDED_URL_INSECURE;
  endpoint_info_out->url = std::move(endpoint_url);

  int priority = ReportingEndpoint::EndpointInfo::kDefaultPriority;
  if (dict->HasKey(kPriorityKey) && !dict->GetInteger(kPriorityKey, &priority))
    return HeaderEndpointOutcome::DISCARDED_PRIORITY_NOT_INTEGER;
  if (priority < 0)
    return HeaderEndpointOutcome::DISCARDED_PRIORITY_NEGATIVE;
  endpoint_info_out->priority = priority;

  int weight = ReportingEndpoint::EndpointInfo::kDefaultWeight;
  if (dict->HasKey(kWeightKey) && !dict->GetInteger(kWeightKey, &weight))
    return HeaderEndpointOutcome::DISCARDED_WEIGHT_NOT_INTEGER;
  if (weight < 0)
    return HeaderEndpointOutcome::DISCARDED_WEIGHT_NEGATIVE;
  endpoint_info_out->weight = weight;

  if (!delegate->CanSetClient(origin, endpoint_url))
    return HeaderEndpointOutcome::SET_REJECTED_BY_DELEGATE;

  return HeaderEndpointOutcome::SET;
}

// Processes a single endpoint group tuple received in a Report-To header.
//
// |origin| is the origin that sent the Report-To header.
//
// |value| is the parsed JSON value of the endpoint group tuple.
HeaderEndpointGroupOutcome ProcessEndpointGroup(
    ReportingDelegate* delegate,
    ReportingCache* cache,
    const url::Origin& origin,
    const base::Value& value,
    ReportingEndpointGroup* parsed_endpoint_group_out) {
  const base::DictionaryValue* dict = nullptr;
  if (!value.GetAsDictionary(&dict))
    return HeaderEndpointGroupOutcome::DISCARDED_NOT_DICTIONARY;
  DCHECK(dict);

  std::string group_name = kDefaultGroupName;
  if (dict->HasKey(kGroupKey) && !dict->GetString(kGroupKey, &group_name))
    return HeaderEndpointGroupOutcome::DISCARDED_GROUP_NOT_STRING;
  parsed_endpoint_group_out->name = std::move(group_name);

  int ttl_sec = -1;
  if (!dict->HasKey(kMaxAgeKey))
    return HeaderEndpointGroupOutcome::DISCARDED_TTL_MISSING;
  if (!dict->GetInteger(kMaxAgeKey, &ttl_sec))
    return HeaderEndpointGroupOutcome::DISCARDED_TTL_NOT_INTEGER;
  if (ttl_sec < 0)
    return HeaderEndpointGroupOutcome::DISCARDED_TTL_NEGATIVE;
  // max_age: 0 signifies removal of the endpoint group.
  if (ttl_sec == 0) {
    cache->RemoveEndpointGroup(origin, group_name);
    return HeaderEndpointGroupOutcome::REMOVED_TTL_ZERO;
  }
  parsed_endpoint_group_out->ttl = base::TimeDelta::FromSeconds(ttl_sec);

  bool subdomains_bool = false;
  if (dict->HasKey(kIncludeSubdomainsKey) &&
      dict->GetBoolean(kIncludeSubdomainsKey, &subdomains_bool) &&
      subdomains_bool == true) {
    // Disallow eTLDs from setting include_subdomains endpoint groups.
    if (registry_controlled_domains::GetRegistryLength(
            origin.GetURL(),
            registry_controlled_domains::INCLUDE_UNKNOWN_REGISTRIES,
            registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES) == 0) {
      return HeaderEndpointGroupOutcome::
          DISCARDED_INCLUDE_SUBDOMAINS_NOT_ALLOWED;
    }

    parsed_endpoint_group_out->include_subdomains = OriginSubdomains::INCLUDE;
  }

  const base::ListValue* endpoint_list = nullptr;
  if (!dict->HasKey(kEndpointsKey))
    return HeaderEndpointGroupOutcome::DISCARDED_ENDPOINTS_MISSING;
  if (!dict->GetList(kEndpointsKey, &endpoint_list))
    return HeaderEndpointGroupOutcome::DISCARDED_ENDPOINTS_NOT_LIST;

  std::vector<ReportingEndpoint::EndpointInfo> endpoints;

  for (size_t i = 0; i < endpoint_list->GetSize(); i++) {
    const base::Value* endpoint = nullptr;
    bool got_endpoint = endpoint_list->Get(i, &endpoint);
    DCHECK(got_endpoint);

    ReportingEndpoint::EndpointInfo parsed_endpoint;

    HeaderEndpointOutcome outcome =
        ProcessEndpoint(delegate, origin, *endpoint, &parsed_endpoint);

    if (outcome == HeaderEndpointOutcome::SET)
      endpoints.push_back(std::move(parsed_endpoint));

    RecordHeaderEndpointOutcome(outcome);
  }

  // Remove the group if it is empty.
  if (endpoints.empty()) {
    cache->RemoveEndpointGroup(origin, group_name);
    return HeaderEndpointGroupOutcome::REMOVED_EMPTY;
  }

  parsed_endpoint_group_out->endpoints = std::move(endpoints);

  return HeaderEndpointGroupOutcome::PARSED;
}

}  // namespace

// static
const char ReportingHeaderParser::kHeaderOutcomeHistogram[] =
    "Net.Reporting.HeaderOutcome";

// static
const char ReportingHeaderParser::kHeaderEndpointGroupOutcomeHistogram[] =
    "Net.Reporting.HeaderEndpointGroupOutcome";

// static
const char ReportingHeaderParser::kHeaderEndpointOutcomeHistogram[] =
    "Net.Reporting.HeaderEndpointOutcome";

// static
void ReportingHeaderParser::RecordHeaderDiscardedForNoReportingService() {
  RecordHeaderOutcome(HeaderOutcome::DISCARDED_NO_REPORTING_SERVICE);
}

// static
void ReportingHeaderParser::RecordHeaderDiscardedForInvalidSSLInfo() {
  RecordHeaderOutcome(HeaderOutcome::DISCARDED_INVALID_SSL_INFO);
}

// static
void ReportingHeaderParser::RecordHeaderDiscardedForCertStatusError() {
  RecordHeaderOutcome(HeaderOutcome::DISCARDED_CERT_STATUS_ERROR);
}

// static
void ReportingHeaderParser::RecordHeaderDiscardedForJsonInvalid() {
  RecordHeaderOutcome(HeaderOutcome::DISCARDED_JSON_INVALID);
}

// static
void ReportingHeaderParser::RecordHeaderDiscardedForJsonTooBig() {
  RecordHeaderOutcome(HeaderOutcome::DISCARDED_JSON_TOO_BIG);
}

// static
void ReportingHeaderParser::ParseHeader(ReportingContext* context,
                                        const GURL& url,
                                        std::unique_ptr<base::Value> value) {
  DCHECK(url.SchemeIsCryptographic());

  const base::ListValue* group_list = nullptr;
  bool is_list = value->GetAsList(&group_list);
  DCHECK(is_list);

  ReportingDelegate* delegate = context->delegate();
  ReportingCache* cache = context->cache();

  url::Origin origin = url::Origin::Create(url);

  std::vector<ReportingEndpointGroup> parsed_header;

  for (size_t i = 0; i < group_list->GetSize(); i++) {
    const base::Value* group_value = nullptr;
    bool got_group = group_list->Get(i, &group_value);
    DCHECK(got_group);
    ReportingEndpointGroup parsed_endpoint_group;
    HeaderEndpointGroupOutcome outcome = ProcessEndpointGroup(
        delegate, cache, origin, *group_value, &parsed_endpoint_group);
    RecordHeaderEndpointGroupOutcome(outcome);
    if (outcome == HeaderEndpointGroupOutcome::PARSED)
      parsed_header.push_back(std::move(parsed_endpoint_group));
  }

  // Remove the client if it has no valid endpoint groups.
  if (parsed_header.empty()) {
    cache->RemoveClient(origin);
    RecordHeaderOutcome(HeaderOutcome::REMOVED_EMPTY);
    return;
  }

  cache->OnParsedHeader(origin, std::move(parsed_header));
  RecordHeaderOutcome(HeaderOutcome::PARSED);
}

}  // namespace net
