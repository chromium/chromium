// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy_resolution/configured_proxy_resolution_request.h"

#include <optional>
#include <utility>
#include <variant>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "net/base/net_errors.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_source_type.h"
#include "net/proxy_resolution/configured_proxy_resolution_service.h"
#include "net/proxy_resolution/proxy_info.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

namespace net {

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

    auto* host_resolver = service_->GetHostResolverForOverrideRules();
    CHECK(host_resolver);

    for (const auto& rule : service_->config_->value().proxy_override_rules()) {
      if (rule.MatchesDestination(url_)) {
        // TODO(crbug.com/454638342): Add optimization to skip marking a rule as
        // applicable if we can already discard it based on cached DNS
        // resolution state (and avoid starting actual DNS resolution that is
        // not needed).
        applicable_override_rules_.push(rule);

        for (const auto& dns_condition : rule.dns_conditions) {
          if (!dns_results_.contains(dns_condition.host)) {
            HostResolver::ResolveHostParameters parameters;
            parameters.initial_priority = priority_;

            auto sub_net_log = NetLogWithSource::Make(
                net_log_.net_log(),
                NetLogSourceType::PROXY_OVERRIDE_HOST_RESOLUTION);
            auto request = host_resolver->CreateRequest(
                dns_condition.host, network_anonymization_key_, sub_net_log,
                parameters);

            // Link the two net log sources together.
            sub_net_log.AddEventReferencingSource(
                NetLogEventType::PROXY_OVERRIDE_HOST_RESOLUTION,
                net_log_.source());
            net_log_.AddEvent(
                NetLogEventType::PROXY_OVERRIDE_BEGIN_HOST_RESOLUTION, [&] {
                  base::Value::Dict dict;
                  sub_net_log.source().AddToEventParameters(dict);
                  dict.Set("dns_condition", dns_condition.ToDict());
                  return dict;
                });

            int rv = request->Start(base::BindOnce(
                &ConfiguredProxyResolutionRequest::OnDnsHostResolved,
                base::Unretained(this), dns_condition.host));
            if (rv != ERR_IO_PENDING) {
              auto result = ResolveHostResult(rv, *request);
              net_log_.AddEvent(
                  NetLogEventType::PROXY_OVERRIDE_END_HOST_RESOLUTION, [&] {
                    base::Value::Dict dict;
                    dict.Set("host", dns_condition.host.Serialize());
                    dict.Set("was_resolved_sync", true);
                    result.AddToDict(dict);
                    return dict;
                  });
              dns_results_.emplace(dns_condition.host, std::move(result));
            } else {
              dns_results_.emplace(dns_condition.host, std::move(request));
            }
          }
        }
      }
    }

    if (applicable_override_rules_.empty()) {
      // Not override rules applied.
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

  if (!dns_results_.empty()) {
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
  if (result_code != ERR_ABORTED && !rv)
    CHECK(results_->traffic_annotation().is_valid());

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

ConfiguredProxyResolutionRequest::ResolveHostResult::ResolveHostResult(
    int result_code,
    const HostResolver::ResolveHostRequest& completed_request)
    : result_code(result_code),
      is_address_list_empty(completed_request.GetAddressResults().empty()) {}

void ConfiguredProxyResolutionRequest::ResolveHostResult::AddToDict(
    base::Value::Dict& dict) const {
  dict.Set("net_error", result_code);
  dict.Set("is_address_list_empty", is_address_list_empty);
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
    int result_code) {
  auto dns_result_it = dns_results_.find(host);
  CHECK(
      std::holds_alternative<std::unique_ptr<HostResolver::ResolveHostRequest>>(
          dns_result_it->second));
  auto request =
      std::move(std::get<std::unique_ptr<HostResolver::ResolveHostRequest>>(
          dns_result_it->second));
  CHECK(request);
  dns_result_it->second.emplace<ResolveHostResult>(result_code, *request);

  net_log_.AddEvent(NetLogEventType::PROXY_OVERRIDE_END_HOST_RESOLUTION, [&] {
    base::Value::Dict dict;
    dict.Set("host", host.Serialize());
    dict.Set("was_resolved_sync", false);
    std::get<ResolveHostResult>(dns_result_it->second).AddToDict(dict);
    return dict;
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
      const auto& dns_result = dns_results_.at(dns_condition.host);
      if (std::holds_alternative<
              std::unique_ptr<HostResolver::ResolveHostRequest>>(dns_result)) {
        return ERR_IO_PENDING;
      }
      conditions_satisfied =
          conditions_satisfied &&
          CheckDnsCondition(dns_condition,
                            std::get<ResolveHostResult>(dns_result));
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

// static
bool ConfiguredProxyResolutionRequest::CheckDnsCondition(
    const ProxyConfig::ProxyOverrideRule::DnsProbeCondition& dns_condition,
    const ConfiguredProxyResolutionRequest::ResolveHostResult& dns_result) {
  switch (dns_condition.result) {
    case ProxyConfig::ProxyOverrideRule::DnsProbeCondition::Result::kNotFound:
      return dns_result.is_address_list_empty;
    case ProxyConfig::ProxyOverrideRule::DnsProbeCondition::Result::kResolved:
      return !dns_result.is_address_list_empty;
  }
}

}  // namespace net
