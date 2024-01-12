// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/system_dns_resolution/system_dns_resolver.h"

#include <algorithm>
#include <memory>

#include "base/functional/bind.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "net/base/address_list.h"
#include "net/base/net_errors.h"
#include "net/dns/dns_names_util.h"
#include "net/dns/host_resolver_system_task.h"
#include "net/log/net_log_source_type.h"
#include "net/log/net_log_with_source.h"
#include "services/network/public/mojom/system_dns_resolution.mojom.h"

namespace content {

namespace {
void ForwardSystemDnsResults(
    std::unique_ptr<net::HostResolverSystemTask>,
    network::mojom::SystemDnsResolver::ResolveCallback callback,
    const net::AddressList& addr_list,
    int os_error,
    int net_error) {
  std::move(callback).Run(addr_list, os_error, net_error);
}
}  // namespace

SystemDnsResolverMojoImpl::SystemDnsResolverMojoImpl() {
  net::EnsureSystemHostResolverCallReady();
}

// network::mojom::SystemDnsResolver impl:
void SystemDnsResolverMojoImpl::Resolve(
    const std::optional<std::string>& hostname,
    net::AddressFamily addr_family,
    int32_t flags,
    uint64_t network,
    ResolveCallback callback) {
  if (hostname && !net::dns_names_util::IsValidDnsName(*hostname)) {
    std::move(callback).Run(net::AddressList(), 0, net::ERR_NAME_NOT_RESOLVED);
    return;
  }

  auto wrapped_mojo_callback = mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      std::move(callback), net::AddressList(), 0, net::ERR_ABORTED);

  // TODO(mpdenton): possibly add netlogging.
  std::unique_ptr<net::HostResolverSystemTask> system_task =
      std::make_unique<net::HostResolverSystemTask>(
          hostname, addr_family, flags,
          net::HostResolverSystemTask::Params(nullptr, 0),
          net::NetLogWithSource(), network);

  // Let the callback own the HostResolverSystemTask, and pass the callback
  // to HostResolverSystemTask so it deletes itself on completion.
  net::HostResolverSystemTask* system_task_ptr = system_task.get();
  system_task_ptr->Start(base::BindOnce(&ForwardSystemDnsResults,
                                        std::move(system_task),
                                        std::move(wrapped_mojo_callback)));
}

}  // namespace content
