// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_RESOLUTION_CONFIGURED_PROXY_RESOLUTION_REQUEST_H_
#define NET_PROXY_RESOLUTION_CONFIGURED_PROXY_RESOLUTION_REQUEST_H_

#include <memory>
#include <string>
#include <variant>

#include "base/containers/queue.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "net/base/completion_once_callback.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/request_priority.h"
#include "net/dns/host_resolver.h"
#include "net/log/net_log_with_source.h"
#include "net/proxy_resolution/proxy_config.h"
#include "net/proxy_resolution/proxy_resolution_request.h"
#include "net/proxy_resolution/proxy_resolver.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

namespace net {

class ProxyInfo;
class ConfiguredProxyResolutionService;

// This is the concrete implementation of ProxyResolutionRequest used by
// ConfiguredProxyResolutionService. Manages a single asynchronous proxy
// resolution request.
class ConfiguredProxyResolutionRequest final : public ProxyResolutionRequest {
 public:
  ConfiguredProxyResolutionRequest(
      ConfiguredProxyResolutionService* service,
      const GURL& url,
      const std::string& method,
      const NetworkAnonymizationKey& network_anonymization_key,
      ProxyInfo* results,
      const CompletionOnceCallback user_callback,
      const NetLogWithSource& net_log,
      RequestPriority priority);

  ConfiguredProxyResolutionRequest(const ConfiguredProxyResolutionRequest&) =
      delete;
  ConfiguredProxyResolutionRequest& operator=(
      const ConfiguredProxyResolutionRequest&) = delete;

  ~ConfiguredProxyResolutionRequest() override;

  // Starts the resolve proxy request.
  int Start();

  bool is_started() const {
    // Note that !! casts to bool. (VS gives a warning otherwise).
    return !!resolve_job_.get() || !dns_results_.empty();
  }

  void StartAndCompleteCheckingForSynchronous();

  // Cancels any pending activities (e.g. DNS resolution or proxy resolver
  // requests). The overall request can be resumed by calling
  // `StartAndCompleteCheckingForSynchronous()`.
  void Cancel();

  // Returns true if the request has been completed.
  bool was_completed() const { return user_callback_.is_null(); }

  // Callback for when the ProxyResolver request has completed.
  void QueryComplete(int result_code);

  // Helper to call after ProxyResolver completion (both synchronous and
  // asynchronous). Fixes up the result that is to be returned to user.
  int QueryDidComplete(int result_code);

  // Helper to call if the request completes synchronously, since in that case
  // the request will not be added to |pending_requests_| (in
  // |ConfiguredProxyResolutionService|).
  int QueryDidCompleteSynchronously(int result_code);

  NetLogWithSource* net_log() { return &net_log_; }

  // Request implementation:
  LoadState GetLoadState() const override;

 private:
  // Struct capturing the result code of a completed DNS resolution request.
  struct ResolveHostResult {
    // Constructs the struct with `result_code` being the net error code of a
    // completed DNS resolution request `completed_request`.
    ResolveHostResult(
        int result_code,
        const HostResolver::ResolveHostRequest& completed_request);

    void AddToDict(base::Value::Dict& dict) const;

    const int result_code;
    const bool is_address_list_empty;
  };

  // Continues proxy resolution after attempting to evaluate override rules.
  // Will return ERR_IO_PENDING if there still are pending DNS resolution
  // requests for override rules, or if the request is now waiting for a PAC
  // script's result.
  int ContinueProxyResolution();

  void OnDnsHostResolved(const url::SchemeHostPort& host, int result_code);

  int EvaluateApplicableOverrideRules();

  // Resets the request's state, which will cancel any pending activities and
  // ensure that `is_started()` returns false. The main difference with
  // `Cancel()` is the intent and extra logging in cancellation cases.
  void Reset();

  static bool CheckDnsCondition(
      const ProxyConfig::ProxyOverrideRule::DnsProbeCondition& dns_condition,
      const ResolveHostResult& dns_result);

  // Note that Request holds a bare pointer to the
  // ConfiguredProxyResolutionService. Outstanding requests are cancelled during
  // ~ConfiguredProxyResolutionService, so this is guaranteed to be valid
  // throughout our lifetime.
  raw_ptr<ConfiguredProxyResolutionService> service_;
  CompletionOnceCallback user_callback_;
  raw_ptr<ProxyInfo> results_;
  const GURL url_;
  const std::string method_;
  const NetworkAnonymizationKey network_anonymization_key_;
  std::unique_ptr<ProxyResolver::Request> resolve_job_;
  MutableNetworkTrafficAnnotationTag traffic_annotation_;
  NetLogWithSource net_log_;
  const RequestPriority priority_;
  // Time when the request was created.  Stored here rather than in |results_|
  // because the time in |results_| will be cleared.
  base::TimeTicks creation_time_;

  base::queue<ProxyConfig::ProxyOverrideRule> applicable_override_rules_;

  // Map containing DNS resolution requests, or their results, issued for proxy
  // override rules' conditions. The key is the host that is being resolved. The
  // value will either be the pending request itself, or a result struct.
  // Deleting this map will cancel any outstanding resolution requests.
  absl::flat_hash_map<
      url::SchemeHostPort,
      std::variant<std::unique_ptr<HostResolver::ResolveHostRequest>,
                   ResolveHostResult>>
      dns_results_;
};

}  // namespace net

#endif  // NET_PROXY_RESOLUTION_CONFIGURED_PROXY_RESOLUTION_REQUEST_H_
