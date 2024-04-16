// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_CONTEXT_HOST_RESOLVER_H_
#define NET_DNS_CONTEXT_HOST_RESOLVER_H_

#include <memory>
#include <optional>
#include <unordered_set>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/values.h"
#include "net/base/net_export.h"
#include "net/base/network_handle.h"
#include "net/base/network_isolation_key.h"
#include "net/dns/host_resolver.h"
#include "net/dns/host_resolver_system_task.h"
#include "net/log/net_log_with_source.h"
#include "url/scheme_host_port.h"

namespace base {
class TickClock;
}  // namespace base

namespace net {

class HostCache;
class HostResolverManager;
class ResolveContext;
class URLRequestContext;

// Wrapper for HostResolverManager, expected to be owned by a URLRequestContext,
// that sets per-URLRequestContext parameters for created requests. Except for
// tests, typically only interacted with through the HostResolver interface.
//
// See HostResolver::Create[...]() methods for construction.
class NET_EXPORT ContextHostResolver : public HostResolver {
 public:
  // Creates a ContextHostResolver that forwards all of its requests through
  // |manager|. Requests will be cached using |host_cache| if not null.
  ContextHostResolver(HostResolverManager* manager,
                      std::unique_ptr<ResolveContext> resolve_context);
  // Same except the created resolver will own its own HostResolverManager.
  ContextHostResolver(std::unique_ptr<HostResolverManager> owned_manager,
                      std::unique_ptr<ResolveContext> resolve_context);

  ContextHostResolver(const ContextHostResolver&) = delete;
  ContextHostResolver& operator=(const ContextHostResolver&) = delete;

  ~ContextHostResolver() override;

  // HostResolver methods:
  void OnShutdown() override;
  std::unique_ptr<ResolveHostRequest> CreateRequest(
      url::SchemeHostPort host,
      NetworkAnonymizationKey network_anonymization_key,
      NetLogWithSource net_log,
      std::optional<ResolveHostParameters> optional_parameters) override;
  std::unique_ptr<ResolveHostRequest> CreateRequest(
      const HostPortPair& host,
      const NetworkAnonymizationKey& network_anonymization_key,
      const NetLogWithSource& net_log,
      const std::optional<ResolveHostParameters>& optional_parameters) override;
  std::unique_ptr<ServiceEndpointRequest> CreateServiceEndpointRequest(
      Host host,
      NetworkAnonymizationKey network_anonymization_key,
      NetLogWithSource net_log,
      ResolveHostParameters parameters) override;
  std::unique_ptr<ProbeRequest> CreateDohProbeRequest() override;
  std::unique_ptr<MdnsListener> CreateMdnsListener(
      const HostPortPair& host,
      DnsQueryType query_type) override;
  HostCache* GetHostCache() override;
  base::Value::Dict GetDnsConfigAsValue() const override;
  void SetRequestContext(URLRequestContext* request_context) override;
  HostResolverManager* GetManagerForTesting() override;
  const URLRequestContext* GetContextForTesting() const override;
  handles::NetworkHandle GetTargetNetworkForTesting() const override;

  // Returns the number of host cache entries that were restored, or 0 if there
  // is no cache.
  size_t LastRestoredCacheSize() const;
  // Returns the number of entries in the host cache, or 0 if there is no cache.
  size_t CacheSize() const;

  void SetHostResolverSystemParamsForTest(
      const HostResolverSystemTask::Params& host_resolver_system_params);
  void SetTickClockForTesting(const base::TickClock* tick_clock);
  ResolveContext* resolve_context_for_testing() {
    return resolve_context_.get();
  }

 private:
  std::unique_ptr<HostResolverManager> owned_manager_;
  // `manager_` might point to `owned_manager_`. It must be declared last and
  // cleared first.
  const raw_ptr<HostResolverManager> manager_;
  std::unique_ptr<ResolveContext> resolve_context_;

  // If true, the context is shutting down. Subsequent request Start() calls
  // will always fail immediately with ERR_CONTEXT_SHUT_DOWN.
  bool shutting_down_ = false;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace net

#endif  // NET_DNS_CONTEXT_HOST_RESOLVER_H_
