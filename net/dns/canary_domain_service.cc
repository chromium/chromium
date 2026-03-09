// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/canary_domain_service.h"

#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "net/base/features.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/base/network_anonymization_key.h"
#include "net/dns/dns_response.h"
#include "net/dns/dns_transaction.h"
#include "net/dns/public/dns_protocol.h"
#include "net/dns/public/dns_query_type.h"
#include "net/dns/public/host_resolver_source.h"
#include "net/dns/public/secure_dns_mode.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/log/net_log_with_source.h"

namespace net {

namespace {

HostResolver::ResolveHostParameters CreateResolveHostParameters() {
  // A canary domain check should not use the cache, and it should not allow
  // Secure DNS. Since canary domain check is not related to user navigation,
  // and is a probe of the system DNS provider, DoH is not needed, even when
  // supported. Also, for example, in the case of a canary domain check for
  // whether to allow DoH, then an insecure check is required.
  HostResolver::ResolveHostParameters params;
  params.cache_usage =
      HostResolver::ResolveHostParameters::CacheUsage::DISALLOWED;
  params.secure_dns_policy = SecureDnsPolicy::kDisable;
  return params;
}

// Note: HostResolver::SquashErrorCode() squashes most errors into
// ERR_NAME_NOT_RESOLVED, so it's not possible to disambiguate NXDOMAIN from
// other errors, such as SERVFAIL.
CanaryDomainResult GetCanaryDomainResult(
    const HostResolver::ResolveHostRequest& request,
    int net_error) {
  switch (net_error) {
    case OK:  // NOERROR
      // Return negative if the response contains no A or AAAA records.
      if (request.GetAddressResults().endpoints().empty()) {
        return CanaryDomainResult::kNegativeNoErrorWithoutRecords;
      }
      return CanaryDomainResult::kPositive;
    case ERR_NAME_NOT_RESOLVED:
      return CanaryDomainResult::kNegativeNxDomainOrOtherError;
    default:
      return CanaryDomainResult::kNegativeOtherError;
  }
}

}  // namespace

CanaryDomainService::CanaryDomainService(
    base::SafeRef<ResolveContext> resolve_context,
    base::SafeRef<HostResolver> host_resolver)
    : host_resolver_(std::move(host_resolver)),
      resolve_context_(std::move(resolve_context)),
      canary_domain_host_(features::kSecureDnsCanaryDomainHost.Get(), 0),
      net_log_(
          NetLogWithSource::Make(NetLog::Get(),
                                 NetLogSourceType::CANARY_DOMAIN_SERVICE)) {
  resolve_context_->set_doh_fallback_canary_domain_check_status(
      CanaryDomainCheckStatus::kNotStarted);
}

CanaryDomainService::~CanaryDomainService() {
  resolve_context_->UnregisterDohStatusObserver(this);
}

void CanaryDomainService::Start() {
  resolve_context_->RegisterDohStatusObserver(this);
  ProbeSecureDnsDomain();
}

// Called when the current session has changed. This should reset back to
// the initial state of the canary domain check not having started yet.
void CanaryDomainService::OnSessionChanged() {
  // Reset any previous result. Secure DNS to the fallback provider should only
  // be allowed if a new probe is positive.
  pending_probe_request_.reset();

  // Cancel any asynchronous ProbeSecureDnsDomain() calls.
  weak_ptr_factory_.InvalidateWeakPtrs();

  resolve_context_->set_doh_fallback_canary_domain_check_status(
      CanaryDomainCheckStatus::kNotStarted);
}

// Called when a DoH server has been marked as unavailable and is ready for
// availability probes. This should trigger a new probe of the canary domain.
void CanaryDomainService::OnDohServerUnavailable(bool network_change) {
  // Start the probe asynchronously, as this may trigger reentrant calls into
  // HostResolverManager, which are not allowed during notification handling.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&CanaryDomainService::ProbeSecureDnsDomain,
                                weak_ptr_factory_.GetWeakPtr()));
}

bool CanaryDomainService::ShouldProbe() {
  // If DoH fallback is not enabled, then there is no need to check the canary
  // domain.
  if (!resolve_context_->IsDohFallbackProbeEnabled()) {
    return false;
  }

  // Now check whether canary domain probing is enabled.
  return base::FeatureList::IsEnabled(features::kProbeSecureDnsCanaryDomain) &&
      !canary_domain_host_.host().empty();
}

void CanaryDomainService::ProbeSecureDnsDomain() {
  if (!ShouldProbe()) {
    if (on_probe_complete_callback_for_testing_) {
      std::move(on_probe_complete_callback_for_testing_).Run();
    }
    return;
  }

  resolve_context_->set_doh_fallback_canary_domain_check_status(
      CanaryDomainCheckStatus::kStarted);

  pending_probe_request_ = host_resolver_->CreateRequest(
      canary_domain_host_, NetworkAnonymizationKey::CreateTransient(), net_log_,
      CreateResolveHostParameters());

  int result = pending_probe_request_->Start(
      base::BindOnce(&CanaryDomainService::OnSecureDnsProbeComplete,
                     weak_ptr_factory_.GetWeakPtr()));

  if (result != ERR_IO_PENDING) {
    OnSecureDnsProbeComplete(result);
  }
}

void CanaryDomainService::OnSecureDnsProbeComplete(int net_error) {
  // pending_probe_request_ should be non-null, since if it is destroyed,
  // this callback is not called.
  CanaryDomainResult result =
      GetCanaryDomainResult(*pending_probe_request_.get(), net_error);

  resolve_context_->set_doh_fallback_canary_domain_check_status(
      result == CanaryDomainResult::kPositive
          ? CanaryDomainCheckStatus::kPositive
          : CanaryDomainCheckStatus::kNegative);

  base::UmaHistogramEnumeration("Net.DNS.CanaryDomainService.SecureDns.Result",
                                result);

  if (on_probe_complete_callback_for_testing_) {
    std::move(on_probe_complete_callback_for_testing_).Run();
  }
}

void CanaryDomainService::SetOnProbeCompleteCallbackForTesting(
    base::OnceClosure callback) {
  CHECK(on_probe_complete_callback_for_testing_.is_null());
  on_probe_complete_callback_for_testing_ = std::move(callback);
}

}  // namespace net
