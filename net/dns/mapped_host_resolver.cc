// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/mapped_host_resolver.h"

#include <string>
#include <utility>

#include "base/strings/string_util.h"
#include "base/values.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/url_util.h"
#include "net/dns/host_resolver.h"
#include "net/log/net_log_with_source.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"
#include "url/url_canon.h"

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
    NetLogWithSource source_net_log,
    absl::optional<ResolveHostParameters> optional_parameters) {
  GURL rewritten_url = host.GetURL();
  HostMappingRules::RewriteResult result = rules_.RewriteUrl(rewritten_url);

  switch (result) {
    case HostMappingRules::RewriteResult::kRewritten:
      DCHECK(rewritten_url.is_valid());
      DCHECK_NE(rewritten_url.host_piece(), "~NOTFOUND");
      return impl_->CreateRequest(url::SchemeHostPort(rewritten_url),
                                  std::move(network_anonymization_key),
                                  std::move(source_net_log),
                                  std::move(optional_parameters));
    case HostMappingRules::RewriteResult::kInvalidRewrite:
      // Treat any invalid mapping as if it was "~NOTFOUND" (which should itself
      // result in `kInvalidRewrite`).
      return CreateFailingRequest(ERR_NAME_NOT_RESOLVED);
    case HostMappingRules::RewriteResult::kNoMatchingRule:
      return impl_->CreateRequest(
          std::move(host), std::move(network_anonymization_key),
          std::move(source_net_log), std::move(optional_parameters));
  }
}

std::unique_ptr<HostResolver::ResolveHostRequest>
MappedHostResolver::CreateRequest(
    const HostPortPair& host,
    const NetworkAnonymizationKey& network_anonymization_key,
    const NetLogWithSource& source_net_log,
    const absl::optional<ResolveHostParameters>& optional_parameters) {
  HostPortPair rewritten = host;
  rules_.RewriteHost(&rewritten);

  if (rewritten.host() == "~NOTFOUND")
    return CreateFailingRequest(ERR_NAME_NOT_RESOLVED);

  return impl_->CreateRequest(rewritten, network_anonymization_key,
                              source_net_log, optional_parameters);
}

std::unique_ptr<HostResolver::ProbeRequest>
MappedHostResolver::CreateDohProbeRequest() {
  return impl_->CreateDohProbeRequest();
}

HostCache* MappedHostResolver::GetHostCache() {
  return impl_->GetHostCache();
}

base::Value::Dict MappedHostResolver::GetDnsConfigAsValue() const {
  return impl_->GetDnsConfigAsValue();
}

void MappedHostResolver::SetRequestContext(URLRequestContext* request_context) {
  impl_->SetRequestContext(request_context);
}

HostResolverManager* MappedHostResolver::GetManagerForTesting() {
  return impl_->GetManagerForTesting();
}

}  // namespace net
