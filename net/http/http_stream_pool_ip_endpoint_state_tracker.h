// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_STREAM_POOL_IP_ENDPOINT_STATE_TRACKER_H_
#define NET_HTTP_HTTP_STREAM_POOL_IP_ENDPOINT_STATE_TRACKER_H_

#include <stdint.h>

#include <map>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_export.h"
#include "net/dns/host_resolver.h"
#include "net/dns/public/host_resolver_results.h"
#include "net/http/http_stream_pool.h"

namespace net {

// Tracks IPEndPoint's attempt state, e.g. an IPEndPoint is slow attempting.
// Provides IPEndPoints for TCP based connection attempts. See the description
// of GetIPEndPointToAttemptTcpBased() for the current logic.
//
// TODO(crbug.com/383606724): The current logic relies on rather naive and not
// very well-founded heuristics. Write a design document and implement more
// appropriate algorithm to pick an IPEndPoint.
class HttpStreamPool::IPEndPointStateTracker {
 public:
  // The state of an IPEndPoint. There is no success state. The absence of a
  // state for an endpoint means that we haven't yet attempted to connect to the
  // endpoint, or that a connection to the endpoint was successfully completed
  // and was not slow. Public for testing.
  enum class IPEndPointState : uint8_t {
    // The endpoint has failed.
    kFailed = 0,
    // The endpoint is considered slow and haven't timed out yet.
    kSlowAttempting = 1,
    // The endpoint was slow to connect, but the connection establishment
    // completed successfully.
    kSlowSucceeded = 2,
  };
  using IPEndPointStateMap = std::map<IPEndPoint, IPEndPointState>;

  // An interface to abstract dependencies. Useful for testing.
  // TODO(crbug.com/383606724): Figure out better abstractions. Currently this
  // interface just exposes internal implementation of AttemptManager.
  class NET_EXPORT_PRIVATE Delegate {
   public:
    Delegate() = default;
    virtual ~Delegate() = default;

    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;

    // Returns the associated ServiceEndpointRequest.
    virtual HostResolver::ServiceEndpointRequest*
    GetServiceEndpointRequest() = 0;

    // Returns whether attempts is "SVCB-optional". See
    // https://www.rfc-editor.org/rfc/rfc9460.html#section-3-4
    // Note that the result can be changed over time while the DNS resolution is
    // still ongoing.
    virtual bool IsSvcbOptional() = 0;

    // Returns true when `endpoint` can be used to attempt TCP/TLS connections.
    virtual bool IsEndpointUsableForTcpBasedAttempt(
        const ServiceEndpoint& endpoint,
        bool svcb_optional) = 0;

    // Returns true when there are enough TcpBasedAttempts for `ip_endpoint`
    // that is slow.
    virtual bool HasEnoughTcpBasedAttemptsForSlowIPEndPoint(
        const IPEndPoint& ip_endpoint) = 0;
  };

  explicit IPEndPointStateTracker(Delegate* delegate);
  ~IPEndPointStateTracker();

  // Returns the current state of `ip_endpoint` if exists.
  std::optional<IPEndPointState> GetState(const IPEndPoint& ip_endpoint) const;

  // Called when `ip_endpoint` is slow, slow succeeded, or failed.
  void OnEndpointSlow(const IPEndPoint& ip_endpoint);
  void OnEndpointSlowSucceeded(const IPEndPoint& ip_endpoint);
  void OnEndpointFailed(const IPEndPoint& ip_endpoint);

  // Removes all slow attempting endpoints.
  void RemoveSlowAttemptingEndpoint();

  // Returns an IPEndPoint to attempt a connection.
  // Brief summary of the behavior is:
  //  * Try preferred address family first.
  //  * Prioritize unattempted or fast endpoints.
  //  * Fall back to slow but succeeded endpoints.
  //  * Use slow and attempting endpoints as the last option.
  //  * For a slow endpoint, skip the endpoint if there are enough attempts for
  //    the endpoint.
  std::optional<IPEndPoint> GetIPEndPointToAttemptTcpBased();

  base::Value::List GetInfoAsValue() const;

 private:
  void FindBetterIPEndPoint(const std::vector<IPEndPoint>& ip_endpoints,
                            std::optional<IPEndPointState>& current_state,
                            std::optional<IPEndPoint>& current_endpoint);

  const raw_ptr<Delegate> delegate_;

  // When true, try to use IPv6 for the next attempt first.
  bool prefer_ipv6_ = true;

  // Updated when a stream attempt is completed or considered slow. Used to
  // calculate next IPEndPoint to attempt.
  IPEndPointStateMap ip_endpoint_states_;
};

}  // namespace net

#endif  // NET_HTTP_HTTP_STREAM_POOL_IP_ENDPOINT_STATE_TRACKER_H_
