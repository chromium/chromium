// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_TCP_CONNECT_JOB_H_
#define NET_SOCKET_TCP_CONNECT_JOB_H_

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <variant>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/types/expected.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_export.h"
#include "net/base/network_anonymization_key.h"
#include "net/dns/host_resolver.h"
#include "net/dns/public/host_resolver_results.h"
#include "net/dns/public/resolve_error_info.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/socket/connect_job.h"
#include "net/socket/connection_attempts.h"
#include "net/socket/transport_connect_job.h"
#include "url/scheme_host_port.h"

namespace net {

class NetLogWithSource;
class SocketTag;

// TcpConnectJob is intended to replace TransportConnectJob.
//
// TcpConnectJob handles the host resolution necessary for TCP socket
// creation and the TCP connect. If provided with a protocol in addition to a
// scheme, it will wait for the relevant extra DNS records before completing
// (e.g., the HTTPS record for HTTP/HTTPS).
//
// It will start connecting as soon as it has partial DNS results.
//
// Since this class runs the DNS request in parallel with trying multiple
// connection attempts, it doesn't use a DoLoop() pattern often found in net.
// Instead, it has two primary methods that manage reacting to state updates:
// DoServiceEndpointsUpdated() and DoTryAdvanceWaitingConnectors(). Each of
// those methods returns a net::Error representing the progress of the
// TcpConnectJob as a whole, and so can be called either synchronously or
// asynchronously. If the caller invoking them is async, it's expected to call
// NotifyDelegateOfCompletion() itself.
class NET_EXPORT_PRIVATE TcpConnectJob
    : public ConnectJob,
      public HostResolver::ServiceEndpointRequest::Delegate {
 public:
  // In cases where both IPv6 and IPv4 addresses were returned from DNS,
  // TcpConnectJobs will start a second connection attempt to just
  // the IPv4 addresses after this much time. (This is "Happy Eyeballs".)
  //
  // TODO(willchan): Base this off RTT instead of statically setting it. Note we
  // choose a timeout that is different from the backup Connector timer so
  // they don't synchronize.
  static constexpr base::TimeDelta kIPv6FallbackTime = base::Milliseconds(300);

  struct NET_EXPORT_PRIVATE ServiceEndpointOverride {
    ServiceEndpointOverride(ServiceEndpoint endpoint,
                            std::set<std::string> dns_aliases);
    ServiceEndpointOverride(ServiceEndpointOverride&&);
    ServiceEndpointOverride(const ServiceEndpointOverride&);
    ~ServiceEndpointOverride();
    ServiceEndpointOverride& operator=(ServiceEndpointOverride&&) = default;
    ServiceEndpointOverride& operator=(const ServiceEndpointOverride&) =
        default;

    // This will only have one ServiceEndpoint, but it's a vector so that it can
    // easily be converted to a span.
    std::vector<ServiceEndpoint> endpoints;

    std::set<std::string> dns_aliases;
  };

  TcpConnectJob(RequestPriority priority,
                const SocketTag& socket_tag,
                const CommonConnectJobParams* common_connect_job_params,
                const scoped_refptr<TransportSocketParams>& params,
                ConnectJob::Delegate* delegate,
                const NetLogWithSource* net_log,
                std::optional<ServiceEndpointOverride>
                    endpoint_result_override = std::nullopt);

  TcpConnectJob(const TcpConnectJob&) = delete;
  TcpConnectJob& operator=(const TcpConnectJob&) = delete;

  ~TcpConnectJob() override;

  // ConnectJob methods:
  LoadState GetLoadState() const override;
  bool HasEstablishedConnection() const override;
  ConnectionAttempts GetConnectionAttempts() const override;
  ResolveErrorInfo GetResolveErrorInfo() const override;
  std::optional<HostResolverEndpointResult> GetHostResolverEndpointResult()
      const override;

  // Callers should use this instead of GetHostResolverEndpointResult(). May
  // only be called on success. Not a virtual ConnectJob method because no other
  // implementations need it, and don't want to accidentally use a proxy's
  // ServiceEndpoint at non-proxy ConnectJob layers.
  ServiceEndpoint GetServiceEndpoint() const;

  static base::TimeDelta ConnectionTimeout();

 private:
  // Connectors manage the actual connection attempts. They keep on pulling
  // IP addresses to try from the TcpConnectJob and trying to connect to them
  // until they either get a usable connection, or all addresses have been
  // tried.
  class Connector;

  // Type used yo provide Connectors with IPEndPoints.
  using IPEndPointInfo = base::expected<IPEndPoint, Error>;

  // Remaining ConnectJob methods:
  int ConnectInternal() override;
  void ChangePriorityInternal(RequestPriority priority) override;

  // One of the two principal methods running the the TcpConnectJob's state
  // machine. Returns either final error code of the TcpConnectJob if complete,
  // or ERR_IO_PENDING if not.
  //
  // Called both when the endpoints are updated, and when the endpoint request
  // finishes. `dns_request_final_result`, if provided, is the error code with
  // which the request finished.
  int DoServiceEndpointsUpdated(std::optional<int> dns_request_final_result);

  // One of the two principal methods running the the TcpConnectJob's state
  // machine.
  //
  // Tells Connectors to check if what DNS data they're waiting on is now
  // available. Returns either final error code of the entire TcpConnectJob if
  // complete, or ERR_IO_PENDING if not.
  //
  // Called when service endpoints are updated.
  int DoTryAdvanceWaitingConnectors();

  // HostResolver::ServiceEndpointRequest:
  void OnServiceEndpointsUpdated() override;
  void OnServiceEndpointRequestFinished(int rv) override;

  // Called back from a Connector when it completes. On failure, the last error
  // of `connection_attempts_` will be preferred over `result`, if there are any
  // errors there, since the error here will generally be ERR_NAME_NOT_RESOLVED
  // due to exhausting all provided IP addresses, rather than a connection
  // error.
  void OnConnectorComplete(int result);

  // Returns the next `IPEndPoint` that the Connector should connect to, and
  // logs that endpoint has been attempted. Never returns the same IPEndPoint
  // twice.
  //
  // Returns ERR_NAME_NOT_RESOLVED if there are no more IPEndPoints, and never
  // will be any more (i.e., the DNS request must be completed), ERR_IO_PENDING
  // if none are available yet. On any fatal DNS error, all work is cancelled,
  // so this shouldn't return other error values.
  IPEndPointInfo GetNextIPEndPoint();

  // Returns whether `result` is usable for this connection. If `svcb_optional`
  // is true, the non-HTTPS/SVCB fallback is allowed.
  bool IsEndpointResultUsable(const ServiceEndpoint& result) const;

  // Finds a usable ServiceEndpoint associated with an IPEndPoint, if there is
  // one. Prefers the higher priority ServiceEndPoints, so, e.g., if an IP
  // appears in both an A/AAAA record and an HTTPS record, but we received the
  // A/AAAA record first, this will return the HTTPS record as long as it's
  // considered usable, and has been received.
  const ServiceEndpoint* FindServiceEndpoint(
      const IPEndPoint& ip_endpoint) const;

  // Convenience wrapper for FindServiceEndpoint().
  bool IsIPEndPointUsable(const IPEndPoint& ip_endpoint) const {
    return FindServiceEndpoint(ip_endpoint) != nullptr;
  }

  // Updates `is_svcb_optional_`. Called whenever more ServiceEndpoints are
  // available.
  void UpdateSvcbOptional();

  // Sets `is_done_` to true, and destroys all connectors. Should only be called
  // when the entire TcpConnectJob is complete - either we've successfully
  // connected to an endpoint and ServiceEndpointRequest is crypto ready, or
  // we've given up on establishing a connection.
  //
  // On success, takes the connected socket from the Connector and calls
  // SetSocket(). On failure, returns the error from the last entry in
  // `connection_attempts_`. If the array is empty, adds an entry in
  // `connection_attempts_` with an empty IP and `result`, and then return
  // `result`. That can happen if there's a DNS error, or there are no usable
  // ServiceEndpoints.
  int SetDone(int result);

  // These wrap the corresponding methods in `dns_request_`. They pull results
  // from `endpoint_override_` instead, if populated.
  base::span<const ServiceEndpoint> GetEndpointResults() const;
  bool EndpointsCryptoReady() const;
  const std::set<std::string>& GetDnsAliasResults() const;

  // If `result` is not ERR_IO_PENDING, calls NotifyDelegateOfCompletion() with
  // the result. There are a lot of asynchronous callbacks in this class that
  // call one of the primary methods, and then need to call
  // NotifyDelegateOfCompletion() if the call completes the TcpConnectJob. This
  // small utility function makes those callsites a little simpler.
  //
  // The TcpConnectJob may be deleted synchronously by the owner when this
  // method is invoked.
  void NotifyDelegateIfDone(int result);

  const scoped_refptr<TransportSocketParams> params_;

  // If populated, will be used instead of making a new ServiceEndpointRequest,
  // and `dns_request_` will be nullptr.
  const std::optional<ServiceEndpointOverride> endpoint_override_;

  std::unique_ptr<HostResolver::ServiceEndpointRequest> dns_request_;
  bool dns_request_complete_ = false;

  std::unique_ptr<Connector> connector_;

  // Set/cleared on error connecting to IPv6/IPv4. Affects what type of IP is
  // preferred, if IPv6 and IPv4 IPs of equal priority are available.
  bool prefer_ipv6_ = true;

  ResolveErrorInfo resolve_error_info_;

  // This includes addresses that Connectors are currently attempting to connect
  // to. No address will ever br tried twice, even if it appears in multiple
  // ServiceEndpoints.
  std::set<IPEndPoint> attempted_addresses_;

  // IPs are only added to this list once connecting to them fails, so this is a
  // strict superset of `attempted_addresses_`, except when there are no usable
  // IPs, in which case an empty attempt is added. The final successful attempt
  // is not included, nor are any cancelled attempts. May include connection
  // errors from IPs that connection attempts were made to before learning they
  // were not usable.
  ConnectionAttempts connection_attempts_;

  // This is updated as DNS results come in, and then will be applied again to
  // the address of any connected socket before use, in case it changes.
  bool is_svcb_optional_ = true;

  // True once a connection has been established. Only relevant when waiting a
  // crypto ready, after establishing a connection. Per spec of
  // ConnectJob::HasEstablishedConnection(), once set to true, will not be
  // cleared, even if the connection is ultimately disabled.
  bool has_established_connection_ = false;

  // IP that was used, in the case of success.
  std::optional<IPEndPoint> final_address_;

  // Whether this is complete or not. Mostly serves a safety valve for async
  // calls that can't be cancelled coming in late, and to double-check that the
  // TcpConnectJob isn't completing twice.
  bool is_done_ = false;
};

}  // namespace net

#endif  // NET_SOCKET_TCP_CONNECT_JOB_H_
