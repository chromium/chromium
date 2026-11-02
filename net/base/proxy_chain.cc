// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/proxy_chain.h"

#include <algorithm>
#include <ostream>
#include <vector>

#include "base/check.h"
#include "base/no_destructor.h"
#include "base/pickle.h"
#include "base/strings/strcat.h"
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

ProxyChain::ProxyChain() = default;

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

// static
std::optional<ProxyChain> ProxyChain::InitFromPickle(
    base::PickleIterator& pickle_iter) {
  // Read and discard the deprecated `ip_protection_chain_id` field to preserve
  // backwards compatibility with ProxyChains persisted to the HTTP cache.
  int unused_ip_protection_chain_id;
  if (!pickle_iter.ReadInt(&unused_ip_protection_chain_id)) {
    return std::nullopt;
  }
  size_t chain_length = 0;
  if (!pickle_iter.ReadLength(&chain_length)) {
    return std::nullopt;
  }

  std::vector<ProxyServer> proxy_server_list;
  for (size_t i = 0; i < chain_length; ++i) {
    proxy_server_list.push_back(ProxyServer::CreateFromPickle(&pickle_iter));
  }

  ProxyChain chain(std::move(proxy_server_list));
  if (!chain.IsValid()) {
    return std::nullopt;
  }
  return chain;
}

void ProxyChain::Persist(base::Pickle* pickle) const {
  DCHECK(IsValid());
  // Write a dummy value (-1, formerly `kNotIpProtectionChainId`) for the
  // removed `ip_protection_chain_id_` field to preserve the pickle format for
  // ProxyChains persisted to the HTTP cache.
  pickle->WriteInt(-1);
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
                 opaque_data_);
  CHECK(new_chain.IsValid());
  return std::make_pair(std::move(new_chain),
                        std::ref(proxy_server_list_->back()));
}

ProxyChain ProxyChain::Prefix(size_t len) const {
  DCHECK(IsValid());
  DCHECK_LE(len, length());
  auto new_chain = ProxyChain(
      {proxy_server_list_->begin(), proxy_server_list_->begin() + len},
      opaque_data_);
  CHECK(new_chain.IsValid());
  return new_chain;
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
  debug_string = base::StrCat({"[", debug_string, "]"});

  if (opaque_data_.has_value()) {
    debug_string += base::StringPrintf(" (Opaque data %d)", *opaque_data_);
  }
  return debug_string;
}

ProxyChain::ProxyChain(std::vector<ProxyServer> proxy_server_list,
                       std::optional<int> opaque_data)
    : proxy_server_list_(std::move(proxy_server_list)),
      opaque_data_(opaque_data) {
  if (!IsValidInternal()) {
    *this = ProxyChain();
  }
}

bool ProxyChain::IsValidInternal() const {
  if (!proxy_server_list_.has_value()) {
    return false;
  }
  if (is_direct()) {
    return true;
  }
  bool should_allow_quic = ShouldAllowQuicForAllChains();
  if (is_single_proxy()) {
    bool is_valid = proxy_server_list_.value().at(0).is_valid();
    if (proxy_server_list_.value().at(0).is_quic()) {
      is_valid = is_valid && should_allow_quic;
    }
    return is_valid;
  }
  DCHECK(is_multi_proxy());

#if BUILDFLAG(ENABLE_BRACKETED_PROXY_URIS)
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

  return !seen_quic || should_allow_quic;
#else
  return false;
#endif  // BUILDFLAG(ENABLE_BRACKETED_PROXY_URIS)
}

std::ostream& operator<<(std::ostream& os, const ProxyChain& proxy_chain) {
  return os << proxy_chain.ToDebugString();
}

}  // namespace net
