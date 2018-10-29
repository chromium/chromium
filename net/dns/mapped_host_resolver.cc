// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/mapped_host_resolver.h"

#include <utility>

#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"

namespace net {

class MappedHostResolver::AlwaysErrorRequestImpl
    : public HostResolver::ResolveHostRequest {
 public:
  explicit AlwaysErrorRequestImpl(int error) : error_(error) {}

  int Start(CompletionOnceCallback callback) override { return error_; }

  const base::Optional<AddressList>& GetAddressResults() const override {
    static base::NoDestructor<base::Optional<AddressList>> nullopt_address_list;
    return *nullopt_address_list;
  }

 private:
  const int error_;
};

MappedHostResolver::MappedHostResolver(std::unique_ptr<HostResolver> impl)
    : impl_(std::move(impl)) {}

MappedHostResolver::~MappedHostResolver() = default;

std::unique_ptr<HostResolver::ResolveHostRequest>
MappedHostResolver::CreateRequest(
    const HostPortPair& host,
    const NetLogWithSource& source_net_log,
    const base::Optional<ResolveHostParameters>& optional_parameters) {
  HostPortPair rewritten = host;
  rules_.RewriteHost(&rewritten);

  if (rewritten.host() == "~NOTFOUND")
    return std::make_unique<AlwaysErrorRequestImpl>(ERR_NAME_NOT_RESOLVED);

  return impl_->CreateRequest(rewritten, source_net_log, optional_parameters);
}

int MappedHostResolver::Resolve(const RequestInfo& original_info,
                                RequestPriority priority,
                                AddressList* addresses,
                                CompletionOnceCallback callback,
                                std::unique_ptr<Request>* request,
                                const NetLogWithSource& net_log) {
  RequestInfo info = original_info;
  int rv = ApplyRules(&info);
  if (rv != OK)
    return rv;

  return impl_->Resolve(info, priority, addresses, std::move(callback), request,
                        net_log);
}

int MappedHostResolver::ResolveFromCache(const RequestInfo& original_info,
                                         AddressList* addresses,
                                         const NetLogWithSource& net_log) {
  RequestInfo info = original_info;
  int rv = ApplyRules(&info);
  if (rv != OK)
    return rv;

  return impl_->ResolveFromCache(info, addresses, net_log);
}

int MappedHostResolver::ResolveStaleFromCache(
    const RequestInfo& original_info,
    AddressList* addresses,
    HostCache::EntryStaleness* stale_info,
    const NetLogWithSource& net_log) {
  RequestInfo info = original_info;
  int rv = ApplyRules(&info);
  if (rv != OK)
    return rv;

  return impl_->ResolveStaleFromCache(info, addresses, stale_info, net_log);
}

void MappedHostResolver::SetDnsClientEnabled(bool enabled) {
  impl_->SetDnsClientEnabled(enabled);
}

HostCache* MappedHostResolver::GetHostCache() {
  return impl_->GetHostCache();
}

bool MappedHostResolver::HasCached(base::StringPiece hostname,
                                   HostCache::Entry::Source* source_out,
                                   HostCache::EntryStaleness* stale_out) const {
  return impl_->HasCached(hostname, source_out, stale_out);
}

std::unique_ptr<base::Value> MappedHostResolver::GetDnsConfigAsValue() const {
  return impl_->GetDnsConfigAsValue();
}

void MappedHostResolver::SetNoIPv6OnWifi(bool no_ipv6_on_wifi) {
  impl_->SetNoIPv6OnWifi(no_ipv6_on_wifi);
}

bool MappedHostResolver::GetNoIPv6OnWifi() {
  return impl_->GetNoIPv6OnWifi();
}

void MappedHostResolver::SetDnsConfigOverrides(
    const DnsConfigOverrides& overrides) {
  impl_->SetDnsConfigOverrides(overrides);
}

void MappedHostResolver::SetRequestContext(URLRequestContext* request_context) {
  impl_->SetRequestContext(request_context);
}

const std::vector<DnsConfig::DnsOverHttpsServerConfig>*
MappedHostResolver::GetDnsOverHttpsServersForTesting() const {
  return impl_->GetDnsOverHttpsServersForTesting();
}

int MappedHostResolver::ApplyRules(RequestInfo* info) const {
  HostPortPair host_port(info->host_port_pair());
  if (rules_.RewriteHost(&host_port)) {
    if (host_port.host() == "~NOTFOUND")
      return ERR_NAME_NOT_RESOLVED;
    info->set_host_port_pair(host_port);
  }
  return OK;
}

}  // namespace net
