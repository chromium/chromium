// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/proxy_chain.h"

#include <ostream>
#include <vector>

#include "base/check.h"
#include "base/no_destructor.h"
#include "base/pickle.h"
#include "base/ranges/algorithm.h"
#include "base/strings/stringprintf.h"
#include "build/buildflag.h"
#include "net/base/proxy_server.h"
#include "net/base/proxy_string_util.h"
#include "net/net_buildflags.h"

namespace net {

namespace {
bool ShouldAllowQuicForAllChains() {
  bool should_allow = false;

#if BUILDFLAG(ENABLE_QUIC_PROXY_SUPPORT)
  should_allow = true;
#endif  // BUILDFLAG(ENABLE_QUIC_PROXY_SUPPORT)

  return should_allow;
}
}  // namespace

ProxyChain::ProxyChain() {
  proxy_server_list_ = std::nullopt;
}

ProxyChain::ProxyChain(const ProxyChain& other) = default;
ProxyChain::ProxyChain(ProxyChain&& other) noexcept = default;

ProxyChain& ProxyChain::operator=(const ProxyChain& other) = default;
ProxyChain& ProxyChain::operator=(ProxyChain&& other) noexcept = default;
ProxyChain::~ProxyChain() = default;

ProxyChain::ProxyChain(ProxyServer proxy_server)
    : ProxyChain(std::vector<ProxyServer>{std::move(proxy_server)}) {}

ProxyChain::ProxyChain(ProxyServer::Scheme scheme,
                       const HostPortPair& host_port_pair)
    : ProxyChain(ProxyServer(scheme, host_port_pair)) {}

ProxyChain::ProxyChain(std::vector<ProxyServer> proxy_server_list)
    : proxy_server_list_(std::move(proxy_server_list)) {
  if (!IsValidInternal()) {
    proxy_server_list_ = std::nullopt;
  }
}

bool ProxyChain::InitFromPickle(base::PickleIterator* pickle_iter) {
  if (!pickle_iter->ReadInt(&ip_protection_chain_id_)) {
    return false;
  }
  size_t chain_length = 0;
  if (!pickle_iter->ReadLength(&chain_length)) {
    return false;
  }

  std::vector<ProxyServer> proxy_server_list;
  for (size_t i = 0; i < chain_length; ++i) {
    proxy_server_list.push_back(ProxyServer::CreateFromPickle(pickle_iter));
  }
  proxy_server_list_ = std::move(proxy_server_list);
  return true;
}

void ProxyChain::Persist(base::Pickle* pickle) const {
  DCHECK(IsValid());
  pickle->WriteInt(ip_protection_chain_id_);
  if (length() > static_cast<size_t>(INT_MAX) - 1) {
    pickle->WriteInt(0);
    return;
  }
  pickle->WriteInt(static_cast<int>(length()));
  for (const auto& proxy_server : proxy_server_list_.value()) {
    proxy_server.Persist(pickle);
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
      ProxyChain({proxy_server_list_->begin(), proxy_server_list_->end() - 1},
                 ip_protection_chain_id_);
  return std::make_pair(new_chain, std::ref(proxy_server_list_->back()));
}

ProxyChain ProxyChain::Prefix(size_t len) const {
  DCHECK(IsValid());
  DCHECK_LE(len, length());
  return ProxyChain(
      {proxy_server_list_->begin(), proxy_server_list_->begin() + len},
      ip_protection_chain_id_);
}

const ProxyServer& ProxyChain::First() const {
  DCHECK(IsValid());
  DCHECK_NE(length(), 0u);
  return proxy_server_list_->front();
}

const ProxyServer& ProxyChain::Last() const {
  DCHECK(IsValid());
  DCHECK_NE(length(), 0u);
  return proxy_server_list_->back();
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
  if (ip_protection_chain_id_ == 0) {
    debug_string += " (IP Protection)";
  } else if (ip_protection_chain_id_ >= 0) {
    debug_string += base::StringPrintf(" (IP Protection chain %d)",
                                       ip_protection_chain_id_);
  }
  return debug_string;
}

ProxyChain::ProxyChain(std::vector<ProxyServer> proxy_server_list,
                       int ip_protection_chain_id)
    : proxy_server_list_(std::move(proxy_server_list)),
      ip_protection_chain_id_(ip_protection_chain_id) {
  CHECK(IsValidInternal());
}

bool ProxyChain::IsValidInternal() const {
  if (!proxy_server_list_.has_value()) {
    return false;
  }
  if (is_direct()) {
    return true;
  }
  bool should_allow_quic =
      is_for_ip_protection() || ShouldAllowQuicForAllChains();
  if (is_single_proxy()) {
    bool is_valid = proxy_server_list_.value().at(0).is_valid();
    if (proxy_server_list_.value().at(0).is_quic()) {
      is_valid = is_valid && should_allow_quic;
    }
    return is_valid;
  }
  DCHECK(is_multi_proxy());

#if !BUILDFLAG(ENABLE_BRACKETED_PROXY_URIS)
  // A chain can only be multi-proxy in release builds if it is for ip
  // protection.
  if (!is_for_ip_protection() && is_multi_proxy()) {
    return false;
  }
#endif  // !BUILDFLAG(ENABLE_BRACKETED_PROXY_URIS)

  // Verify that the chain is zero or more SCHEME_QUIC servers followed by zero
  // or more SCHEME_HTTPS servers.
  bool seen_quic = false;
  bool seen_https = false;
  for (const auto& proxy_server : proxy_server_list_.value()) {
    if (proxy_server.is_quic()) {
      if (seen_https) {
        // SCHEME_QUIC cannot follow SCHEME_HTTPS.
        return false;
      }
      seen_quic = true;
    } else if (proxy_server.is_https()) {
      seen_https = true;
    } else {
      return false;
    }
  }

  // QUIC is only allowed for IP protection unless in debug builds where it is
  // generally available.
  return !seen_quic || should_allow_quic;
}

std::ostream& operator<<(std::ostream& os, const ProxyChain& proxy_chain) {
  return os << proxy_chain.ToDebugString();
}

}  // namespace net
