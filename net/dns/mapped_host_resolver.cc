// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/mapped_host_resolver.h"

#include <string>
#include <utility>

#include "base/strings/string_util.h"
#include "base/values.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"

namespace net {

MappedHostResolver::MappedHostResolver(std::unique_ptr<HostResolver> impl)
    : impl_(std::move(impl)) {}

MappedHostResolver::~MappedHostResolver() = default;

void MappedHostResolver::OnShutdown() {
  impl_->OnShutdown();
}

std::unique_ptr<HostResolver::ResolveHostRequest>
MappedHostResolver::CreateRequest(
    const HostPortPair& host,
    const NetworkIsolationKey& network_isolation_key,
    const NetLogWithSource& source_net_log,
    const base::Optional<ResolveHostParameters>& optional_parameters) {
  HostPortPair rewritten = host;
  rules_.RewriteHost(&rewritten);

  if (rewritten.host() == "~NOTFOUND")
    return CreateFailingRequest(ERR_NAME_NOT_RESOLVED);

  return impl_->CreateRequest(rewritten, network_isolation_key, source_net_log,
                              optional_parameters);
}

std::unique_ptr<HostResolver::ProbeRequest>
MappedHostResolver::CreateDohProbeRequest() {
  return impl_->CreateDohProbeRequest();
}

HostCache* MappedHostResolver::GetHostCache() {
  return impl_->GetHostCache();
}

std::unique_ptr<base::Value> MappedHostResolver::GetDnsConfigAsValue() const {
  return impl_->GetDnsConfigAsValue();
}

void MappedHostResolver::SetRequestContext(URLRequestContext* request_context) {
  impl_->SetRequestContext(request_context);
}

HostResolverManager* MappedHostResolver::GetManagerForTesting() {
  return impl_->GetManagerForTesting();
}

}  // namespace net
