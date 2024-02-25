// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_TRANSPORT_CONNECT_JOB_H_
#define NET_SOCKET_TRANSPORT_CONNECT_JOB_H_

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_export.h"
#include "net/base/network_anonymization_key.h"
#include "net/dns/host_resolver.h"
#include "net/dns/public/host_resolver_results.h"
#include "net/dns/public/resolve_error_info.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/socket/connect_job.h"
#include "net/socket/connection_attempts.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/scheme_host_port.h"

namespace net {

class NetLogWithSource;
class SocketTag;
class TransportConnectSubJob;

class NET_EXPORT_PRIVATE TransportSocketParams
    : public base::RefCounted<TransportSocketParams> {
 public:
  // Representation of the destination endpoint of the transport
  // socket/connection. Unlike ConnectJobFactory::Endpoint, this does not have a
  // `using_ssl` field for schemeless endpoints because that has no meaning for
  // transport parameters.
  using Endpoint = absl::variant<url::SchemeHostPort, HostPortPair>;

  // `host_resolution_callback` will be invoked after the the hostname is
  // resolved. `network_anonymization_key` is passed to the HostResolver to
  // prevent cross-NAK leaks. If `host_resolution_callback` does not return OK,
  // then the connection will be aborted with that value. `supported_alpns`
  // specifies ALPN protocols for selecting HTTPS/SVCB records. If empty,
  // addresses from HTTPS/SVCB records will be ignored and only A/AAAA will be
  // used.
  TransportSocketParams(Endpoint destination,
                        NetworkAnonymizationKey network_anonymization_key,
                        SecureDnsPolicy secure_dns_policy,
                        OnHostResolutionCallback host_resolution_callback,
                        base::flat_set<std::string> supported_alpns);

  TransportSocketParams(const TransportSocketParams&) = delete;
  TransportSocketParams& operator=(const TransportSocketParams&) = delete;

  const Endpoint& destination() const { return destination_; }
  const NetworkAnonymizationKey& network_anonymization_key() const {
    return network_anonymization_key_;
  }
  SecureDnsPolicy secure_dns_policy() const { return secure_dns_policy_; }
  const OnHostResolutionCallback& host_resolution_callback() const {
    return host_resolution_callback_;
  }
  const base::flat_set<std::string>& supported_alpns() const {
    return supported_alpns_;
  }

 private:
  friend class base::RefCounted<TransportSocketParams>;
  ~TransportSocketParams();

  const Endpoint destination_;
  const NetworkAnonymizationKey network_anonymization_key_;
  const SecureDnsPolicy secure_dns_policy_;
  const OnHostResolutionCallback host_resolution_callback_;
  const base::flat_set<std::string> supported_alpns_;
};

// TransportConnectJob handles the host resolution necessary for socket creation
// and the transport (likely TCP) connect. TransportConnectJob also has fallback
// logic for IPv6 connect() timeouts (which may happen due to networks / routers
// with broken IPv6 support). Those timeouts take 20s, so rather than make the
// user wait 20s for the timeout to fire, we use a fallback timer
// (kIPv6FallbackTime) and start a connect() to a IPv4 address if the timer
// fires. Then we race the IPv4 connect() against the IPv6 connect() (which has
// a headstart) and return the one that completes first to the socket pool.
class NET_EXPORT_PRIVATE TransportConnectJob : public ConnectJob {
 public:
  class NET_EXPORT_PRIVATE Factory {
   public:
    Factory() = default;
    virtual ~Factory() = default;

    virtual std::unique_ptr<TransportConnectJob> Create(
        RequestPriority priority,
        const SocketTag& socket_tag,
        const CommonConnectJobParams* common_connect_job_params,
        const scoped_refptr<TransportSocketParams>& params,
        Delegate* delegate,
        const NetLogWithSource* net_log);
  };

  // In cases where both IPv6 and IPv4 addresses were returned from DNS,
  // TransportConnectJobs will start a second connection attempt to just the
  // IPv4 addresses after this much time. (This is "Happy Eyeballs".)
  //
  // TODO(willchan): Base this off RTT instead of statically setting it. Note we
  // choose a timeout that is different from the backup connect job timer so
  // they don't synchronize.
  static constexpr base::TimeDelta kIPv6FallbackTime = base::Milliseconds(300);

  struct NET_EXPORT_PRIVATE EndpointResultOverride {
    EndpointResultOverride(HostResolverEndpointResult result,
                           std::set<std::string> dns_aliases);
    EndpointResultOverride(EndpointResultOverride&&);
    EndpointResultOverride(const EndpointResultOverride&);
    ~EndpointResultOverride();
    EndpointResultOverride& operator=(EndpointResultOverride&&) = default;
    EndpointResultOverride& operator=(const EndpointResultOverride&) = default;

