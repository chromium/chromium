// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy_resolution/proxy_config_service_ios.h"

#include <CFNetwork/CFProxySupport.h>
#include <CoreFoundation/CoreFoundation.h>

#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/sys_string_conversions.h"
#include "net/base/proxy_chain.h"
#include "net/proxy_resolution/proxy_chain_util_apple.h"
#include "net/proxy_resolution/proxy_config_with_annotation.h"

namespace net {

namespace {

const int kPollIntervalSec = 10;

// Utility function to pull out a boolean value from a dictionary and return it,
// returning a default value if the key is not present.
bool GetBoolFromDictionary(CFDictionaryRef dict,
                           CFStringRef key,
                           bool default_value) {
  CFNumberRef number =
      base::apple::GetValueFromDictionary<CFNumberRef>(dict, key);
  if (!number) {
    return default_value;
  }

  int int_value;
  if (CFNumberGetValue(number, kCFNumberIntType, &int_value)) {
    return int_value;
  } else {
    return default_value;
  }
}

void GetCurrentProxyConfig(const NetworkTrafficAnnotationTag traffic_annotation,
                           ProxyConfigWithAnnotation* config) {
  base::apple::ScopedCFTypeRef<CFDictionaryRef> config_dict(
      CFNetworkCopySystemProxySettings());
  DCHECK(config_dict);
  ProxyConfig proxy_config;
  // Auto-detect is not supported.
  // The kCFNetworkProxiesProxyAutoDiscoveryEnable key is not available on iOS.

  // PAC file

  if (GetBoolFromDictionary(config_dict.get(),
                            kCFNetworkProxiesProxyAutoConfigEnable, false)) {
    CFStringRef pac_url_ref = base::apple::GetValueFromDictionary<CFStringRef>(
        config_dict.get(), kCFNetworkProxiesProxyAutoConfigURLString);
    if (pac_url_ref) {
      proxy_config.set_pac_url(GURL(base::SysCFStringRefToUTF8(pac_url_ref)));
    }
  }

  // Proxies (for now http).

  // The following keys are not available on iOS:
  //   kCFNetworkProxiesFTPEnable
  //   kCFNetworkProxiesFTPProxy
  //   kCFNetworkProxiesFTPPort
  //   kCFNetworkProxiesHTTPSEnable
  //   kCFNetworkProxiesHTTPSProxy
  //   kCFNetworkProxiesHTTPSPort
  //   kCFNetworkProxiesSOCKSEnable
  //   kCFNetworkProxiesSOCKSProxy
  //   kCFNetworkProxiesSOCKSPort
  if (GetBoolFromDictionary(config_dict.get(), kCFNetworkProxiesHTTPEnable,
                            false)) {
    ProxyChain proxy_chain = ProxyDictionaryToProxyChain(
        kCFProxyTypeHTTP, config_dict.get(), kCFNetworkProxiesHTTPProxy,
        kCFNetworkProxiesHTTPPort);
    if (proxy_chain.IsValid()) {
      proxy_config.proxy_rules().type =
          ProxyConfig::ProxyRules::Type::PROXY_LIST_PER_SCHEME;
      proxy_config.proxy_rules().proxies_for_http.SetSingleProxyChain(
          proxy_chain);
      // Desktop Safari applies the HTTP proxy to http:// URLs only, but
      // Mobile Safari applies the HTTP proxy to https:// URLs as well.
      proxy_config.proxy_rules().proxies_for_https.SetSingleProxyChain(
          proxy_chain);
    }
  }

  // Proxy bypass list is not supported.
  // The kCFNetworkProxiesExceptionsList key is not available on iOS.

  // Proxy bypass boolean is not supported.
  // The kCFNetworkProxiesExcludeSimpleHostnames key is not available on iOS.

  // Source
  proxy_config.set_from_system(true);
  *config = ProxyConfigWithAnnotation(proxy_config, traffic_annotation);
}

}  // namespace

ProxyConfigServiceIOS::ProxyConfigServiceIOS(
    const NetworkTrafficAnnotationTag& traffic_annotation)
    : PollingProxyConfigService(base::Seconds(kPollIntervalSec),
                                base::BindRepeating(GetCurrentProxyConfig),
                                traffic_annotation) {}

ProxyConfigServiceIOS::~ProxyConfigServiceIOS() = default;

}  // namespace net
