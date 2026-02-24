// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/proxy_resolver_mac/mac_api_wrapper/mac_api_wrapper_impl.h"

#include <CFNetwork/CFNetwork.h>
#include <SystemConfiguration/SystemConfiguration.h>

#include "base/apple/bridging.h"
#include "base/apple/scoped_cftyperef.h"
#include "net/base/apple/url_conversions.h"
#include "net/base/url_util.h"

namespace proxy_resolver_mac {

// static
std::unique_ptr<MacAPIWrapper> MacAPIWrapper::Create() {
  return std::make_unique<MacAPIWrapperImpl>();
}

MacAPIWrapperImpl::MacAPIWrapperImpl() = default;

MacAPIWrapperImpl::~MacAPIWrapperImpl() = default;

base::apple::ScopedCFTypeRef<CFDictionaryRef> MacAPIWrapperImpl::CopyProxies() {
  return base::apple::ScopedCFTypeRef<CFDictionaryRef>(
      SCDynamicStoreCopyProxies(nullptr));
}

base::apple::ScopedCFTypeRef<CFArrayRef> MacAPIWrapperImpl::CopyProxiesForURL(
    const GURL& url,
    CFDictionaryRef proxy_settings) {
  // macOS system resolver does not support WebSocket URLs. Replace ws/wss
  // schemes with http/https respectively.
  GURL effective_url =
      url.SchemeIsWSOrWSS() ? net::ChangeWebSocketSchemeToHttpScheme(url) : url;

  NSURL* ns_url = net::NSURLWithGURL(effective_url);
  if (!ns_url) {
    return base::apple::ScopedCFTypeRef<CFArrayRef>();
  }

  return base::apple::ScopedCFTypeRef<CFArrayRef>(CFNetworkCopyProxiesForURL(
      base::apple::NSToCFPtrCast(ns_url), proxy_settings));
}

}  // namespace proxy_resolver_mac
