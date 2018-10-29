// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_MAPPED_HOST_RESOLVER_H_
#define NET_DNS_MAPPED_HOST_RESOLVER_H_

#include <memory>
#include <string>
#include <vector>

#include "net/base/completion_once_callback.h"
#include "net/base/host_mapping_rules.h"
#include "net/base/net_export.h"
#include "net/dns/dns_config.h"
#include "net/dns/host_resolver.h"

namespace net {

// This class wraps an existing HostResolver instance, but modifies the
// request before passing it off to |impl|. This is different from
// MockHostResolver which does the remapping at the HostResolverProc
// layer, so it is able to preserve the effectiveness of the cache.
class NET_EXPORT MappedHostResolver : public HostResolver {
 public:
  // Creates a MappedHostResolver that forwards all of its requests through
  // |impl|.
  explicit MappedHostResolver(std::unique_ptr<HostResolver> impl);
  ~MappedHostResolver() override;

  // Adds a rule to this mapper. The format of the rule can be one of:
  //
  //   "MAP" <hostname_pattern> <replacement_host> [":" <replacement_port>]
  //   "EXCLUDE" <hostname_pattern>
  //
  // The <replacement_host> can be either a hostname, or an IP address literal,
  // or "~NOTFOUND". If it is "~NOTFOUND" then all matched hostnames will fail
  // to be resolved with ERR_NAME_NOT_RESOLVED.
  //
  // Returns true if the rule was successfully parsed and added.
  bool AddRuleFromString(const std::string& rule_string) {
    return rules_.AddRuleFromString(rule_string);
  }

  // Takes a comma separated list of rules, and assigns them to this resolver.
  void SetRulesFromString(const std::string& rules_string) {
    rules_.SetRulesFromString(rules_string);
  }

  // HostResolver methods:
  std::unique_ptr<ResolveHostRequest> CreateRequest(
      const HostPortPair& host,
      const NetLogWithSource& net_log,
      const base::Optional<ResolveHostParameters>& optional_parameters)
      override;
  int Resolve(const RequestInfo& info,
              RequestPriority priority,
              AddressList* addresses,
              CompletionOnceCallback callback,
              std::unique_ptr<Request>* request,
              const NetLogWithSource& net_log) override;
  int ResolveFromCache(const RequestInfo& info,
                       AddressList* addresses,
                       const NetLogWithSource& net_log) override;
  int ResolveStaleFromCache(const RequestInfo& info,
                            AddressList* addresses,
                            HostCache::EntryStaleness* stale_info,
                            const NetLogWithSource& source_net_log) override;
  void SetDnsClientEnabled(bool enabled) override;
  HostCache* GetHostCache() override;
  bool HasCached(base::StringPiece hostname,
                 HostCache::Entry::Source* source_out,
                 HostCache::EntryStaleness* stale_out) const override;
  std::unique_ptr<base::Value> GetDnsConfigAsValue() const override;
  void SetNoIPv6OnWifi(bool no_ipv6_on_wifi) override;
  bool GetNoIPv6OnWifi() override;
  void SetDnsConfigOverrides(const DnsConfigOverrides& overrides) override;
  void SetRequestContext(URLRequestContext* request_context) override;
  const std::vector<DnsConfig::DnsOverHttpsServerConfig>*
  GetDnsOverHttpsServersForTesting() const override;

 private:
  class AlwaysErrorRequestImpl;

  // Modify the request |info| according to |rules_|. Returns either OK or
  // the network error code that the hostname's resolution mapped to.
  int ApplyRules(RequestInfo* info) const;

  std::unique_ptr<HostResolver> impl_;

  HostMappingRules rules_;
};

}  // namespace net

#endif  // NET_DNS_MAPPED_HOST_RESOLVER_H_
