// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy_resolution/configured_proxy_resolution_request.h"

#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "net/base/net_errors.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_source_type.h"
#include "net/proxy_resolution/configured_proxy_resolution_service.h"
#include "net/proxy_resolution/proxy_info.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

namespace net {

namespace {

bool CheckDnsCondition(
    const ProxyConfig::ProxyOverrideRule::DnsProbeCondition& dns_condition,
    const ResolveHostResult& dns_result) {
  switch (dns_condition.result) {
    case ProxyConfig::ProxyOverrideRule::DnsProbeCondition::Result::kNotFound:
      return dns_result.is_address_list_empty;
    case ProxyConfig::ProxyOverrideRule::DnsProbeCondition::Result::kResolved:
      return !dns_result.is_address_list_empty;
  }
}

base::Value::Dict CreateEndHostResolutionNetLogParams(
    const url::SchemeHostPort& host,
    const ResolveHostResult& result,
    bool was_resolved_sync) {
  base::Value::Dict dict;
  dict.Set("host", host.Serialize());
  dict.Set("was_resolved_sync", was_resolved_sync);
  result.AddToDict(dict);
  return dict;
}

}  // namespace

ConfiguredProxyResolutionRequest::ConfiguredProxyResolutionRequest(
    ConfiguredProxyResolutionService* service,
    const GURL& url,
    const std::string& method,
    const NetworkAnonymizationKey& network_anonymization_key,
    ProxyInfo* results,
    CompletionOnceCallback user_callback,
    const NetLogWithSource& net_log,
    RequestPriority priority)
    : service_(service),
      user_callback_(std::move(user_callback)),
      results_(results),
      url_(url),
      method_(method),
      network_anonymization_key_(network_anonymization_key),
      net_log_(net_log),
      priority_(priority),
      creation_time_(base::TimeTicks::Now()) {
  CHECK(!user_callback_.is_null());
}

ConfiguredProxyResolutionRequest::~ConfiguredProxyResolutionRequest() {
  if (service_) {
    service_->RemovePendingRequest(this);
    Cancel();

    // This should be emitted last, after any message `Cancel()` may
    // trigger.
    net_log_.EndEvent(NetLogEventType::PROXY_RESOLUTION_SERVICE);
  }
}

// Starts the resolve proxy request.
int ConfiguredProxyResolutionRequest::Start() {
  CHECK(!was_completed());
  CHECK(!is_started());

  CHECK(service_->config_);
  traffic_annotation_ = MutableNetworkTrafficAnnotationTag(
      service_->config_->traffic_annotation());

  if (!service_->config_->value().proxy_override_rules().empty()) {
    net_log_.BeginEvent(NetLogEventType::PROXY_RESOLUTION_OVERRIDE_RULES);

    for (const auto& rule : service_->config_->value().proxy_override_rules()) {
      if (rule.MatchesDestination(url_)) {
        bool skip_rule = false;
        for (const auto& dns_condition : rule.dns_conditions) {
          // DNS resolution requests are keyed by the host and the network
          // anonymization key, but not by listeners. This means that a given
          // request may end up as listener multiple times for the same DNS host
          // resolution requests (e.g. if the same host is in multiple rules).
          // This is fine, as long as `OnDnsHostResolved` knows how to handle
          // multiple calls for the same host.
          auto result = service_->RequestHostResolution(
              dns_condition, weak_factory_.GetWeakPtr(),
              network_anonymization_key_, net_log_, priority_);
          if (result) {
            net_log_.AddEvent(
                NetLogEventType::PROXY_OVERRIDE_END_HOST_RESOLUTION, [&] {
                  return CreateEndHostResolutionNetLogParams(
                      dns_condition.host, result.value(),
                      /*was_resolved_sync=*/true);
                });

            if (!CheckDnsCondition(dns_condition, result.value())) {
              // No need to trigger DNS resolutions for the remaining hosts
              // in this rule, as it does not apply.
              skip_rule = true;
              break;
            }

            dns_results_.emplace(dns_condition.host, result.value());
          }
        }
        if (!skip_rule) {
          applicable_override_rules_.push(rule);
        }
      }
    }

    if (applicable_override_rules_.empty()) {
      // No override rules applied.
      net_log_.EndEvent(NetLogEventType::PROXY_RESOLUTION_OVERRIDE_RULES);
    }
  }

  return ContinueProxyResolution();
}

void ConfiguredProxyResolutionRequest::
    StartAndCompleteCheckingForSynchronous() {
  int rv = service_->TryToCompleteSynchronously(
      url_, /*bypass_override_rules=*/false, net_log_, results_);
  if (rv == ERR_IO_PENDING) {
    rv = Start();
  }

  if (rv != ERR_IO_PENDING) {
    QueryComplete(rv);
  }
}

void ConfiguredProxyResolutionRequest::Cancel() {
  net_log_.AddEvent(NetLogEventType::CANCELLED);

  if (!applicable_override_rules_.empty()) {
    net_log_.EndEvent(NetLogEventType::PROXY_RESOLUTION_OVERRIDE_RULES);
  }

  Reset();
  CHECK(!is_started());
}

int ConfiguredProxyResolutionRequest::QueryDidComplete(int result_code) {
  CHECK(!was_completed());

  // Clear state so that `is_started()` returns false while
  // DidFinishResolvingProxy() runs.
  if (is_started()) {
    Reset();
  }

  // Note that DidFinishResolvingProxy might modify `results_`.
  int rv = service_->DidFinishResolvingProxy(url_, network_anonymization_key_,
                                             method_, results_, result_code,
                                             net_log_);

  // Make a note in the results which configuration was in use at the
  // time of the resolve.
  results_->set_proxy_resolve_start_time(creation_time_);
  results_->set_proxy_resolve_end_time(base::TimeTicks::Now());

  // If annotation is not already set, e.g. through TryToCompleteSynchronously
  // function, use in-progress-resolve annotation.
  if (!results_->traffic_annotation().is_valid())
    results_->set_traffic_annotation(traffic_annotation_);

  // If proxy is set without error, ensure that an annotation is provided.
  if (result_code != ERR_ABORTED && !rv) {
    // TODO(crbug.com/469006851): Turn this back into a CHECK once crashes were
    // fully fixed.
    DCHECK(results_->traffic_annotation().is_valid());
  }

  // Reset the state associated with in-progress-resolve.
  traffic_annotation_.reset();

  return rv;
}

int ConfiguredProxyResolutionRequest::QueryDidCompleteSynchronously(
    int result_code) {
  int rv = QueryDidComplete(result_code);
  service_ = nullptr;
  return rv;
}

LoadState ConfiguredProxyResolutionRequest::GetLoadState() const {
  LoadState load_state = LOAD_STATE_IDLE;
  if (service_ && service_->GetLoadStateIfAvailable(&load_state))
    return load_state;

  if (is_started())
    return resolve_job_->GetLoadState();
  return LOAD_STATE_RESOLVING_PROXY_FOR_URL;
}

// Callback for when the ProxyResolver request has completed.
void ConfiguredProxyResolutionRequest::QueryComplete(int result_code) {
  result_code = QueryDidComplete(result_code);

  CompletionOnceCallback callback = std::move(user_callback_);

  service_->RemovePendingRequest(this);
  service_ = nullptr;
  user_callback_.Reset();
  std::move(callback).Run(result_code);
}

int ConfiguredProxyResolutionRequest::ContinueProxyResolution() {
  if (!applicable_override_rules_.empty()) {
    int rv = EvaluateApplicableOverrideRules();
    if (rv == ERR_IO_PENDING || !results_->is_empty()) {
      return rv;
    }
  }

  if (service_->ApplyPacBypassRules(url_, results_)) {
    return OK;
  }

  // Required in case some override rules needed to run asynchronously, but
  // ended up not applying, and there are e.g. manual proxy settings as
  // fallback.
  int rv = service_->TryToCompleteSynchronously(
      url_, /*bypass_override_rules=*/true, net_log_, results_);
  if (rv != ERR_IO_PENDING) {
    return rv;
  }

  return service_->GetProxyResolver()->GetProxyForURL(
      url_, network_anonymization_key_, results_,
      base::BindOnce(&ConfiguredProxyResolutionRequest::QueryComplete,
                     base::Unretained(this)),
      &resolve_job_, net_log_);
}

void ConfiguredProxyResolutionRequest::OnDnsHostResolved(
    const url::SchemeHostPort& host,
    const ResolveHostResult& result) {
  if (applicable_override_rules_.empty()) {
    // DNS resolution is only required for override rules. Ignore these function
    // calls if the current request is not resolving override rules.
    return;
  }

  // Using `try_emplace` as this line may be hit multiple times for the same
  // host (see comment in `Start`).
  auto [unused, inserted] = dns_results_.try_emplace(host, result);
  // Entry already existed, likely with the same value as before. Same value or
  // not, we keep the old result, so there's nothing else to do.
  if (!inserted) {
    return;
  }

  net_log_.AddEvent(NetLogEventType::PROXY_OVERRIDE_END_HOST_RESOLUTION, [&] {
    return CreateEndHostResolutionNetLogParams(host, result,
                                               /*was_resolved_sync=*/false);
  });

  if (!service_ || !service_->IsReady()) {
    // Something happened during DNS resolution which invalidated the
    // service's state (e.g. config update). The service will resume
    // the current request if/when needed.
    return;
  }

  int rv = ContinueProxyResolution();
  if (rv == ERR_IO_PENDING) {
    // Missing a required DNS resolution.
    return;
  }

  QueryComplete(rv);
}

int ConfiguredProxyResolutionRequest::EvaluateApplicableOverrideRules() {
  while (!applicable_override_rules_.empty()) {
    const auto& applicable_rule = applicable_override_rules_.front();

    // Not having any conditions means they are all satisfied.
    bool conditions_satisfied = true;
    for (const auto& dns_condition : applicable_rule.dns_conditions) {
      // If no results are found in the DNS results map, it means that the DNS
      // resolution request owned by the service is still pending.
      auto dns_result_it = dns_results_.find(dns_condition.host);
      if (dns_result_it == dns_results_.end()) {
        return ERR_IO_PENDING;
      }

      conditions_satisfied =
          conditions_satisfied &&
          CheckDnsCondition(dns_condition, dns_result_it->second);
    }

    if (conditions_satisfied) {
      net_log_.AddEvent(NetLogEventType::PROXY_RESOLUTION_OVERRIDE_RULE_APPLIED,
                        [&] { return applicable_rule.ToDict(); });
      results_->UseProxyList(applicable_rule.proxy_list);
      break;
    }

    // Conditions were evaluated successfully, but are not satisfied. This
    // current rule is therefore no longer applicable.
    applicable_override_rules_.pop();
  }

  dns_results_.clear();
  net_log_.EndEvent(NetLogEventType::PROXY_RESOLUTION_OVERRIDE_RULES);
  return OK;
}

void ConfiguredProxyResolutionRequest::Reset() {
  // The request may already be running in the resolver.
  resolve_job_.reset();
  applicable_override_rules_ = base::queue<ProxyConfig::ProxyOverrideRule>();
  dns_results_.clear();
}

}  // namespace net
