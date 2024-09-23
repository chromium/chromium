// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_MAPPED_HOST_RESOLVER_H_
#define NET_DNS_MAPPED_HOST_RESOLVER_H_

#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include "base/values.h"
#include "net/base/completion_once_callback.h"
#include "net/base/host_mapping_rules.h"
#include "net/base/net_export.h"
#include "net/base/network_anonymization_key.h"
#include "net/dns/dns_config.h"
#include "net/dns/host_resolver.h"
#include "net/log/net_log_with_source.h"
#include "url/scheme_host_port.h"

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

  void OnShutdown() override;

  // Adds a rule to this mapper. The format of the rule can be one of:
  //
  //   "MAP" <hostname_pattern> <replacement_host> [":" <replacement_port>]
  //   "EXCLUDE" <hostname_pattern>
  //
  // The <replacement_host> can be either a hostname, or an IP address literal,
  // or "^NOTFOUND". If it is "^NOTFOUND" then all matched hostnames will fail
  // to be resolved with ERR_NAME_NOT_RESOLVED.
  //
  // Returns true if the rule was successfully parsed and added.
  bool AddRuleFromString(std::string_view rule_string) {
    return rules_.AddRuleFromString(rule_string);
  }

  // Takes a comma separated list of rules, and assigns them to this resolver.
  void SetRulesFromString(std::string_view rules_string) {
    rules_.SetRulesFromString(rules_string);
  }

  // HostResolver methods:
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
  HostCache* GetHostCache() override;
  base::Value::Dict GetDnsConfigAsValue() const override;
  void SetRequestContext(URLRequestContext* request_context) override;
  HostResolverManager* GetManagerForTesting() override;

 private:
  std::unique_ptr<HostResolver> impl_;

  HostMappingRules rules_;
};

}  // namespace net

#endif  // NET_DNS_MAPPED_HOST_RESOLVER_H_
