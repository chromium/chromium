// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/dns/dns_api.h"

#include "base/bind.h"
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

DnsResolveFunction::~DnsResolveFunction() {}

ExtensionFunction::ResponseAction DnsResolveFunction::Run() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::unique_ptr<Resolve::Params> params(Resolve::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  // Yes, we are passing zero as the port. There are some interesting but not
  // presently relevant reasons why HostResolver asks for the port of the
  // hostname you'd like to resolve, even though it doesn't use that value in
  // determining its answer.
  net::HostPortPair host_port_pair(params->hostname, 0);
  url::Origin origin = extension_->origin();
  browser_context()
      ->GetDefaultStoragePartition()
      ->GetNetworkContext()
      ->ResolveHost(host_port_pair, net::NetworkIsolationKey(origin, origin),
                    nullptr, receiver_.BindNewPipeAndPassRemote());
  receiver_.set_disconnect_handler(
      base::BindOnce(&DnsResolveFunction::OnComplete, base::Unretained(this),
                     net::ERR_NAME_NOT_RESOLVED,
                     net::ResolveErrorInfo(net::ERR_FAILED), absl::nullopt));

  // Balanced in OnComplete().
  AddRef();
  return RespondLater();
}

void DnsResolveFunction::OnComplete(
    int result,
    const net::ResolveErrorInfo& resolve_error_info,
    const absl::optional<net::AddressList>& resolved_addresses) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  receiver_.reset();

  ResolveCallbackResolveInfo resolve_info;
  resolve_info.result_code = resolve_error_info.error;
  if (result == net::OK) {
    DCHECK(resolved_addresses.has_value() && !resolved_addresses->empty());
    resolve_info.address = std::make_unique<std::string>(
        resolved_addresses->front().ToStringWithoutPort());
  }

  Respond(ArgumentList(Resolve::Results::Create(resolve_info)));

  Release();  // Added in Run().
}

}  // namespace extensions
