// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_REPORTING_REPORTING_ENDPOINT_MANAGER_H_
#define NET_REPORTING_REPORTING_ENDPOINT_MANAGER_H_

#include <memory>

#include "net/base/net_export.h"
#include "net/base/rand_callback.h"
#include "net/reporting/reporting_endpoint.h"

class GURL;

namespace base {
class TickClock;
}

namespace net {

class NetworkAnonymizationKey;
class ReportingCache;
class ReportingDelegate;
struct ReportingEndpoint;
struct ReportingPolicy;

// Keeps track of which endpoints are pending (have active delivery attempts to
// them) or in exponential backoff after one or more failures, and chooses an
// endpoint from an endpoint group to receive reports for an origin.
class NET_EXPORT ReportingEndpointManager {
 public:
  // Maximum size of the backoff cache. This is deliberately much larger than is
  // likely necessary - the only goal is to prevent it from growing without
  // bound, while preventing repeatedly trying to upload data to down servrs.
  // Public for tests.
  static constexpr int kMaxEndpointBackoffCacheSize = 200;

  // The ReportingEndpointManager must not be used after any of the objects
  // passed to its constructor are destroyed.
  static std::unique_ptr<ReportingEndpointManager> Create(
      const ReportingPolicy* policy,
      const base::TickClock* tick_clock,
      const ReportingDelegate* delegate,
      ReportingCache* cache,
      const RandIntCallback& rand_callback);

  virtual ~ReportingEndpointManager();

  // Finds an endpoint that applies to deliveries to the group identified by
  // |group_key| that are not expired or in exponential backoff from failed
  // requests. The returned endpoint may have been configured by a superdomain
  // of the group's origin. Deliberately chooses an endpoint randomly to ensure
  // sites aren't relying on any sort of fallback ordering. If no suitable
  // endpoint was found, returns an endpoint with is_valid() false.
  virtual const ReportingEndpoint FindEndpointForDelivery(
      const ReportingEndpointGroupKey& group_key) = 0;

  // Informs the EndpointManager of a successful or unsuccessful request made to
  // |endpoint| so it can manage exponential backoff of failing endpoints.
  virtual void InformOfEndpointRequest(
      const NetworkAnonymizationKey& network_anonymization_key,
      const GURL& endpoint,
      bool succeeded) = 0;
};

}  // namespace net

#endif  // NET_REPORTING_REPORTING_ENDPOINT_MANAGER_H_
