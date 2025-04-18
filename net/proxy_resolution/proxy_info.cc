// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy_resolution/proxy_info.h"

#include "net/proxy_resolution/proxy_retry_info.h"

namespace net {

// static
ProxyInfo ProxyInfo::Direct() {
  ProxyInfo proxy_info;
  proxy_info.UseDirect();
  return proxy_info;
}

ProxyInfo::ProxyInfo() = default;

ProxyInfo::ProxyInfo(const ProxyInfo& other) = default;

ProxyInfo::~ProxyInfo() = default;

void ProxyInfo::Use(const ProxyInfo& other) {
  proxy_resolve_start_time_ = other.proxy_resolve_start_time_;
  proxy_resolve_end_time_ = other.proxy_resolve_end_time_;
  proxy_list_ = other.proxy_list_;
  proxy_retry_info_ = other.proxy_retry_info_;
  did_bypass_proxy_ = other.did_bypass_proxy_;
}

void ProxyInfo::UseDirect() {
  Reset();
  proxy_list_.SetSingleProxyChain(ProxyChain::Direct());
}

void ProxyInfo::UseDirectWithBypassedProxy() {
  UseDirect();
  did_bypass_proxy_ = true;
}

void ProxyInfo::UseNamedProxy(const std::string& proxy_uri_list) {
  Reset();
  proxy_list_.Set(proxy_uri_list);
}

void ProxyInfo::UseProxyChain(const ProxyChain& proxy_chain) {
  Reset();
  proxy_list_.SetSingleProxyChain(proxy_chain);
}

void ProxyInfo::UsePacString(const std::string& pac_string) {
  Reset();
  proxy_list_.SetFromPacString(pac_string);
}

void ProxyInfo::UseProxyList(const ProxyList& proxy_list) {
  Reset();
  proxy_list_ = proxy_list;
}

void ProxyInfo::OverrideProxyList(const ProxyList& proxy_list) {
  proxy_list_ = proxy_list;
}

bool ProxyInfo::ContainsMultiProxyChain() const {
  auto& proxy_chains = proxy_list_.AllChains();
  return std::any_of(proxy_chains.begin(), proxy_chains.end(),
                     [](const ProxyChain& proxy_chain) {
                       return proxy_chain.is_multi_proxy();
                     });
}

std::string ProxyInfo::ToPacString() const {
  return proxy_list_.ToPacString();
}

bool ProxyInfo::is_for_ip_protection() const {
  if (is_empty()) {
    return false;
  }
  return proxy_chain().is_for_ip_protection();
}

std::string ProxyInfo::ToDebugString() const {
  return proxy_list_.ToDebugString();
}

bool ProxyInfo::Fallback(int net_error, const NetLogWithSource& net_log) {
  return proxy_list_.Fallback(&proxy_retry_info_, net_error, net_log);
}

void ProxyInfo::DeprioritizeBadProxyChains(
    const ProxyRetryInfoMap& proxy_retry_info) {
  proxy_list_.DeprioritizeBadProxyChains(proxy_retry_info);
}

void ProxyInfo::RemoveProxiesWithoutScheme(int scheme_bit_field) {
  proxy_list_.RemoveProxiesWithoutScheme(scheme_bit_field);
}

void ProxyInfo::Reset() {
  proxy_resolve_start_time_ = base::TimeTicks();
  proxy_resolve_end_time_ = base::TimeTicks();
  proxy_list_.Clear();
  proxy_retry_info_.clear();
  did_bypass_proxy_ = false;
}

bool ProxyInfo::AllChainProxiesAreHttps() const {
  const std::vector<ProxyServer>& proxy_servers = proxy_chain().proxy_servers();
  return std::all_of(
      proxy_servers.begin(), proxy_servers.end(),
      [](const ProxyServer& proxy_server) { return proxy_server.is_https(); });
}

}  // namespace net
