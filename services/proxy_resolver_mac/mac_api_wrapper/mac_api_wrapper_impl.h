// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PROXY_RESOLVER_MAC_MAC_API_WRAPPER_MAC_API_WRAPPER_IMPL_H_
#define SERVICES_PROXY_RESOLVER_MAC_MAC_API_WRAPPER_MAC_API_WRAPPER_IMPL_H_

#include "services/proxy_resolver_mac/mac_api_wrapper/mac_api_wrapper.h"

namespace proxy_resolver_mac {

// Production implementation of MacAPIWrapper that makes actual system calls
// to SystemConfiguration and CFNetwork frameworks.
class MacAPIWrapperImpl : public MacAPIWrapper {
 public:
  MacAPIWrapperImpl();
  ~MacAPIWrapperImpl() override;

  MacAPIWrapperImpl(const MacAPIWrapperImpl&) = delete;
  MacAPIWrapperImpl& operator=(const MacAPIWrapperImpl&) = delete;

  // MacAPIWrapper implementation:

  // Returns the system proxy configuration dictionary by calling
  // SCDynamicStoreCopyProxies(). Returns nullptr if the system configuration
  // cannot be retrieved.
  base::apple::ScopedCFTypeRef<CFDictionaryRef> CopyProxies() override;

  // Returns an array of proxy dictionaries for the given URL by calling
  // CFNetworkCopyProxiesForURL(). Each dictionary in the array describes
  // a proxy that can be used to reach the URL. Returns nullptr on failure.
  base::apple::ScopedCFTypeRef<CFArrayRef> CopyProxiesForURL(
      const GURL& url,
      CFDictionaryRef proxy_settings) override;
};

}  // namespace proxy_resolver_mac

#endif  // SERVICES_PROXY_RESOLVER_MAC_MAC_API_WRAPPER_MAC_API_WRAPPER_IMPL_H_
