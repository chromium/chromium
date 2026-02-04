// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PROXY_RESOLVER_MAC_MAC_API_WRAPPER_MAC_API_WRAPPER_H_
#define SERVICES_PROXY_RESOLVER_MAC_MAC_API_WRAPPER_MAC_API_WRAPPER_H_

#include <CoreFoundation/CoreFoundation.h>

#include <memory>

#include "base/apple/scoped_cftyperef.h"
#include "url/gurl.h"

namespace proxy_resolver_mac {

// Abstract interface for macOS SystemConfiguration and CFNetwork proxy
// resolution APIs. This abstraction allows for testing without making actual
// system calls.
class MacAPIWrapper {
 public:
  virtual ~MacAPIWrapper() = default;

  // Creates a MacAPIWrapper instance for production use.
  static std::unique_ptr<MacAPIWrapper> Create();

  // Wraps SCDynamicStoreCopyProxies() to get the system proxy configuration.
  // The "Copy" naming follows Apple's Core Foundation convention indicating
  // the caller takes ownership of the returned object.
  // Returns nullptr on failure.
  virtual base::apple::ScopedCFTypeRef<CFDictionaryRef> CopyProxies() = 0;

  // Wraps CFNetworkCopyProxiesForURL() to resolve proxies for a given URL.
  // The "Copy" naming follows Apple's Core Foundation convention indicating
  // the caller takes ownership of the returned object.
  // |url| is the URL to resolve proxies for.
  // |proxy_settings| is the proxy configuration from CopyProxies().
  // Returns nullptr on failure.
  virtual base::apple::ScopedCFTypeRef<CFArrayRef> CopyProxiesForURL(
      const GURL& url,
      CFDictionaryRef proxy_settings) = 0;
};

}  // namespace proxy_resolver_mac

#endif  // SERVICES_PROXY_RESOLVER_MAC_MAC_API_WRAPPER_MAC_API_WRAPPER_H_
