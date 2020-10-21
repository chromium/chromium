// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/reporting/reporting_header_parser.h"

#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/check.h"
#include "base/json/json_reader.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "base/values.h"
#include "net/base/network_isolation_key.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/reporting/reporting_cache.h"
#include "net/reporting/reporting_context.h"
#include "net/reporting/reporting_delegate.h"
#include "net/reporting/reporting_endpoint.h"

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

// Processes a single endpoint tuple received in a Report-To header.
//
// |origin| is the origin that sent the Report-To header.
//
// |value| is the parsed JSON value of the endpoint tuple.
//
// |*endpoint_out| will contain the endpoint URL parsed out of the tuple.
// Returns true on success or false if endpoint was discarded.
bool ProcessEndpoint(ReportingDelegate* delegate,
                     const ReportingEndpointGroupKey& group_key,
                     const base::Value& value,
                     ReportingEndpoint::EndpointInfo* endpoint_info_out) {
  const base::DictionaryValue* dict = nullptr;
  if (!value.GetAsDictionary(&dict))
    return false;
  DCHECK(dict);

  std::string endpoint_url_string;
  if (!dict->HasKey(kUrlKey))
    return false;
  if (!dict->GetString(kUrlKey, &endpoint_url_string))
    return false;

  GURL endpoint_url;
  // Support path-absolute-URL string
  if (std::strspn(endpoint_url_string.c_str(), "/") == 1) {
    endpoint_url = group_key.origin.GetURL().Resolve(endpoint_url_string);
  } else {
    endpoint_url = GURL(endpoint_url_string);
  }
  if (!endpoint_url.is_valid())
    return false;
  if (!endpoint_url.SchemeIsCryptographic())
    return false;
  endpoint_info_out->url = std::move(endpoint_url);

  int priority = ReportingEndpoint::EndpointInfo::kDefaultPriority;
  if (dict->HasKey(kPriorityKey) && !dict->GetInteger(kPriorityKey, &priority))
    return false;
  if (priority < 0)
    return false;
  endpoint_info_out->priority = priority;

  int weight = ReportingEndpoint::EndpointInfo::kDefaultWeight;
  if (dict->HasKey(kWeightKey) && !dict->GetInteger(kWeightKey, &weight))
    return false;
  if (weight < 0)
    return false;
  endpoint_info_out->weight = weight;

  return delegate->CanSetClient(group_key.origin, endpoint_info_out->url);
}

// Processes a single endpoint group tuple received in a Report-To header.
//
// |origin| is the origin that sent the Report-To header.
//
// |value| is the parsed JSON value of the endpoint group tuple.
// Returns true on successfully adding a non-empty group, or false if endpoint
// group was discarded or processed as a deletion.
bool ProcessEndpointGroup(ReportingDelegate* delegate,
                          ReportingCache* cache,
                          const NetworkIsolationKey& network_isolation_key,
                          const url::Origin& origin,
                          const base::Value& value,
                          ReportingEndpointGroup* parsed_endpoint_group_out) {
  const base::DictionaryValue* dict = nullptr;
  if (!value.GetAsDictionary(&dict))
    return false;
  DCHECK(dict);

  std::string group_name = kDefaultGroupName;
  if (dict->HasKey(kGroupKey) && !dict->GetString(kGroupKey, &group_name))
    return false;
  ReportingEndpointGroupKey group_key(network_isolation_key, origin,
                                      group_name);
  parsed_endpoint_group_out->group_key = group_key;

  int ttl_sec = -1;
  if (!dict->HasKey(kMaxAgeKey))
    return false;
  if (!dict->GetInteger(kMaxAgeKey, &ttl_sec))
    return false;
  if (ttl_sec < 0)
    return false;
  // max_age: 0 signifies removal of the endpoint group.
  if (ttl_sec == 0) {
    cache->RemoveEndpointGroup(group_key);
    return false;
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
      return false;
    }

    parsed_endpoint_group_out->include_subdomains = OriginSubdomains::INCLUDE;
  }

  const base::ListValue* endpoint_list = nullptr;
  if (!dict->HasKey(kEndpointsKey))
    return false;
  if (!dict->GetList(kEndpointsKey, &endpoint_list))
    return false;

  std::vector<ReportingEndpoint::EndpointInfo> endpoints;

  for (size_t i = 0; i < endpoint_list->GetSize(); i++) {
    const base::Value* endpoint = nullptr;
    bool got_endpoint = endpoint_list->Get(i, &endpoint);
    DCHECK(got_endpoint);

    ReportingEndpoint::EndpointInfo parsed_endpoint;

    if (ProcessEndpoint(delegate, group_key, *endpoint, &parsed_endpoint))
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

}  // namespace

// static
void ReportingHeaderParser::ParseHeader(
    ReportingContext* context,
    const NetworkIsolationKey& network_isolation_key,
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
    if (ProcessEndpointGroup(delegate, cache, network_isolation_key, origin,
                             *group_value, &parsed_endpoint_group)) {
      parsed_header.push_back(std::move(parsed_endpoint_group));
    }
  }

  // Remove the client if it has no valid endpoint groups.
  if (parsed_header.empty()) {
    cache->RemoveClient(network_isolation_key, origin);
    return;
  }

  cache->OnParsedHeader(network_isolation_key, origin,
                        std::move(parsed_header));
}

}  // namespace net
