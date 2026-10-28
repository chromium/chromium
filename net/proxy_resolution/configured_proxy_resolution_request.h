// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_RESOLUTION_CONFIGURED_PROXY_RESOLUTION_REQUEST_H_
#define NET_PROXY_RESOLUTION_CONFIGURED_PROXY_RESOLUTION_REQUEST_H_

#include <memory>
#include <optional>
#include <string>
#include <variant>

#include "base/containers/queue.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "net/base/completion_once_callback.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/network_handle.h"
#include "net/base/request_priority.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_with_source.h"
#include "net/proxy_resolution/proxy_config.h"
#include "net/proxy_resolution/proxy_resolution_request.h"
#include "net/proxy_resolution/proxy_resolver.h"
#include "net/proxy_resolution/resolve_host_request.h"
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
      handles::NetworkHandle target_network,
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
    return !!resolve_job_.get() || !applicable_override_rules_.empty();
  }

  // Resumes a pending proxy resolution request once the service becomes ready
  // (e.g. after PAC initialization or dynamic route updates).
  // Attempts to resolve the request synchronously, or starts asynchronous
  // resolution (DNS probes for override rules or a `ProxyResolver` query) via
  // `Start()` if needed.
  // If resolution completes immediately without waiting for async work,
  // populates `results_` and delivers the final status code to the caller by
  // invoking `user_callback_` via `OnAsyncResolutionComplete()`.
  // Otherwise, the request remains pending and this may be called again later
  // to resume.
  void ResumeOrCompleteSynchronously();

  // Cancels any pending activities (e.g. DNS resolution or proxy resolver
  // requests). The overall request can be resumed by calling
  // `ResumeOrCompleteSynchronously()`.
  void CancelActiveJob();

  // Cancels an in-flight per-request PAC evaluation (`GetProxyForURL` /
  // `resolve_job_`), if any, while marking `ProxyOverrideRules` (POR) as
  // already evaluated (`override_rules_evaluated_ = true`) and leaving any
  // in-flight POR DNS probes untouched. Returns true if an active
  // `resolve_job_` was cancelled.
  bool CancelResolveJob();

  // Resets cached `ProxyOverrideRules` (POR) evaluation state when the
  // underlying ProxyConfig is reset while the request is paused.
  void ResetOverrideRulesEvaluated() { override_rules_evaluated_ = false; }

  // Returns true if the request has been completed.
  bool was_completed() const { return user_callback_.is_null(); }

  // Completes a pending request after asynchronous proxy resolution finishes
  // (e.g. from `ProxyResolver`, override rule DNS resolution, or when a
  // previously stalled request resolves during
  // `ResumeOrCompleteSynchronously()`), finalizing `results_` and invoking
  // `user_callback_`.
  void OnAsyncResolutionComplete(int result_code);

  // Post-processes `results_` after proxy resolution finishes (whether via
  // override rules, synchronous fallback rules, or `ProxyResolver`, and
  // whether completed synchronously or asynchronously).
  // Returns the final result code to be returned to the user.
  int FinalizeRequestResult(int result_code);

  // Called to notify that an asynchronous DNS resolution request was completed
  // for `host` with `result`. This will add an entry in the `dns_results_` map.
  void OnDnsHostResolved(const url::SchemeHostPort& host,
                         const ResolveHostResult& result);

  handles::NetworkHandle target_network() const { return target_network_; }

  // Begins a NetLog wait event (`event_type`) if not already active, closing
  // any previously active wait event first so wait events remain balanced.
  void BeginWaitEvent(NetLogEventType event_type);

  // Ends the currently active NetLog wait event, if any.
  void EndWaitEvent();

  NetLogWithSource* net_log() { return &net_log_; }

  // Request implementation:
  LoadState GetLoadState() const override;

 private:
  // Continues proxy resolution after attempting to evaluate override rules.
  // Will return ERR_IO_PENDING if there still are pending DNS resolution
  // requests for override rules, or if the request is now waiting for a PAC
  // script's result.
  int ContinueProxyResolution();

  int EvaluateApplicableOverrideRules();

  // Resets the request's state, which will cancel any pending activities and
  // ensure that `is_started()` returns false. The main difference with
  // `CancelActiveJob()` is the intent and extra logging in cancellation cases.
  void ResetActiveJobState();

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
  const handles::NetworkHandle target_network_;
  // Active per-request PAC script evaluation (`ProxyResolver::GetProxyForURL`)
  // for this URL, after the service has already initialized the PAC resolver.
  std::unique_ptr<ProxyResolver::Request> resolve_job_;
  MutableNetworkTrafficAnnotationTag traffic_annotation_;
  NetLogWithSource net_log_;
  const RequestPriority priority_;
  // Time when the request was created.  Stored here rather than in |results_|
  // because the time in |results_| will be cleared.
  base::TimeTicks creation_time_;

  // Queue of `ProxyOverrideRules` (POR) matching `url_` whose DNS conditions
  // are still being evaluated.
  base::queue<ProxyConfig::ProxyOverrideRule> applicable_override_rules_;
  // True once `ProxyOverrideRules` (POR, including any POR DNS probes) have
  // finished evaluating for this request without matching, so resuming after a
  // dynamic routing update can skip re-evaluating POR.
  bool override_rules_evaluated_ = false;
  // Currently open NetLog wait event (`WAITING_FOR_INIT_PAC` or
  // `WAITING_FOR_DYNAMIC_PROXY_CONFIGS`), if the request is paused waiting for
  // service readiness.
  std::optional<NetLogEventType> current_wait_event_;

  // Map containing DNS resolution results, issued for proxy override rules'
  // conditions. The key is the host that is being resolved. The value is result
  // struct. Map entries will either be added synchronously (if the DNS entry
  // was cached) or via a call to `OnDnsHostResolved`.
  absl::flat_hash_map<url::SchemeHostPort, ResolveHostResult> dns_results_;

  base::WeakPtrFactory<ConfiguredProxyResolutionRequest> weak_factory_{this};
};

}  // namespace net

#endif  // NET_PROXY_RESOLUTION_CONFIGURED_PROXY_RESOLUTION_REQUEST_H_
