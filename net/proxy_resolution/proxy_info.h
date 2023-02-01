// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_RESOLUTION_PROXY_INFO_H_
#define NET_PROXY_RESOLUTION_PROXY_INFO_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "base/time/time.h"
#include "net/base/net_export.h"
#include "net/base/proxy_server.h"
#include "net/proxy_resolution/proxy_config.h"
#include "net/proxy_resolution/proxy_list.h"
#include "net/proxy_resolution/proxy_retry_info.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace net {

class NetLogWithSource;

// This object holds proxy information returned by ResolveProxy.
class NET_EXPORT ProxyInfo {
 public:
  ProxyInfo();
  ProxyInfo(const ProxyInfo& other);
  ~ProxyInfo();
  // Default copy-constructor and assignment operator are OK!

  // Uses the same proxy server as the given |proxy_info|.
  void Use(const ProxyInfo& proxy_info);

  // Uses a direct connection.
  void UseDirect();

  // Uses a direct connection. did_bypass_proxy() will return true to indicate
  // that the direct connection is the result of configured proxy bypass rules.
  void UseDirectWithBypassedProxy();

  // Uses a specific proxy server, of the form:
  //   proxy-uri = [<scheme> "://"] <hostname> [":" <port>]
  // This may optionally be a semi-colon delimited list of <proxy-uri>.
  // It is OK to have LWS between entries.
  void UseNamedProxy(const std::string& proxy_uri_list);

  // Sets the proxy list to a single entry, |proxy_server|.
  void UseProxyServer(const ProxyServer& proxy_server);

  // Parses from the given PAC result.
  void UsePacString(const std::string& pac_string);

  // Uses the proxies from the given list.
  void UseProxyList(const ProxyList& proxy_list);

  // Uses the proxies from the given list, but does not otherwise reset the
  // proxy configuration.
  void OverrideProxyList(const ProxyList& proxy_list);

  // Returns true if this proxy info specifies a direct connection.
  bool is_direct() const {
    // We don't implicitly fallback to DIRECT unless it was added to the list.
    if (is_empty())
      return false;
    return proxy_list_.Get().is_direct();
  }

  bool is_direct_only() const {
    return is_direct() && proxy_list_.size() == 1 && proxy_retry_info_.empty();
  }

  // Returns true if the first valid proxy server is an https proxy.
  bool is_https() const {
    if (is_empty())
      return false;
    return proxy_server().is_https();
  }

  // Returns true if the first proxy server is an HTTP compatible proxy.
  bool is_http_like() const {
    if (is_empty())
      return false;
    return proxy_server().is_http_like();
  }

  // Returns true if the first proxy server is an HTTP compatible proxy over a
  // secure connection.
  bool is_secure_http_like() const {
    if (is_empty())
      return false;
    return proxy_server().is_secure_http_like();
  }

  // Returns true if the first valid proxy server is an http proxy.
  bool is_http() const {
    if (is_empty())
      return false;
    return proxy_server().is_http();
  }

  // Returns true if the first valid proxy server is a quic proxy.
  bool is_quic() const {
    if (is_empty())
      return false;
    return proxy_server().is_quic();
  }

  // Returns true if the first valid proxy server is a socks server.
  bool is_socks() const {
    if (is_empty())
      return false;
    return proxy_server().is_socks();
  }

  // Returns true if this proxy info has no proxies left to try.
  bool is_empty() const {
    return proxy_list_.IsEmpty();
  }

  // Returns true if this proxy resolution is using a direct connection due to
  // proxy bypass rules.
  bool did_bypass_proxy() const {
    return did_bypass_proxy_;
  }

  // Returns the first valid proxy server. is_empty() must be false to be able
  // to call this function.
  const ProxyServer& proxy_server() const { return proxy_list_.Get(); }

  // Returns the full list of proxies to use.
  const ProxyList& proxy_list() const { return proxy_list_; }

  // See description in ProxyList::ToPacString().
  std::string ToPacString() const;

  // Marks the current proxy as bad. |net_error| should contain the network
  // error encountered when this proxy was tried, if any. If this fallback
  // is not because of a network error, then |OK| should be passed in (eg. for
  // reasons such as local policy). Returns true if there is another proxy
  // available to try in |proxy_list_|.
  bool Fallback(int net_error, const NetLogWithSource& net_log);

  // De-prioritizes the proxies that we have cached as not working, by moving
  // them to the end of the proxy list.
  void DeprioritizeBadProxies(const ProxyRetryInfoMap& proxy_retry_info);

  // Deletes any entry which doesn't have one of the specified proxy schemes.
  void RemoveProxiesWithoutScheme(int scheme_bit_field);

  void set_proxy_resolve_start_time(
      const base::TimeTicks& proxy_resolve_start_time) {
    proxy_resolve_start_time_ = proxy_resolve_start_time;
  }

  base::TimeTicks proxy_resolve_start_time() const {
    return proxy_resolve_start_time_;
  }

  void set_proxy_resolve_end_time(
      const base::TimeTicks& proxy_resolve_end_time) {
    proxy_resolve_end_time_ = proxy_resolve_end_time;
  }

  base::TimeTicks proxy_resolve_end_time() const {
    return proxy_resolve_end_time_;
  }

  void set_traffic_annotation(
      const MutableNetworkTrafficAnnotationTag& traffic_annotation) {
    traffic_annotation_ = traffic_annotation;
  }

  MutableNetworkTrafficAnnotationTag traffic_annotation() const {
    return traffic_annotation_;
  }

  const ProxyRetryInfoMap& proxy_retry_info() const {
    return proxy_retry_info_;
  }

 private:
  // Reset proxy and config settings.
  void Reset();

  // The ordered list of proxy servers (including DIRECT attempts) remaining to
  // try. If proxy_list_ is empty, then there is nothing left to fall back to.
  ProxyList proxy_list_;

  // List of proxies that have been tried already.
  ProxyRetryInfoMap proxy_retry_info_;

  // The traffic annotation of the used proxy config.
  MutableNetworkTrafficAnnotationTag traffic_annotation_;

  // Whether the proxy result represent a proxy bypass.
  bool did_bypass_proxy_ = false;

  // How long it took to resolve the proxy.  Times are both null if proxy was
  // determined synchronously without running a PAC.
  base::TimeTicks proxy_resolve_start_time_;
  base::TimeTicks proxy_resolve_end_time_;
};

}  // namespace net

#endif  // NET_PROXY_RESOLUTION_PROXY_INFO_H_
