// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_CANARY_DOMAIN_SERVICE_H_
#define NET_DNS_CANARY_DOMAIN_SERVICE_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/safe_ref.h"
#include "base/memory/weak_ptr.h"
#include "net/base/net_export.h"
#include "net/dns/host_resolver.h"
#include "net/dns/resolve_context.h"
#include "net/log/net_log_with_source.h"

namespace net {

class DnsResponse;
class DnsTransaction;
class DnsTransactionFactory;
class NetLogWithSource;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(CanaryDomainResult)
enum class CanaryDomainResult {
  kUnknownResult = 0,
  kPositive = 1,
  kNegativeNoErrorWithoutRecords = 2,
  kNegativeNxDomainOrOtherError = 3,
  kNegativeOtherError = 4,
  kMaxValue = kNegativeOtherError,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/net/enums.xml:CanaryDomainResult)

// Service for probing canary domains.  It is owned by a HostResolverManager.
class NET_EXPORT_PRIVATE CanaryDomainService
    : public ResolveContext::DohStatusObserver {
 public:
  // `resolve_context` is expected to be owned by `host_resolver`.
  // Both must outlive this object.
  // Also, the kProbeSecureDnsCanaryDomain feature must be enabled, with
  // a non-empty canary domain host specified.
  CanaryDomainService(base::SafeRef<ResolveContext> resolve_context,
                      base::SafeRef<HostResolver> host_resolver);
  ~CanaryDomainService() override;

  CanaryDomainService(const CanaryDomainService&) = delete;
  CanaryDomainService& operator=(const CanaryDomainService&) = delete;

  // Starts observing DoH status changes and probes the canary domain.
  // Should only be called once.
  void Start();

  // ResolveContext::DohStatusObserver implementation:
  void OnSessionChanged() override;
  void OnDohServerUnavailable(bool network_change) override;

  void SetOnProbeCompleteCallbackForTesting(base::OnceClosure callback);

 private:
  // Returns true if the canary domain should be probed. Otherwise, calls to
  // Start() and OnDohServerUnavailable() will be no-ops.
  bool ShouldProbe();

  // Probes a canary domain that reports whether Secure DNS (DoH) fallback is
  // allowed, and then runs `OnSecureDnsProbeComplete()` with the result.
  // Should cancel any previous probe that is still pending.
  void ProbeSecureDnsDomain();

  // Called with the result of a Secure DNS probe request. Sets the canary
  // domain status in ResolveContext.
  void OnSecureDnsProbeComplete(int net_error);

  // The HostResolver that this service uses to perform probes.
  // Must outlive this object.
  base::SafeRef<HostResolver> host_resolver_;

  // This context is used to observe DoH status changes and update the canary
  // domain check status.
  // Must outlive this object, and is expected to be owned by `host_resolver_`.
  base::SafeRef<ResolveContext> resolve_context_;

  // The canary domain host-port pair to probe.
  const HostPortPair canary_domain_host_;

  NetLogWithSource net_log_;

  std::unique_ptr<HostResolver::ResolveHostRequest> pending_probe_request_;

  base::OnceClosure on_probe_complete_callback_for_testing_;

  base::WeakPtrFactory<CanaryDomainService> weak_ptr_factory_{this};
};

}  // namespace net

#endif  // NET_DNS_CANARY_DOMAIN_SERVICE_H_
