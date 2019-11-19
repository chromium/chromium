// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_REPORTING_REPORTING_ENDPOINT_H_
#define NET_REPORTING_REPORTING_ENDPOINT_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/time/time.h"
#include "net/base/net_export.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net {

// Identifies an endpoint group.
struct NET_EXPORT ReportingEndpointGroupKey {
  ReportingEndpointGroupKey(url::Origin origin, std::string group_name);

  // Origin that configured this endpoint group.
  url::Origin origin;

  // Name of the endpoint group (defaults to "default" during header parsing).
  std::string group_name;
};

NET_EXPORT bool operator==(const ReportingEndpointGroupKey& lhs,
                           const ReportingEndpointGroupKey& rhs);
NET_EXPORT bool operator!=(const ReportingEndpointGroupKey& lhs,
                           const ReportingEndpointGroupKey& rhs);
NET_EXPORT bool operator<(const ReportingEndpointGroupKey& lhs,
                          const ReportingEndpointGroupKey& rhs);
NET_EXPORT bool operator>(const ReportingEndpointGroupKey& lhs,
                          const ReportingEndpointGroupKey& rhs);

// The configuration by an origin to use an endpoint for report delivery.
// TODO(crbug.com/921049): Rename to ReportingEndpoint because that's what it
// actually represents.
// TODO(crbug.com/912622): Track endpoint failures for garbage collection.
struct NET_EXPORT ReportingEndpoint {
  struct NET_EXPORT EndpointInfo {
    static const int kDefaultPriority;
    static const int kDefaultWeight;

    // The endpoint to which reports may be delivered. (Origins may configure
    // many.)
    GURL url;

    // Priority when multiple endpoints are configured for an origin; endpoints
    // with numerically lower priorities are used first.
    int priority = kDefaultPriority;

    // Weight when multiple endpoints are configured for an origin with the same
    // priority; among those with the same priority, each endpoint has a chance
    // of being chosen that is proportional to its weight.
    int weight = kDefaultWeight;
  };

  struct Statistics {
    // The number of attempted uploads that we've made for this endpoint.
    int attempted_uploads = 0;
    // The number of uploads that have succeeded for this endpoint.
    int successful_uploads = 0;
    // The number of individual reports that we've attempted to upload for this
    // endpoint.  (Failed uploads will cause a report to be counted multiple
    // times, once for each attempt.)
    int attempted_reports = 0;
    // The number of individual reports that we've successfully uploaded for
    // this endpoint.
    int successful_reports = 0;
  };

  // Constructs an invalid ReportingEndpoint.
  ReportingEndpoint();

  ReportingEndpoint(url::Origin origin,
                    std::string group_name,
                    EndpointInfo endpoint_info);

  ReportingEndpoint(const ReportingEndpoint& other);
  ReportingEndpoint(ReportingEndpoint&& other);

  ReportingEndpoint& operator=(const ReportingEndpoint&);
  ReportingEndpoint& operator=(ReportingEndpoint&&);

  ~ReportingEndpoint();

  bool is_valid() const;
  explicit operator bool() const { return is_valid(); }

  // Identifies the endpoint group to which this endpoint belongs.
  ReportingEndpointGroupKey group_key;

  // URL, priority, and weight of the endpoint.
  EndpointInfo info;

  // Information about the number of deliveries that we've attempted for this
  // endpoint. Not persisted across restarts.
  Statistics stats;
};

// Marks whether a given endpoint group is configured to include its origin's
// subdomains.
enum class OriginSubdomains { EXCLUDE, INCLUDE, DEFAULT = EXCLUDE };

// Represents an endpoint group set by an origin via Report-To header.
struct NET_EXPORT ReportingEndpointGroup {
  ReportingEndpointGroup();

  ReportingEndpointGroup(const ReportingEndpointGroup& other);

  ~ReportingEndpointGroup();

  // Group name.
  std::string name;

  // Whether this group applies to subdomains of its origin.
  OriginSubdomains include_subdomains = OriginSubdomains::DEFAULT;

  // Time for which the endpoint group remains valid after it is set.
  base::TimeDelta ttl;

  // Endpoints in this group.
  std::vector<ReportingEndpoint::EndpointInfo> endpoints;
};

// Representation of an endpoint group used for in-memory and persistent
// storage.
struct NET_EXPORT CachedReportingEndpointGroup {
  CachedReportingEndpointGroup(url::Origin origin,
                               std::string name,
                               OriginSubdomains include_subdomains,
                               base::Time expires,
                               base::Time last_used);

  // |origin| is the origin that set |endpoint_group|, |now| is the time at
  // which the header was processed.
  CachedReportingEndpointGroup(url::Origin origin,
                               const ReportingEndpointGroup& endpoint_group,
                               base::Time now);

  // Origin and group name.
  ReportingEndpointGroupKey group_key;

  // Whether this group applies to subdomains of |group_key.origin|.
  OriginSubdomains include_subdomains = OriginSubdomains::DEFAULT;

  // When this group's max_age expires.
  // (base::Time is used here instead of base::TimeTicks for ease of
  // serialization for persistent storage, and because it is more appropriate
  // for expiration times, as per //base/time/time.h.)
  base::Time expires;

  // Last time that this group was accessed for a delivery or updated via a
  // new header.
  base::Time last_used;
};

}  // namespace net

#endif  // NET_REPORTING_REPORTING_ENDPOINT_H_
