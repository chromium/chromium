// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/dns/dns_api.h"

#include "base/functional/bind.h"
#include "base/values.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/common/api/dns.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/base/network_isolation_key.h"
#include "net/dns/public/resolve_error_info.h"
#include "url/origin.h"

using content::BrowserThread;
using extensions::api::dns::ResolveCallbackResolveInfo;

namespace Resolve = extensions::api::dns::Resolve;

namespace extensions {

DnsResolveFunction::DnsResolveFunction() = default;

DnsResolveFunction::~DnsResolveFunction() = default;

ExtensionFunction::ResponseAction DnsResolveFunction::Run() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::optional<Resolve::Params> params = Resolve::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  // Intentionally pass host only (no scheme or non-zero port) to only get a
  // basic resolution for the hostname itself.
  net::HostPortPair host_port_pair(params->hostname, 0);
  net::SchemefulSite site = net::SchemefulSite(extension_->origin());
  browser_context()
      ->GetDefaultStoragePartition()
      ->GetNetworkContext()
      ->ResolveHost(network::mojom::HostResolverHost::NewHostPortPair(
                        std::move(host_port_pair)),
                    net::NetworkAnonymizationKey::CreateSameSite(site), nullptr,
                    receiver_.BindNewPipeAndPassRemote());
  receiver_.set_disconnect_handler(base::BindOnce(
      &DnsResolveFunction::OnComplete, base::Unretained(this),
      net::ERR_NAME_NOT_RESOLVED, net::ResolveErrorInfo(net::ERR_FAILED),
      /*resolved_addresses=*/std::nullopt,
      /*endpoint_results_with_metadata=*/std::nullopt));

  // Balanced in OnComplete().
  AddRef();
  return RespondLater();
}

void DnsResolveFunction::OnComplete(
    int result,
    const net::ResolveErrorInfo& resolve_error_info,
    const std::optional<net::AddressList>& resolved_addresses,
    const std::optional<net::HostResolverEndpointResults>&
        endpoint_results_with_metadata) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  receiver_.reset();

  ResolveCallbackResolveInfo resolve_info;
  resolve_info.result_code = resolve_error_info.error;
  if (result == net::OK) {
    DCHECK(resolved_addresses.has_value() && !resolved_addresses->empty());
    resolve_info.address = resolved_addresses->front().ToStringWithoutPort();
  }

  Respond(ArgumentList(Resolve::Results::Create(resolve_info)));

  Release();  // Added in Run().
}

}  // namespace extensions
