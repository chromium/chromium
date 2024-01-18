// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/proxy_chain.h"

#include <ostream>
#include <vector>

#include "base/check.h"
#include "base/no_destructor.h"
#include "base/ranges/algorithm.h"
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

ProxyChain::ProxyChain(ProxyServer proxy_server)
    : ProxyChain(std::vector<ProxyServer>{std::move(proxy_server)}) {}

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

std::pair<ProxyChain, const ProxyServer&> ProxyChain::SplitLast() const {
  DCHECK(IsValid());
  DCHECK_NE(length(), 0u);
  ProxyChain new_chain =
      ProxyChain({proxy_server_list_->begin(), proxy_server_list_->end() - 1});
  new_chain.is_for_ip_protection_ = is_for_ip_protection_;
  return std::make_pair(new_chain, std::ref(proxy_server_list_->back()));
}

const ProxyServer& ProxyChain::Last() const {
  DCHECK(IsValid());
  DCHECK_NE(length(), 0u);
  return proxy_server_list_->back();
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

bool ProxyChain::IsValidInternal() const {
  if (!proxy_server_list_.has_value()) {
    return false;
  }
  if (is_single_proxy()) {
    return proxy_server_list_.value().at(0).is_valid();
  }
  return base::ranges::all_of(
      proxy_server_list_.value(), [](const auto& proxy_server) {
        return proxy_server.is_valid() && proxy_server.is_https();
      });
}

std::ostream& operator<<(std::ostream& os, const ProxyChain& proxy_chain) {
  return os << proxy_chain.ToDebugString();
}

}  // namespace net
