// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_RESOLUTION_PROXY_LIST_H_
#define NET_PROXY_RESOLUTION_PROXY_LIST_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "net/base/net_export.h"
#include "net/proxy_resolution/proxy_retry_info.h"

namespace base {
class TimeDelta;
class Value;
}

namespace net {

class ProxyServer;
class NetLogWithSource;

// This class is used to hold a list of proxies returned by GetProxyForUrl or
// manually configured. It handles proxy fallback if multiple servers are
// specified.
class NET_EXPORT_PRIVATE ProxyList {
 public:
  ProxyList();
  ProxyList(const ProxyList& other);
  ProxyList(ProxyList&& other);
  ProxyList& operator=(const ProxyList& other);
  ProxyList& operator=(ProxyList&& other);
  ~ProxyList();

  // Initializes the proxy list to a string containing one or more proxy servers
  // delimited by a semicolon.
  void Set(const std::string& proxy_uri_list);

  // Set the proxy list to a single entry, |proxy_server|.
  void SetSingleProxyServer(const ProxyServer& proxy_server);

  // Append a single proxy server to the end of the proxy list.
  void AddProxyServer(const ProxyServer& proxy_server);

  // De-prioritizes the proxies that are cached as not working but are allowed
  // to be reconsidered, by moving them to the end of the fallback list.
  void DeprioritizeBadProxies(const ProxyRetryInfoMap& proxy_retry_info);

  // Delete any entry which doesn't have one of the specified proxy schemes.
  // |scheme_bit_field| is a bunch of ProxyServer::Scheme bitwise ORed together.
  void RemoveProxiesWithoutScheme(int scheme_bit_field);

  // Clear the proxy list.
  void Clear();

  // Returns true if there is nothing left in the ProxyList.
  bool IsEmpty() const;

  // Returns the number of proxy servers in this list.
  size_t size() const;

  // Returns true if |*this| lists the same proxies as |other|.
  bool Equals(const ProxyList& other) const;

  // Returns the first proxy server in the list. It is only valid to call
  // this if !IsEmpty().
  const ProxyServer& Get() const;

  // Returns all proxy servers in the list.
  const std::vector<ProxyServer>& GetAll() const;

  // Sets the list by parsing the PAC result |pac_string|.
  // Some examples for |pac_string|:
  //   "DIRECT"
  //   "PROXY foopy1"
  //   "PROXY foopy1; SOCKS4 foopy2:1188"
  // Does a best-effort parse, and silently discards any errors.
  void SetFromPacString(const std::string& pac_string);

  // Returns a PAC-style semicolon-separated list of valid proxy servers.
  // For example: "PROXY xxx.xxx.xxx.xxx:xx; SOCKS yyy.yyy.yyy:yy".
  std::string ToPacString() const;

  // Returns a serialized value for the list.
  base::Value ToValue() const;

  // Marks the current proxy server as bad and deletes it from the list. The
  // list of known bad proxies is given by |proxy_retry_info|. |net_error|
  // should contain the network error encountered when this proxy was tried, if
  // any. If this fallback is not because of a network error, then |OK| should
  // be passed in (eg. for reasons such as local policy). Returns true if there
  // is another server available in the list.
  bool Fallback(ProxyRetryInfoMap* proxy_retry_info,
                int net_error,
                const NetLogWithSource& net_log);

  // Updates |proxy_retry_info| to indicate that the first proxy in the list
  // is bad. This is distinct from Fallback(), above, to allow updating proxy
  // retry information without modifying a given transction's proxy list. Will
  // retry after |retry_delay| if positive, and will use the default proxy retry
  // duration otherwise. It may reconsider the proxy beforehand if |reconsider|
  // is true. Additionally updates |proxy_retry_info| with
  // |additional_proxies_to_bypass|. |net_error| should contain the network
  // error countered when this proxy was tried, or OK if the proxy retry info is
  // being updated for a non-network related reason (e.g. local policy).
  void UpdateRetryInfoOnFallback(
      ProxyRetryInfoMap* proxy_retry_info,
      base::TimeDelta retry_delay,
      bool reconsider,
      const std::vector<ProxyServer>& additional_proxies_to_bypass,
      int net_error,
      const NetLogWithSource& net_log) const;

 private:
  // Updates |proxy_retry_info| to indicate that the |proxy_to_retry| in
  // |proxies_| is bad for |retry_delay|, but may be reconsidered earlier if
  // |try_while_bad| is true. |net_error| should contain the network error
  // countered when this proxy was tried, or OK if the proxy retry info is
  // being updated for a non-network related reason (e.g. local policy).
  void AddProxyToRetryList(ProxyRetryInfoMap* proxy_retry_info,
                           base::TimeDelta retry_delay,
                           bool try_while_bad,
                           const ProxyServer& proxy_to_retry,
                           int net_error,
                           const NetLogWithSource& net_log) const;

  // List of proxies.
  std::vector<ProxyServer> proxies_;
};

}  // namespace net

#endif  // NET_PROXY_RESOLUTION_PROXY_LIST_H_
