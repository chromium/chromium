// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_RESOLUTION_PROXY_LIST_H_
#define NET_PROXY_RESOLUTION_PROXY_LIST_H_

#include <stddef.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "net/base/net_export.h"
#include "net/base/proxy_server.h"
#include "net/proxy_resolution/proxy_retry_info.h"

namespace base {
class TimeDelta;
class Value;
}

namespace net {

class ProxyChain;
class NetLogWithSource;

// This class is used to hold a prioritized list of proxy chains. It handles
// fallback to lower-priority chains if multiple chains are specified.
class NET_EXPORT_PRIVATE ProxyList {
 public:
  ProxyList();
  ProxyList(const ProxyList& other);
  ProxyList(ProxyList&& other);
  ProxyList& operator=(const ProxyList& other);
  ProxyList& operator=(ProxyList&& other);
  ~ProxyList();

  // Initializes the ProxyList to contain one or more ProxyChains.
  // `proxy_uri_list` is a semicolon-delimited list of proxy URIs. Note that
  // multi-proxy chains cannot be represented in this format.
  void Set(const std::string& proxy_uri_list);

  // Set the proxy list to a single entry, |proxy_chain|.
  void SetSingleProxyChain(const ProxyChain& proxy_chain);

  // Set the proxy list to a single entry, a chain containing |proxy_server|.
  void SetSingleProxyServer(const ProxyServer& proxy_server);

  // Append a single proxy chain to the end of the proxy list.
  void AddProxyChain(const ProxyChain& proxy_chain);

  // Append a single proxy chain containing the given server to the end of the
  // proxy list.
  void AddProxyServer(const ProxyServer& proxy_server);

  // De-prioritizes the proxy chains that are cached as not working but are
  // allowed to be reconsidered, by moving them to the end of the fallback list.
  void DeprioritizeBadProxyChains(const ProxyRetryInfoMap& proxy_retry_info);

  // Deletes all chains which don't exclusively consist of proxy servers with
  // the specified schemes. `scheme_bit_field` is a bunch of
  // `ProxyServer::Scheme` bitwise ORed together. This is used to remove proxies
  // that do not support specific functionality such as websockets.
  void RemoveProxiesWithoutScheme(int scheme_bit_field);

  // Clear the proxy list.
  void Clear();

  // Returns true if there is nothing left in the ProxyList.
  bool IsEmpty() const;

  // Returns the number of proxy servers in this list.
  size_t size() const;

  // Returns true if |*this| lists the same proxies as |other|.
  bool Equals(const ProxyList& other) const;

  // Returns the first proxy chain in the list.
  const ProxyChain& First() const;

  // Returns all proxy chains in the list.
  const std::vector<ProxyChain>& AllChains() const;

  // Sets the list by parsing the PAC result |pac_string|.
  // Some examples for |pac_string|:
  //   "DIRECT"
  //   "PROXY foopy1"
  //   "PROXY foopy1; SOCKS4 foopy2:1188"
  // Does a best-effort parse, and silently discards any errors.
  void SetFromPacString(const std::string& pac_string);

  // Returns a PAC-style semicolon-separated list of valid proxy servers.
  // For example: "PROXY xxx.xxx.xxx.xxx:xx; SOCKS yyy.yyy.yyy:yy". This is
  // only valid if the list contains no multi-proxy chains, as those cannot
  // be represented in PAC syntax.
  std::string ToPacString() const;

  // Returns a semicolon-separated list of proxy chain debug representations.
  // For single-proxy chains, this is just the PAC representation of the proxy;
  // otherwise the chain is displayed in "[..]".
  // TODO(crbug.com/40284947): Once a PAC string format for multi-proxy
  // chains is implemented, this can be removed in favor of `ToPacString()`.
  std::string ToDebugString() const;

  // Returns a serialized value for the list.
  base::Value ToValue() const;

  // Marks the current proxy chain as bad and deletes it from the list. The
  // list of known bad proxies is given by |proxy_retry_info|. |net_error|
  // should contain the network error encountered when this proxy chain was
  // tried, if any. If this fallback is not because of a network error, then
  // |OK| should be passed in (eg. for reasons such as local policy). Returns
  // true if there is another chain available in the list.
  bool Fallback(ProxyRetryInfoMap* proxy_retry_info,
                int net_error,
                const NetLogWithSource& net_log);

  // Updates |proxy_retry_info| to indicate that the first proxy chain in the
  // list is bad. This is distinct from Fallback(), above, to allow updating
  // proxy retry information without modifying a given transction's proxy list.
  // Will retry after |retry_delay| if positive, and will use the default proxy
  // retry duration otherwise. It may reconsider the proxy beforehand if
  // |reconsider| is true. Additionally updates |proxy_retry_info| with
  // |additional_proxies_to_bypass|. |net_error| should contain the network
  // error countered when this proxy chain was tried, or OK if the proxy retry
  // info is being updated for a non-network related reason (e.g. local policy).
  void UpdateRetryInfoOnFallback(
      ProxyRetryInfoMap* proxy_retry_info,
      base::TimeDelta retry_delay,
      bool reconsider,
      const std::vector<ProxyChain>& additional_proxies_to_bypass,
      int net_error,
      const NetLogWithSource& net_log) const;

 private:
  // Updates |proxy_retry_info| to indicate that the |proxy_to_retry| in
  // |proxies_| is bad for |retry_delay|, but may be reconsidered earlier if
  // |try_while_bad| is true. |net_error| should contain the network error
  // countered when this proxy was tried, or OK if the proxy retry info is
  // being updated for a non-network related reason (e.g. local policy).
  void AddProxyChainToRetryList(ProxyRetryInfoMap* proxy_retry_info,
                                base::TimeDelta retry_delay,
                                bool try_while_bad,
                                const ProxyChain& proxy_chain_to_retry,
                                int net_error,
                                const NetLogWithSource& net_log) const;

  // List of proxy chains.
  std::vector<ProxyChain> proxy_chains_;
};

}  // namespace net

#endif  // NET_PROXY_RESOLUTION_PROXY_LIST_H_
