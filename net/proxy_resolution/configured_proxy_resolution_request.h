// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_RESOLUTION_CONFIGURED_PROXY_RESOLUTION_REQUEST_H_
#define NET_PROXY_RESOLUTION_CONFIGURED_PROXY_RESOLUTION_REQUEST_H_

#include <memory>
#include <string>

#include "base/time/time.h"
#include "net/base/completion_once_callback.h"
#include "net/base/network_isolation_key.h"
#include "net/log/net_log_with_source.h"
#include "net/proxy_resolution/proxy_resolution_request.h"
#include "net/proxy_resolution/proxy_resolver.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/gurl.h"

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
      const NetworkIsolationKey& network_isolation_key,
      ProxyInfo* results,
      const CompletionOnceCallback user_callback,
      const NetLogWithSource& net_log);

  ConfiguredProxyResolutionRequest(const ConfiguredProxyResolutionRequest&) =
      delete;
  ConfiguredProxyResolutionRequest& operator=(
      const ConfiguredProxyResolutionRequest&) = delete;

  ~ConfiguredProxyResolutionRequest() override;

  // Starts the resolve proxy request.
  int Start();

  bool is_started() const {
    // Note that !! casts to bool. (VS gives a warning otherwise).
    return !!resolve_job_.get();
  }

  void StartAndCompleteCheckingForSynchronous();

  void CancelResolveJob();

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
  // Note that Request holds a bare pointer to the
  // ConfiguredProxyResolutionService. Outstanding requests are cancelled during
  // ~ConfiguredProxyResolutionService, so this is guaranteed to be valid
  // throughout our lifetime.
  ConfiguredProxyResolutionService* service_;
  CompletionOnceCallback user_callback_;
  ProxyInfo* results_;
  const GURL url_;
  const std::string method_;
  const NetworkIsolationKey network_isolation_key_;
  std::unique_ptr<ProxyResolver::Request> resolve_job_;
  MutableNetworkTrafficAnnotationTag traffic_annotation_;
  NetLogWithSource net_log_;
  // Time when the request was created.  Stored here rather than in |results_|
  // because the time in |results_| will be cleared.
  base::TimeTicks creation_time_;
};

}  // namespace net

#endif  // NET_PROXY_RESOLUTION_CONFIGURED_PROXY_RESOLUTION_REQUEST_H_
