// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

ProxyList::~ProxyList() {
#if DCHECK_IS_ON()
  // Validate that the result of `UpdateProxyServers()` is equivalent to the
  // incremental updates made in other methods.
  auto proxy_servers = std::move(proxy_servers_);
  UpdateProxyServers();
  CHECK(proxy_servers == proxy_servers_);
#endif
}

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
    if (!proxy_chain.is_multi_proxy() && proxy_servers_.has_value()) {
      proxy_servers_->push_back(proxy_chain.proxy_server());
    } else {
      proxy_servers_ = absl::nullopt;
    }
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
    // TODO(crbug.com/1491092): Store chains in ProxyRetryInfo.
    auto bad_info =
        proxy_retry_info.find(ProxyServerToProxyUri(iter->proxy_server()));
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

  UpdateProxyServers();
}

void ProxyList::RemoveProxiesWithoutScheme(int scheme_bit_field) {
  for (auto it = proxy_chains_.begin(); it != proxy_chains_.end();) {
    // TODO(crbug.com/1491092): Iterate over all servers in the chain.
    if (!(scheme_bit_field & it->proxy_server().scheme())) {
      it = proxy_chains_.erase(it);
      continue;
    }
    ++it;
  }

  UpdateProxyServers();
}

void ProxyList::Clear() {
  proxy_chains_.clear();
  UpdateProxyServers();
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
  // `proxy_servers_` is just a cache, so is not part of the comparison.
  return proxy_chains_ == other.proxy_chains_;
}

const ProxyServer& ProxyList::Get() const {
  CHECK(!proxy_chains_.empty());
  return proxy_servers_->front();
}

const ProxyChain& ProxyList::First() const {
  CHECK(!proxy_chains_.empty());
  return proxy_chains_[0];
}

const std::vector<ProxyServer>& ProxyList::GetAll() const {
  CHECK(proxy_servers_.has_value())
      << "ProxyList contains multi-proxy ProxyChains";
  return *proxy_servers_;
}

const std::vector<ProxyChain>& ProxyList::AllChains() const {
  return proxy_chains_;
}

void ProxyList::SetFromPacString(const std::string& pac_string) {
  Clear();
  base::StringTokenizer entry_tok(pac_string, ";");
  while (entry_tok.GetNext()) {
    // TODO(crbug.com/1491092): Parse multi-proxy chains.
    ProxyServer proxy_server =
        PacResultElementToProxyServer(entry_tok.token_piece());
    if (proxy_server.is_valid()) {
      proxy_chains_.emplace_back(proxy_server);
    }
  }

  // If we failed to parse anything from the PAC results list, fallback to
  // DIRECT (this basically means an error in the PAC script).
  if (proxy_chains_.empty()) {
    proxy_chains_.push_back(ProxyChain::Direct());
  }

  UpdateProxyServers();
}

std::string ProxyList::ToPacString() const {
  std::string proxy_list;
  auto iter = proxy_chains_.begin();
  for (; iter != proxy_chains_.end(); ++iter) {
    if (!proxy_list.empty())
      proxy_list += ";";
    // TODO(crbug.com/1491092): Figure out how to represent a multi-proxy chain.
    proxy_list += ProxyServerToPacResultElement(iter->proxy_server());
  }
  return proxy_list.empty() ? std::string() : proxy_list;
}

base::Value ProxyList::ToValue() const {
  base::Value::List list;
  for (const auto& proxy_chain : proxy_chains_) {
    // TODO(crbug.com/1491092): Determine a representation for proxy chains in
    // Values and use that for chains of length greater than one.
    list.Append(ProxyServerToProxyUri(proxy_chain.proxy_server()));
  }
  return base::Value(std::move(list));
}

bool ProxyList::Fallback(ProxyRetryInfoMap* proxy_retry_info,
                         int net_error,
                         const NetLogWithSource& net_log) {
  if (proxy_chains_.empty()) {
    NOTREACHED();
    return false;
  }
  // By default, proxies are not retried for 5 minutes.
  UpdateRetryInfoOnFallback(proxy_retry_info, base::Minutes(5), true,
                            std::vector<ProxyServer>(), net_error, net_log);

  // Remove this proxy from our list.
  proxy_chains_.erase(proxy_chains_.begin());
  UpdateProxyServers();
  return !proxy_chains_.empty();
}

void ProxyList::AddProxyChainToRetryList(
    ProxyRetryInfoMap* proxy_retry_info,
    base::TimeDelta retry_delay,
    bool try_while_bad,
    const ProxyChain& proxy_chain_to_retry,
    int net_error,
    const NetLogWithSource& net_log) const {
  // Mark this proxy as bad.
  TimeTicks bad_until = TimeTicks::Now() + retry_delay;
  // TODO(crbug.com/1491092): Key retry info by proxy chain.
  std::string proxy_key =
      ProxyServerToProxyUri(proxy_chain_to_retry.proxy_server());
  auto iter = proxy_retry_info->find(proxy_key);
  if (iter == proxy_retry_info->end() || bad_until > iter->second.bad_until) {
    ProxyRetryInfo retry_info;
    retry_info.current_delay = retry_delay;
    retry_info.bad_until = bad_until;
    retry_info.try_while_bad = try_while_bad;
    retry_info.net_error = net_error;
    (*proxy_retry_info)[proxy_key] = retry_info;
  }
  net_log.AddEventWithStringParams(NetLogEventType::PROXY_LIST_FALLBACK,
                                   "bad_proxy", proxy_key);
}

void ProxyList::UpdateRetryInfoOnFallback(
    ProxyRetryInfoMap* proxy_retry_info,
    base::TimeDelta retry_delay,
    bool reconsider,
    // TODO(crbug.com/1491092): Take a vector of ProxyChains instead.
    const std::vector<ProxyServer>& additional_proxies_to_bypass,
    int net_error,
    const NetLogWithSource& net_log) const {
  DCHECK(!retry_delay.is_zero());

  if (proxy_chains_.empty()) {
    NOTREACHED();
    return;
  }

  auto& first_chain = proxy_chains_[0];
  if (!first_chain.is_direct()) {
    AddProxyChainToRetryList(proxy_retry_info, retry_delay, reconsider,
                             first_chain, net_error, net_log);
    // If any additional proxies to bypass are specified, add to the retry map
    // as well.
    for (const ProxyServer& additional_proxy : additional_proxies_to_bypass) {
      AddProxyChainToRetryList(proxy_retry_info, retry_delay, reconsider,
                               ProxyChain(additional_proxy), net_error,
                               net_log);
    }
  }
}

void ProxyList::UpdateProxyServers() {
  if (proxy_chains_.empty()) {
    proxy_servers_ = std::vector<ProxyServer>();
    return;
  }

  std::vector<ProxyServer> proxy_servers;
  for (auto& it : proxy_chains_) {
    if (it.is_multi_proxy()) {
      proxy_servers_.reset();
      return;
    }
    proxy_servers.push_back(it.proxy_server());
  }
  proxy_servers_ = std::move(proxy_servers);
}

}  // namespace net
