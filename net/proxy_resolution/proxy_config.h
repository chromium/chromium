// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_RESOLUTION_PROXY_CONFIG_H_
#define NET_PROXY_RESOLUTION_PROXY_CONFIG_H_

#include <string>

#include "net/base/net_export.h"
#include "net/base/proxy_server.h"
#include "net/proxy_resolution/proxy_bypass_rules.h"
#include "net/proxy_resolution/proxy_list.h"
#include "url/gurl.h"

namespace base {
class Value;
}

namespace net {

class ProxyInfo;

// ProxyConfig describes a user's proxy settings.
//
// There are two categories of proxy settings:
//   (1) Automatic (indicates the methods to obtain a PAC script)
//   (2) Manual (simple set of proxy servers per scheme, and bypass patterns)
//
// When both automatic and manual settings are specified, the Automatic ones
// take precedence over the manual ones.
//
// For more details see:
// http://www.chromium.org/developers/design-documents/network-stack/proxy-settings-fallback
class NET_EXPORT ProxyConfig {
 public:
  // ProxyRules describes the "manual" proxy settings.
  struct NET_EXPORT ProxyRules {
    enum class Type {
      EMPTY,
      PROXY_LIST,
      PROXY_LIST_PER_SCHEME,
    };

    // Note that the default of Type::EMPTY results in direct connections
    // being made when using this ProxyConfig.
    ProxyRules();
    ProxyRules(const ProxyRules& other);
    ~ProxyRules();

    bool empty() const {
      return type == Type::EMPTY;
    }

    // Sets |result| with the proxies to use for |url| based on the current
    // rules.
    void Apply(const GURL& url, ProxyInfo* result) const;

    // Parses the rules from a string, indicating which proxies to use.
    //
    //   proxy-uri = [<proxy-scheme>"://"]<proxy-host>[":"<proxy-port>]
    //
    //   proxy-uri-list = <proxy-uri>[","<proxy-uri-list>]
    //
    //   url-scheme = "http" | "https" | "ftp" | "socks"
    //
    //   scheme-proxies = [<url-scheme>"="]<proxy-uri-list>
    //
    //   proxy-rules = scheme-proxies[";"<scheme-proxies>]
    //
    // Thus, the proxy-rules string should be a semicolon-separated list of
    // ordered proxies that apply to a particular URL scheme. Unless specified,
    // the proxy scheme for proxy-uris is assumed to be http.
    //
    // Some special cases:
    //  * If the scheme is omitted from the first proxy list, that list applies
    //    to all URL schemes and subsequent lists are ignored.
    //  * If a scheme is omitted from any proxy list after a list where a scheme
    //    has been provided, the list without a scheme is ignored.
    //  * If the url-scheme is set to 'socks', that sets a fallback list that
    //    to all otherwise unspecified url-schemes, however the default proxy-
    //    scheme for proxy urls in the 'socks' list is understood to be
    //    socks4:// if unspecified.
    //
    // For example:
    //   "http=foopy:80;ftp=foopy2"  -- use HTTP proxy "foopy:80" for http://
    //                                  URLs, and HTTP proxy "foopy2:80" for
    //                                  ftp:// URLs.
    //   "foopy:80"                  -- use HTTP proxy "foopy:80" for all URLs.
    //   "foopy:80,bar,direct://"    -- use HTTP proxy "foopy:80" for all URLs,
    //                                  failing over to "bar" if "foopy:80" is
    //                                  unavailable, and after that using no
    //                                  proxy.
    //   "socks4://foopy"            -- use SOCKS v4 proxy "foopy:1080" for all
    //                                  URLs.
    //   "http=foop,socks5://bar.com -- use HTTP proxy "foopy" for http URLs,
    //                                  and fail over to the SOCKS5 proxy
    //                                  "bar.com" if "foop" is unavailable.
    //   "http=foopy,direct://       -- use HTTP proxy "foopy" for http URLs,
    //                                  and use no proxy if "foopy" is
    //                                  unavailable.
    //   "http=foopy;socks=foopy2   --  use HTTP proxy "foopy" for http URLs,
    //                                  and use socks4://foopy2 for all other
    //                                  URLs.
    void ParseFromString(const std::string& proxy_rules);

