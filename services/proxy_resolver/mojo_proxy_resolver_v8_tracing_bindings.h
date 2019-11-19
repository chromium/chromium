// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PROXY_RESOLVER_MOJO_PROXY_RESOLVER_V8_TRACING_BINDINGS_H_
#define SERVICES_PROXY_RESOLVER_MOJO_PROXY_RESOLVER_V8_TRACING_BINDINGS_H_

#include <memory>
#include <string>
#include <utility>

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_checker.h"
#include "net/base/address_family.h"
#include "net/base/host_port_pair.h"
#include "net/base/network_isolation_key.h"
#include "net/dns/host_resolver.h"
#include "net/log/net_log_with_source.h"
#include "net/proxy_resolution/proxy_resolve_dns_operation.h"
#include "services/proxy_resolver/host_resolver_mojo.h"
#include "services/proxy_resolver/proxy_host_resolver.h"
#include "services/proxy_resolver/proxy_resolver_v8_tracing.h"
#include "services/proxy_resolver/public/mojom/proxy_resolver.mojom.h"

namespace proxy_resolver {

// An implementation of ProxyResolverV8Tracing::Bindings that forwards requests
// onto a Client mojo interface. Alert() and OnError() may be called from any
// thread; when they are called from another thread, the calls are proxied to
// the origin task runner. GetHostResolver() and GetNetLogWithSource() may only
// be called from the origin task runner.
template <typename Client>
class MojoProxyResolverV8TracingBindings
    : public ProxyResolverV8Tracing::Bindings,
      public HostResolverMojo::Impl {
 public:
  explicit MojoProxyResolverV8TracingBindings(Client* client)
      : client_(client), host_resolver_(this) {
    DCHECK(client_);
  }

  // ProxyResolverV8Tracing::Bindings overrides.
  void Alert(const base::string16& message) override {
    DCHECK(thread_checker_.CalledOnValidThread());
    client_->Alert(base::UTF16ToUTF8(message));
  }

  void OnError(int line_number, const base::string16& message) override {
    DCHECK(thread_checker_.CalledOnValidThread());
    client_->OnError(line_number, base::UTF16ToUTF8(message));
  }

  ProxyHostResolver* GetHostResolver() override {
    DCHECK(thread_checker_.CalledOnValidThread());
    return &host_resolver_;
  }

  net::NetLogWithSource GetNetLogWithSource() override {
    DCHECK(thread_checker_.CalledOnValidThread());
    return net::NetLogWithSource();
  }

 private:
  // HostResolverMojo::Impl override.
  void ResolveDns(
      const std::string& hostname,
      net::ProxyResolveDnsOperation operation,
      const net::NetworkIsolationKey& network_isolation_key,
      mojo::PendingRemote<mojom::HostResolverRequestClient> client) override {
    DCHECK(thread_checker_.CalledOnValidThread());
    client_->ResolveDns(hostname, operation, network_isolation_key,
                        std::move(client));
  }

  base::ThreadChecker thread_checker_;
  Client* const client_;
  HostResolverMojo host_resolver_;
};

}  // namespace proxy_resolver

#endif  // SERVICES_PROXY_RESOLVER_MOJO_PROXY_RESOLVER_V8_TRACING_BINDINGS_H_
