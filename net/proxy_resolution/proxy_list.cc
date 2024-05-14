// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "net/proxy_resolution/proxy_list.h"

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
    AddProxyChain(chain);
  }
}

void ProxyList::SetSingleProxyChain(const ProxyChain& proxy_chain) {
  Clear();
  AddProxyChain(proxy_chain);
}

void ProxyList::SetSingleProxyServer(const ProxyServer& proxy_server) {
  Clear();
  AddProxyServer(proxy_server);
}

void ProxyList::AddProxyChain(const ProxyChain& proxy_chain) {
  // Silently discard malformed inputs.
  if (proxy_chain.IsValid()) {
    proxy_chains_.push_back(proxy_chain);
  }
}

void ProxyList::AddProxyServer(const ProxyServer& proxy_server) {
  AddProxyChain(ProxyChain(proxy_server));
}

void ProxyList::DeprioritizeBadProxyChains(
    const ProxyRetryInfoMap& proxy_retry_info) {
  // Partition the proxy list in two:
  //   (1) the known bad proxy chains
  //   (2) everything else
  std::vector<ProxyChain> good_chains;
  std::vector<ProxyChain> bad_chains_to_try;

  std::vector<ProxyChain>::const_iterator iter = proxy_chains_.begin();
  for (; iter != proxy_chains_.end(); ++iter) {
    auto bad_info = proxy_retry_info.find(*iter);
    if (bad_info != proxy_retry_info.end()) {
      // This proxy is bad. Check if it's time to retry.
      if (bad_info->second.bad_until >= TimeTicks::Now()) {
        // still invalid.
        if (bad_info->second.try_while_bad) {
          bad_chains_to_try.push_back(*iter);
        }
        continue;
      }
    }
    good_chains.push_back(*iter);
  }

  // "proxy_chains_ = good_chains + bad_proxies"
  proxy_chains_.swap(good_chains);
  proxy_chains_.insert(proxy_chains_.end(), bad_chains_to_try.begin(),
                       bad_chains_to_try.end());
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
  return proxy_list.empty() ? std::string() : proxy_list;
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
  if (proxy_chains_.empty()) {
    NOTREACHED_IN_MIGRATION();
    return false;
  }
  // By default, proxy chains are not retried for 5 minutes.
  UpdateRetryInfoOnFallback(proxy_retry_info, base::Minutes(5), true,
                            std::vector<ProxyChain>(), net_error, net_log);

  // Remove this proxy from our list.
  proxy_chains_.erase(proxy_chains_.begin());
  return !proxy_chains_.empty();
}

void ProxyList::AddProxyChainToRetryList(
    ProxyRetryInfoMap* proxy_retry_info,
    base::TimeDelta retry_delay,
    bool try_while_bad,
    const ProxyChain& proxy_chain_to_retry,
    int net_error,
    const NetLogWithSource& net_log) const {
  // Mark this proxy chain as bad.
  TimeTicks bad_until = TimeTicks::Now() + retry_delay;
  auto iter = proxy_retry_info->find(proxy_chain_to_retry);
  if (iter == proxy_retry_info->end() || bad_until > iter->second.bad_until) {
    ProxyRetryInfo retry_info;
    retry_info.current_delay = retry_delay;
    retry_info.bad_until = bad_until;
    retry_info.try_while_bad = try_while_bad;
    retry_info.net_error = net_error;
    (*proxy_retry_info)[proxy_chain_to_retry] = retry_info;
  }
  net_log.AddEventWithStringParams(NetLogEventType::PROXY_LIST_FALLBACK,
                                   "bad_proxy_chain",
                                   proxy_chain_to_retry.ToDebugString());
}

void ProxyList::UpdateRetryInfoOnFallback(
    ProxyRetryInfoMap* proxy_retry_info,
    base::TimeDelta retry_delay,
    bool reconsider,
    const std::vector<ProxyChain>& additional_proxies_to_bypass,
    int net_error,
    const NetLogWithSource& net_log) const {
  DCHECK(!retry_delay.is_zero());

  if (proxy_chains_.empty()) {
    NOTREACHED_IN_MIGRATION();
    return;
  }

  auto& first_chain = proxy_chains_[0];
  if (!first_chain.is_direct()) {
    AddProxyChainToRetryList(proxy_retry_info, retry_delay, reconsider,
                             first_chain, net_error, net_log);
    // If any additional proxies to bypass are specified, add to the retry map
    // as well.
    for (const ProxyChain& additional_proxy_chain :
         additional_proxies_to_bypass) {
      AddProxyChainToRetryList(
          proxy_retry_info, retry_delay, reconsider,
          ProxyChain(additional_proxy_chain.proxy_servers()), net_error,
          net_log);
    }
  }
}

}  // namespace net