    // Returns one of {&proxies_for_http, &proxies_for_https, &proxies_for_ftp,
    // &fallback_proxies}, or NULL if there is no proxy to use.
    // Should only call this if the type is Type::PROXY_LIST_PER_SCHEME.
    const ProxyList* MapUrlSchemeToProxyList(
        const std::string& url_scheme) const;

    // Returns true if |*this| describes the same configuration as |other|.
    bool Equals(const ProxyRules& other) const;

    // Exceptions for when not to use a proxy.
    ProxyBypassRules bypass_rules;

    // Reverse the meaning of |bypass_rules|.
    bool reverse_bypass;

    Type type;

    // Set if |type| is Type::PROXY_LIST.
    ProxyList single_proxies;

    // Set if |type| is Type::PROXY_LIST_PER_SCHEME.
    ProxyList proxies_for_http;
    ProxyList proxies_for_https;
    ProxyList proxies_for_ftp;

    // Used when a fallback has been defined and the url to be proxied doesn't
    // match any of the standard schemes.
    ProxyList fallback_proxies;

   private:
    // Returns one of {&proxies_for_http, &proxies_for_https, &proxies_for_ftp}
    // or NULL if it is a scheme that we don't have a mapping for. Should only
    // call this if the type is Type::PROXY_LIST_PER_SCHEME. Intentionally returns
    // NULL for "ws" and "wss" as those are handled specially by
    // GetProxyListForWebSocketScheme().
    ProxyList* MapUrlSchemeToProxyListNoFallback(const std::string& scheme);

    // Returns the first of {&fallback_proxies, &proxies_for_https,
    // &proxies_for_http} that is non-empty, or NULL.
    const ProxyList* GetProxyListForWebSocketScheme() const;
  };

  ProxyConfig();
  ProxyConfig(const ProxyConfig& config);
  ~ProxyConfig();
  ProxyConfig& operator=(const ProxyConfig& config);

  // Returns true if the given config is equivalent to this config.
  bool Equals(const ProxyConfig& other) const;

  // Returns true if this config contains any "automatic" settings. See the
  // class description for what that means.
  bool HasAutomaticSettings() const;

  void ClearAutomaticSettings();

  // Creates a Value dump of this configuration.
  base::Value ToValue() const;

  ProxyRules& proxy_rules() {
    return proxy_rules_;
  }

  const ProxyRules& proxy_rules() const {
    return proxy_rules_;
  }

  void set_pac_url(const GURL& url) {
    pac_url_ = url;
  }

  const GURL& pac_url() const {
    return pac_url_;
  }

  void set_pac_mandatory(bool enable_pac_mandatory) {
    pac_mandatory_ = enable_pac_mandatory;
  }

  bool pac_mandatory() const {
    return pac_mandatory_;
  }

  bool has_pac_url() const {
    return pac_url_.is_valid();
  }

  void set_auto_detect(bool enable_auto_detect) {
    auto_detect_ = enable_auto_detect;
  }

  bool auto_detect() const {
    return auto_detect_;
  }

  // Helpers to construct some common proxy configurations.

  static ProxyConfig CreateDirect() {
    return ProxyConfig();
  }

  static ProxyConfig CreateAutoDetect() {
    ProxyConfig config;
    config.set_auto_detect(true);
    return config;
  }

  static ProxyConfig CreateFromCustomPacURL(const GURL& pac_url) {
    ProxyConfig config;
    config.set_pac_url(pac_url);
    // By default fall back to direct connection in case PAC script fails.
    config.set_pac_mandatory(false);
    return config;
  }

 private:
  // True if the proxy configuration should be auto-detected.
  bool auto_detect_;

  // If non-empty, indicates the URL of the proxy auto-config file to use.
  GURL pac_url_;

  // If true, blocks all traffic in case fetching the PAC script from |pac_url_|
  // fails. Only valid if |pac_url_| is non-empty.
  bool pac_mandatory_;

  // Manual proxy settings.
  ProxyRules proxy_rules_;
};

}  // namespace net



#endif  // NET_PROXY_RESOLUTION_PROXY_CONFIG_H_
