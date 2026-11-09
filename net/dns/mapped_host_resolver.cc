// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/mapped_host_resolver.h"

#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/values.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/base/network_anonymization_key.h"
#include "net/dns/host_resolver.h"
#include "net/log/net_log_with_source.h"
#include "url/scheme_host_port.h"

namespace net {

MappedHostResolver::MappedHostResolver(std::unique_ptr<HostResolver> impl)
    : impl_(std::move(impl)) {}

MappedHostResolver::~MappedHostResolver() = default;

void MappedHostResolver::OnShutdown() {
  impl_->OnShutdown();
}

std::unique_ptr<HostResolver::ResolveHostRequest>
MappedHostResolver::CreateRequest(
    url::SchemeHostPort host,
    NetworkAnonymizationKey network_anonymization_key,
    handles::NetworkHandle target_network,
    NetLogWithSource source_net_log,
    std::optional<ResolveHostParameters> optional_parameters) {
  switch (rules_.RewriteSchemeHostPort(host)) {
    case HostMappingRules::RewriteResult::kRewritten:
      DCHECK(host.IsValid());
      DCHECK_NE(host.host(), "^NOTFOUND");
      return impl_->CreateRequest(
          std::move(host), std::move(network_anonymization_key), target_network,
          std::move(source_net_log), std::move(optional_parameters));
    case HostMappingRules::RewriteResult::kInvalidRewrite:
      // Treat any invalid mapping as if it was "^NOTFOUND" (which should itself
      // result in `kInvalidRewrite`).
      return CreateFailingRequest(ERR_NAME_NOT_RESOLVED);
    case HostMappingRules::RewriteResult::kNoMatchingRule:
      return impl_->CreateRequest(
          std::move(host), std::move(network_anonymization_key),
          std::move(target_network), std::move(source_net_log),
          std::move(optional_parameters));
  }
}

std::unique_ptr<HostResolver::ResolveHostRequest>
MappedHostResolver::CreateRequest(
    const HostPortPair& host,
    const NetworkAnonymizationKey& network_anonymization_key,
    handles::NetworkHandle target_network,
    const NetLogWithSource& source_net_log,
    const std::optional<ResolveHostParameters>& optional_parameters) {
  HostPortPair rewritten = host;
  rules_.RewriteHost(&rewritten);

  if (rewritten.host() == "^NOTFOUND") {
    return CreateFailingRequest(ERR_NAME_NOT_RESOLVED);
  }

  return impl_->CreateRequest(rewritten, network_anonymization_key,
                              target_network, source_net_log,
                              optional_parameters);
}

std::unique_ptr<HostResolver::ServiceEndpointRequest>
MappedHostResolver::CreateServiceEndpointRequest(
    Host host,
    NetworkAnonymizationKey network_anonymization_key,
    handles::NetworkHandle target_network,
    NetLogWithSource net_log,
    ResolveHostParameters parameters) {
  if (host.HasScheme()) {
    url::SchemeHostPort rewritten = host.AsSchemeHostPort();
    switch (rules_.RewriteSchemeHostPort(rewritten)) {
      case HostMappingRules::RewriteResult::kRewritten:
        DCHECK(rewritten.IsValid());
        DCHECK_NE(rewritten.host(), "^NOTFOUND");
        return impl_->CreateServiceEndpointRequest(
            Host(std::move(rewritten)), std::move(network_anonymization_key),
            std::move(target_network), std::move(net_log),
            std::move(parameters));
      case HostMappingRules::RewriteResult::kInvalidRewrite:
        // Treat any invalid mapping as if it was "^NOTFOUND" (which should
        // itself result in `kInvalidRewrite`).
        return CreateFailingServiceEndpointRequest(ERR_NAME_NOT_RESOLVED);
      case HostMappingRules::RewriteResult::kNoMatchingRule:
        return impl_->CreateServiceEndpointRequest(
            std::move(host), std::move(network_anonymization_key),
            std::move(target_network), std::move(net_log),
            std::move(parameters));
    }
  } else {
    HostPortPair rewritten = host.AsHostPortPair();
    rules_.RewriteHost(&rewritten);

    if (rewritten.host() == "^NOTFOUND") {
      return CreateFailingServiceEndpointRequest(ERR_NAME_NOT_RESOLVED);
    }
    return impl_->CreateServiceEndpointRequest(
        Host(std::move(rewritten)), std::move(network_anonymization_key),
        std::move(target_network), std::move(net_log), std::move(parameters));
  }
}

std::unique_ptr<HostResolver::ProbeRequest>
MappedHostResolver::CreateDohProbeRequest() {
  return impl_->CreateDohProbeRequest();
}

std::unique_ptr<HostResolver::MdnsListener>
MappedHostResolver::CreateMdnsListener(const HostPortPair& host,
                                       DnsQueryType query_type) {
  HostPortPair rewritten = host;
  rules_.RewriteHost(&rewritten);
  if (rewritten.host() == "^NOTFOUND") {
    return CreateFailingMdnsListener(ERR_NAME_NOT_RESOLVED);
  }
  return impl_->CreateMdnsListener(rewritten, query_type);
}

HostCache* MappedHostResolver::GetHostCache() {
  return impl_->GetHostCache();
}

base::DictValue MappedHostResolver::GetDnsConfigAsValue() const {
  return impl_->GetDnsConfigAsValue();
}

void MappedHostResolver::SetRequestContext(URLRequestContext* request_context) {
  impl_->SetRequestContext(request_context);
}

bool MappedHostResolver::IsHappyEyeballsV3Enabled() const {
  return impl_->IsHappyEyeballsV3Enabled();
}

HostResolverManager* MappedHostResolver::GetManagerForTesting() {
  return impl_->GetManagerForTesting();
}

const URLRequestContext* MappedHostResolver::GetContextForTesting() const {
  return impl_->GetContextForTesting();  // IN-TEST
}

handles::NetworkHandle MappedHostResolver::GetTargetNetworkForTesting() const {
  return impl_->GetTargetNetworkForTesting();  // IN-TEST
}

}  // namespace net
