// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/reporting/reporting_endpoint.h"

#include <string>
#include <tuple>

#include "base/time/time.h"
#include "net/base/network_anonymization_key.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net {

ReportingEndpointGroupKey::ReportingEndpointGroupKey() = default;

ReportingEndpointGroupKey::ReportingEndpointGroupKey(
    const NetworkAnonymizationKey& network_anonymization_key,
    const url::Origin& origin,
    const std::string& group_name)
    : ReportingEndpointGroupKey(network_anonymization_key,
                                std::nullopt,
                                origin,
                                group_name) {}

ReportingEndpointGroupKey::ReportingEndpointGroupKey(
    const NetworkAnonymizationKey& network_anonymization_key,
    std::optional<base::UnguessableToken> reporting_source,
    const url::Origin& origin,
    const std::string& group_name)
    : network_anonymization_key(network_anonymization_key),
      reporting_source(std::move(reporting_source)),
      origin(origin),
      group_name(group_name) {
  // If |reporting_source| is present, it must not be empty.
  DCHECK(!(this->reporting_source.has_value() &&
           this->reporting_source->is_empty()));
}

ReportingEndpointGroupKey::ReportingEndpointGroupKey(
    const ReportingEndpointGroupKey& other,
    const std::optional<base::UnguessableToken>& reporting_source)
    : ReportingEndpointGroupKey(other.network_anonymization_key,
                                reporting_source,
                                other.origin,
                                other.group_name) {}

ReportingEndpointGroupKey::ReportingEndpointGroupKey(
    const ReportingEndpointGroupKey& other) = default;
ReportingEndpointGroupKey::ReportingEndpointGroupKey(
    ReportingEndpointGroupKey&& other) = default;

ReportingEndpointGroupKey& ReportingEndpointGroupKey::operator=(
    const ReportingEndpointGroupKey&) = default;
ReportingEndpointGroupKey& ReportingEndpointGroupKey::operator=(
    ReportingEndpointGroupKey&&) = default;

ReportingEndpointGroupKey::~ReportingEndpointGroupKey() = default;

bool operator==(const ReportingEndpointGroupKey& lhs,
                const ReportingEndpointGroupKey& rhs) {
  return std::tie(lhs.reporting_source, lhs.network_anonymization_key,
                  lhs.origin, lhs.group_name) ==
         std::tie(rhs.reporting_source, rhs.network_anonymization_key,
                  rhs.origin, rhs.group_name);
}

bool operator!=(const ReportingEndpointGroupKey& lhs,
                const ReportingEndpointGroupKey& rhs) {
  return !(lhs == rhs);
}

bool operator<(const ReportingEndpointGroupKey& lhs,
               const ReportingEndpointGroupKey& rhs) {
  return std::tie(lhs.reporting_source, lhs.network_anonymization_key,
                  lhs.origin, lhs.group_name) <
         std::tie(rhs.reporting_source, rhs.network_anonymization_key,
                  rhs.origin, rhs.group_name);
}

bool operator>(const ReportingEndpointGroupKey& lhs,
               const ReportingEndpointGroupKey& rhs) {
  return std::tie(lhs.reporting_source, lhs.network_anonymization_key,
                  lhs.origin, lhs.group_name) >
         std::tie(rhs.reporting_source, rhs.network_anonymization_key,
                  rhs.origin, rhs.group_name);
}

std::string ReportingEndpointGroupKey::ToString() const {
  return "Source: " +
         (reporting_source ? reporting_source->ToString() : "null") +
         "; NAK: " + network_anonymization_key.ToDebugString() +
         "; Origin: " + origin.Serialize() + "; Group name: " + group_name;
}

const int ReportingEndpoint::EndpointInfo::kDefaultPriority = 1;
const int ReportingEndpoint::EndpointInfo::kDefaultWeight = 1;

ReportingEndpoint::ReportingEndpoint() = default;

ReportingEndpoint::ReportingEndpoint(const ReportingEndpointGroupKey& group,
                                     const EndpointInfo& info)
    : group_key(group), info(info) {
  DCHECK_LE(0, info.weight);
  DCHECK_LE(0, info.priority);
}

ReportingEndpoint::ReportingEndpoint(const ReportingEndpoint& other) = default;
ReportingEndpoint::ReportingEndpoint(ReportingEndpoint&& other) = default;

ReportingEndpoint& ReportingEndpoint::operator=(const ReportingEndpoint&) =
    default;
ReportingEndpoint& ReportingEndpoint::operator=(ReportingEndpoint&&) = default;

ReportingEndpoint::~ReportingEndpoint() = default;

bool ReportingEndpoint::is_valid() const {
  return info.url.is_valid();
}

ReportingEndpointGroup::ReportingEndpointGroup() = default;

ReportingEndpointGroup::ReportingEndpointGroup(
    const ReportingEndpointGroup& other) = default;

ReportingEndpointGroup::~ReportingEndpointGroup() = default;

CachedReportingEndpointGroup::CachedReportingEndpointGroup(
    const ReportingEndpointGroupKey& group_key,
    OriginSubdomains include_subdomains,
    base::Time expires,
    base::Time last_used)
    : group_key(group_key),
      include_subdomains(include_subdomains),
      expires(expires),
      last_used(last_used) {}

CachedReportingEndpointGroup::CachedReportingEndpointGroup(
    const ReportingEndpointGroup& endpoint_group,
    base::Time now)
    : CachedReportingEndpointGroup(endpoint_group.group_key,
                                   endpoint_group.include_subdomains,
                                   now + endpoint_group.ttl /* expires */,
                                   now /* last_used */) {
  // Don't cache V1 document endpoints; this should only be used for V0
  // endpoint groups.
  DCHECK(!endpoint_group.group_key.IsDocumentEndpoint());
}

}  // namespace net
