// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/proxy_chain.h"

#include <ostream>
#include <vector>

#include "base/check.h"
#include "base/no_destructor.h"
#include "net/base/proxy_server.h"
#include "net/base/proxy_string_util.h"

namespace net {

ProxyChain::ProxyChain() {
  proxy_server_list_ = std::nullopt;
}

ProxyChain::ProxyChain(const ProxyChain& other) = default;
ProxyChain::ProxyChain(ProxyChain&& other) noexcept = default;

ProxyChain& ProxyChain::operator=(const ProxyChain& other) = default;
// Note: We define this move assignment operator explicitly to make the
// `ForIpProtection()` method safer to use. Specifically, we want to prevent
// moving the `proxy_server_list_` in the event that self-assignment is
// occurring (i.e. "proxy_chain = std::move(proxy_chain).ForIpProtection()") or
// else the list of ProxyServers will get cleared.
ProxyChain& ProxyChain::operator=(ProxyChain&& other) noexcept {
  if (this != &other) {
    proxy_server_list_ = std::move(other.proxy_server_list_);
    is_for_ip_protection_ = other.is_for_ip_protection_;
  }
  return *this;
}
ProxyChain::~ProxyChain() = default;

// TODO(crbug.com/1491092): Remove is_direct() check when
// ProxyServer::SCHEME_DIRECT is deprecated.
ProxyChain::ProxyChain(ProxyServer proxy_server)
    : ProxyChain(!proxy_server.is_direct()
                     ? std::vector<ProxyServer>{std::move(proxy_server)}
                     : std::vector<ProxyServer>()) {}

ProxyChain::ProxyChain(ProxyServer::Scheme scheme,
                       const HostPortPair& host_port_pair)
    : ProxyChain({ProxyServer(scheme, host_port_pair)}) {}

ProxyChain::ProxyChain(std::vector<ProxyServer> proxy_server_list)
    : proxy_server_list_(std::move(proxy_server_list)) {
  if (!IsValidInternal()) {
    proxy_server_list_ = std::nullopt;
  }
}

const ProxyServer& ProxyChain::GetProxyServer(size_t chain_index) const {
  DCHECK(IsValid());
  CHECK_LT(chain_index, proxy_server_list_.value().size());
  return proxy_server_list_.value().at(chain_index);
}

const std::vector<ProxyServer>& ProxyChain::proxy_servers() const {
  DCHECK(IsValid());
  return proxy_server_list_.value();
}

const ProxyServer& ProxyChain::proxy_server() const {
  if (!proxy_server_list_.has_value()) {
    static base::NoDestructor<ProxyServer> invalid(ProxyServer::SCHEME_INVALID,
                                                   HostPortPair());
    return *invalid;
  } else if (proxy_server_list_.value().empty()) {
    static base::NoDestructor<ProxyServer> direct(ProxyServer::SCHEME_DIRECT,
                                                  HostPortPair());
    return *direct;
  }
  CHECK_EQ(1u, proxy_server_list_->size())
      << "Cannot call `proxy_server() on a ProxyChain with multiple proxies: "
      << ToDebugString();
  return proxy_server_list_.value().at(0);
}

ProxyChain&& ProxyChain::ForIpProtection() && {
  CHECK(IsValid());
  is_for_ip_protection_ = true;
  return std::move(*this);
}

std::string ProxyChain::ToDebugString() const {
  if (!IsValid()) {
    return "INVALID PROXY CHAIN";
  }
  std::string debug_string =
      proxy_server_list_.value().empty() ? "direct://" : "";
  for (const ProxyServer& proxy_server : proxy_server_list_.value()) {
    if (!debug_string.empty()) {
      debug_string += ", ";
    }
    debug_string += ProxyServerToProxyUri(proxy_server);
  }
  debug_string = "[" + debug_string + "]";
  if (is_for_ip_protection()) {
    debug_string += " (IP Protection)";
  }
  return debug_string;
}

// TODO(crbug.com/1491092): Remove is_direct() checks when
// ProxyServer::SCHEME_DIRECT is deprecated.
bool ProxyChain::IsValidInternal() const {
  if (!proxy_server_list_.has_value()) {
    return false;
  }
  if (is_single_proxy()) {
    return proxy_server_list_.value().at(0).is_valid() &&
           !proxy_server_list_.value().at(0).is_direct();
  }
  for (const auto& proxy_server : proxy_server_list_.value()) {
    if (!proxy_server.is_valid() || !proxy_server.is_https() ||
        proxy_server.is_direct()) {
      return false;
    }
  }
  return true;
}

std::ostream& operator<<(std::ostream& os, const ProxyChain& proxy_chain) {
  return os << proxy_chain.ToDebugString();
}

}  // namespace net
