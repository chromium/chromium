// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy_resolution/proxy_list.h"

#include <algorithm>

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/notreached.h"
#include "base/strings/string_tokenizer.h"
#include "base/time/time.h"
#include "base/values.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_server.h"
#include "net/base/proxy_string_util.h"
#include "net/log/net_log.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_with_source.h"

using base::TimeTicks;

namespace net {

ProxyList::ProxyList() = default;

ProxyList::ProxyList(const ProxyList& other) = default;

ProxyList::ProxyList(ProxyList&& other) = default;

ProxyList& ProxyList::operator=(const ProxyList& other) = default;

ProxyList& ProxyList::operator=(ProxyList&& other) = default;

ProxyList::~ProxyList() = default;

void ProxyList::Set(const std::string& proxy_uri_list) {
  Clear();
  base::StringTokenizer str_tok(proxy_uri_list, ";");
  while (str_tok.GetNext()) {
    ProxyChain chain =
        ProxyUriToProxyChain(str_tok.token_piece(), ProxyServer::SCHEME_HTTP);
    AddProxyChain(std::move(chain));
  }
}

void ProxyList::SetSingleProxyChain(ProxyChain proxy_chain) {
  Clear();
  AddProxyChain(std::move(proxy_chain));
}

void ProxyList::SetSingleProxyServer(ProxyServer proxy_server) {
  Clear();
  AddProxyServer(std::move(proxy_server));
}

void ProxyList::AddProxyChain(ProxyChain proxy_chain) {
  // Silently discard malformed inputs.
  if (proxy_chain.IsValid()) {
    proxy_chains_.emplace_back(std::move(proxy_chain));
  }
}

void ProxyList::AddProxyServer(ProxyServer proxy_server) {
  AddProxyChain(ProxyChain(std::move(proxy_server)));
}

void ProxyList::DeprioritizeBadProxyChains(
    const ProxyRetryInfoMap& proxy_retry_info,
    bool remove_bad_proxy_chains) {
  // Partition the proxy list in two:
  //   (1) the known bad proxy chains
  //   (2) everything else
  std::vector<ProxyChain> good_chains;
  std::vector<ProxyChain> bad_chains_to_try;

  for (const auto& proxy_chain : proxy_chains_) {
    if (auto bad_info = proxy_retry_info.find(proxy_chain);
        bad_info != proxy_retry_info.end() &&
        bad_info->second.bad_until >= TimeTicks::Now()) {
      // This proxy is bad and it's not time to retry.
      bad_chains_to_try.push_back(proxy_chain);
      continue;
    }
    good_chains.push_back(proxy_chain);
  }

  // "proxy_chains_ = good_chains + bad_proxies"
  proxy_chains_.swap(good_chains);
  if (!remove_bad_proxy_chains) {
    proxy_chains_.insert(proxy_chains_.end(), bad_chains_to_try.begin(),
                         bad_chains_to_try.end());
  }
}

void ProxyList::RemoveProxiesWithoutScheme(int scheme_bit_field) {
  std::erase_if(proxy_chains_, [&](const ProxyChain& chain) {
    auto& proxy_servers = chain.proxy_servers();
    // Remove the chain if any of the component servers does not match
    // at least one scheme in `scheme_bit_field`.
    return std::any_of(proxy_servers.begin(), proxy_servers.end(),
                       [&](const ProxyServer& server) {
                         return !(scheme_bit_field & server.scheme());
                       });
  });
}

void ProxyList::Clear() {
  proxy_chains_.clear();
}

bool ProxyList::IsEmpty() const {
  return proxy_chains_.empty();
}

size_t ProxyList::size() const {
  return proxy_chains_.size();
}

// Returns true if |*this| lists the same proxy chains as |other|.
bool ProxyList::Equals(const ProxyList& other) const {
  if (size() != other.size())
    return false;
  return proxy_chains_ == other.proxy_chains_;
}

const ProxyChain& ProxyList::First() const {
  CHECK(!proxy_chains_.empty());
  return proxy_chains_[0];
}

const std::vector<ProxyChain>& ProxyList::AllChains() const {
  return proxy_chains_;
}

void ProxyList::SetFromPacString(const std::string& pac_string) {
  Clear();
  base::StringTokenizer entry_tok(pac_string, ";");
  while (entry_tok.GetNext()) {
    ProxyChain proxy_chain =
        PacResultElementToProxyChain(entry_tok.token_piece());
    if (proxy_chain.IsValid()) {
      proxy_chains_.emplace_back(proxy_chain);
    }
  }

  // If we failed to parse anything from the PAC results list, fallback to
  // DIRECT (this basically means an error in the PAC script).
  if (proxy_chains_.empty()) {
    proxy_chains_.push_back(ProxyChain::Direct());
  }
}

std::string ProxyList::ToPacString() const {
  std::string proxy_list;
  for (const ProxyChain& proxy_chain : proxy_chains_) {
    if (!proxy_list.empty()) {
      proxy_list += ";";
    }
    CHECK(!proxy_chain.is_multi_proxy());
    proxy_list += proxy_chain.is_direct()
                      ? "DIRECT"
                      : ProxyServerToPacResultElement(proxy_chain.First());
  }
  return proxy_list;
}

std::string ProxyList::ToDebugString() const {
  std::string proxy_list;

  for (const ProxyChain& proxy_chain : proxy_chains_) {
    if (!proxy_list.empty()) {
      proxy_list += ";";
    }
    if (proxy_chain.is_multi_proxy()) {
      proxy_list += proxy_chain.ToDebugString();
    } else {
      proxy_list += proxy_chain.is_direct()
                        ? "DIRECT"
                        : ProxyServerToPacResultElement(proxy_chain.First());
    }
  }
  return proxy_list;
}

base::Value ProxyList::ToValue() const {
  base::Value::List list;
  for (const auto& proxy_chain : proxy_chains_) {
    if (proxy_chain.is_direct()) {
      list.Append("direct://");
    } else {
      list.Append(proxy_chain.ToDebugString());
    }
  }
  return base::Value(std::move(list));
}

bool ProxyList::Fallback(ProxyRetryInfoMap* proxy_retry_info,
                         int net_error,
                         const NetLogWithSource& net_log) {
  CHECK(!proxy_chains_.empty());

  auto first_chain = std::move(proxy_chains_.front());
  proxy_chains_.erase(proxy_chains_.begin());

  if (!first_chain.is_direct()) {
    net_log.AddEventWithStringParams(NetLogEventType::PROXY_LIST_FALLBACK,
                                     "bad_proxy_chain",
                                     first_chain.ToDebugString());

    // By default, proxy chains are not retried for 5 minutes.
    static constexpr base::TimeDelta kDefaultRetryDelay = base::Minutes(5);
    TimeTicks bad_until = TimeTicks::Now() + kDefaultRetryDelay;
    auto iter = proxy_retry_info->find(first_chain);
    if (iter == proxy_retry_info->end() || bad_until > iter->second.bad_until) {
      ProxyRetryInfo retry_info;
      retry_info.bad_until = bad_until;
      retry_info.net_error = net_error;
      (*proxy_retry_info)[std::move(first_chain)] = std::move(retry_info);
    }
  }

  return !proxy_chains_.empty();
}

}  // namespace net
