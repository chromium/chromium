// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/proxy_resolver_mac/mac_api_wrapper/mac_api_wrapper_impl.h"

#include "base/notimplemented.h"

namespace proxy_resolver_mac {

// static
std::unique_ptr<MacAPIWrapper> MacAPIWrapper::Create() {
  return std::make_unique<MacAPIWrapperImpl>();
}

MacAPIWrapperImpl::MacAPIWrapperImpl() = default;

MacAPIWrapperImpl::~MacAPIWrapperImpl() = default;

base::apple::ScopedCFTypeRef<CFDictionaryRef> MacAPIWrapperImpl::CopyProxies() {
  // TODO(crbug.com/442313607): Implement using SCDynamicStoreCopyProxies().
  NOTIMPLEMENTED_LOG_ONCE();
  return base::apple::ScopedCFTypeRef<CFDictionaryRef>();
}

base::apple::ScopedCFTypeRef<CFArrayRef> MacAPIWrapperImpl::CopyProxiesForURL(
    const GURL& url,
    CFDictionaryRef proxy_settings) {
  // TODO(crbug.com/442313607): Implement using CFNetworkCopyProxiesForURL().
  NOTIMPLEMENTED_LOG_ONCE();
  return base::apple::ScopedCFTypeRef<CFArrayRef>();
}

}  // namespace proxy_resolver_mac