    HostResolverEndpointResult result;
    std::set<std::string> dns_aliases;
  };

  TransportConnectJob(RequestPriority priority,
                      const SocketTag& socket_tag,
                      const CommonConnectJobParams* common_connect_job_params,
                      const scoped_refptr<TransportSocketParams>& params,
                      Delegate* delegate,
                      const NetLogWithSource* net_log,
                      std::optional<EndpointResultOverride>
                          endpoint_result_override = std::nullopt);

  TransportConnectJob(const TransportConnectJob&) = delete;
  TransportConnectJob& operator=(const TransportConnectJob&) = delete;

  ~TransportConnectJob() override;

  // ConnectJob methods.
  LoadState GetLoadState() const override;
  bool HasEstablishedConnection() const override;
  ConnectionAttempts GetConnectionAttempts() const override;
  ResolveErrorInfo GetResolveErrorInfo() const override;
  std::optional<HostResolverEndpointResult> GetHostResolverEndpointResult()
      const override;

  static base::TimeDelta ConnectionTimeout();

 private:
  friend class TransportConnectSubJob;

  enum State {
    STATE_RESOLVE_HOST,
    STATE_RESOLVE_HOST_COMPLETE,
    STATE_RESOLVE_HOST_CALLBACK_COMPLETE,
    STATE_TRANSPORT_CONNECT,
    STATE_TRANSPORT_CONNECT_COMPLETE,
    STATE_NONE,
  };

  // Although it is not strictly necessary, it makes the code simpler if each
  // subjob knows what type it is.
  enum SubJobType { SUB_JOB_IPV4, SUB_JOB_IPV6 };

  void OnIOComplete(int result);
  int DoLoop(int result);

  int DoResolveHost();
  int DoResolveHostComplete(int result);
  int DoResolveHostCallbackComplete();
  int DoTransportConnect();
  int DoTransportConnectComplete(int result);

  // Helper method called called when a SubJob completes, synchronously
  // or asynchronously. Returns `ERR_IO_PENDING` if there is more work to
  // do and another error if completed. It's up to the caller to manage
  // advancing `DoLoop` if a value other than `ERR_IO_PENDING` is returned.
  int HandleSubJobComplete(int result, TransportConnectSubJob* job);
  // Called back from a SubJob when it completes. Invokes `OnIOComplete`,
  // re-entering `DoLoop`, if there is no more work to do. Must not
  // be called from within `DoLoop`.
  void OnSubJobComplete(int result, TransportConnectSubJob* job);

  // Called from |fallback_timer_|.
  void StartIPv4JobAsync();

  // Begins the host resolution and the TCP connect.  Returns OK on success
  // and ERR_IO_PENDING if it cannot immediately service the request.
  // Otherwise, it returns a net error code.
  int ConnectInternal() override;

  void ChangePriorityInternal(RequestPriority priority) override;

  // Returns whether the client should be SVCB-optional when connecting to
  // `results`.
  bool IsSvcbOptional(
      base::span<const HostResolverEndpointResult> results) const;

  // Returns whether `result` is usable for this connection. If `svcb_optional`
  // is true, the non-HTTPS/SVCB fallback is allowed.
  bool IsEndpointResultUsable(const HostResolverEndpointResult& result,
                              bool svcb_optional) const;

  // Returns the `HostResolverEndpointResult` for the current subjobs.
  const HostResolverEndpointResult& GetEndpointResultForCurrentSubJobs() const;

  scoped_refptr<TransportSocketParams> params_;
  std::unique_ptr<HostResolver::ResolveHostRequest> request_;
  std::vector<HostResolverEndpointResult> endpoint_results_;
  size_t current_endpoint_result_ = 0;
  std::set<std::string> dns_aliases_;
  bool has_dns_override_ = false;

  State next_state_ = STATE_NONE;

  // The addresses are divided into IPv4 and IPv6, which are performed partially
  // in parallel. If the list of IPv6 addresses is non-empty, then the IPv6 jobs
  // go first, followed after `kIPv6FallbackTime` by the IPv4 addresses. The
  // first sub-job to establish a connection wins. If one sub-job fails, the
  // other one is launched if needed, and we wait for it to complete.
  std::unique_ptr<TransportConnectSubJob> ipv4_job_;
  std::unique_ptr<TransportConnectSubJob> ipv6_job_;

  base::OneShotTimer fallback_timer_;

  ResolveErrorInfo resolve_error_info_;
  ConnectionAttempts connection_attempts_;

  base::WeakPtrFactory<TransportConnectJob> weak_ptr_factory_{this};
};

}  // namespace net

#endif  // NET_SOCKET_TRANSPORT_CONNECT_JOB_H_
