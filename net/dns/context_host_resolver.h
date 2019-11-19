// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_CONTEXT_HOST_RESOLVER_H_
#define NET_DNS_CONTEXT_HOST_RESOLVER_H_

#include <memory>
#include <unordered_set>
#include <vector>

#include "base/macros.h"
#include "base/sequence_checker.h"
#include "net/base/net_export.h"
#include "net/dns/host_resolver.h"

namespace base {
class TickClock;
}  // namespace base

namespace net {

class HostCache;
class HostResolverManager;
struct ProcTaskParams;
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
  explicit ContextHostResolver(HostResolverManager* manager,
                               std::unique_ptr<HostCache> host_cache);
  // Same except the created resolver will own its own HostResolverManager.
  explicit ContextHostResolver(
      std::unique_ptr<HostResolverManager> owned_manager,
      std::unique_ptr<HostCache> host_cache);
  ~ContextHostResolver() override;

  // HostResolver methods:
  void OnShutdown() override;
  std::unique_ptr<ResolveHostRequest> CreateRequest(
      const HostPortPair& host,
      const NetworkIsolationKey& network_isolation_key,
      const NetLogWithSource& net_log,
      const base::Optional<ResolveHostParameters>& optional_parameters)
      override;
  std::unique_ptr<ProbeRequest> CreateDohProbeRequest() override;
  std::unique_ptr<MdnsListener> CreateMdnsListener(
      const HostPortPair& host,
      DnsQueryType query_type) override;
  HostCache* GetHostCache() override;
  std::unique_ptr<base::Value> GetDnsConfigAsValue() const override;
  void SetRequestContext(URLRequestContext* request_context) override;
  HostResolverManager* GetManagerForTesting() override;
  const URLRequestContext* GetContextForTesting() const override;

  // Returns the number of host cache entries that were restored, or 0 if there
  // is no cache.
  size_t LastRestoredCacheSize() const;
  // Returns the number of entries in the host cache, or 0 if there is no cache.
  size_t CacheSize() const;

  void SetProcParamsForTesting(const ProcTaskParams& proc_params);
  void SetTickClockForTesting(const base::TickClock* tick_clock);

  size_t GetNumActiveRequestsForTesting() const {
    return handed_out_requests_.size();
  }

 private:
  class WrappedRequest;
  class WrappedResolveHostRequest;
  class WrappedProbeRequest;

  HostResolverManager* const manager_;
  std::unique_ptr<HostResolverManager> owned_manager_;

  // Requests are expected to clear themselves from this set on destruction or
  // cancellation.  Requests in an early shutdown state (from
  // HostResolver::OnShutdown()) are still in this set, so they can be notified
  // on resolver destruction.
  std::unordered_set<WrappedRequest*> handed_out_requests_;

  URLRequestContext* context_ = nullptr;
  std::unique_ptr<HostCache> host_cache_;

  // If true, the context is shutting down. Subsequent request Start() calls
  // will always fail immediately with ERR_CONTEXT_SHUT_DOWN.
  bool shutting_down_ = false;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(ContextHostResolver);
};

}  // namespace net

#endif  // NET_DNS_CONTEXT_HOST_RESOLVER_H_
