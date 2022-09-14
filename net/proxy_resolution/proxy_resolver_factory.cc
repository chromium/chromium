// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy_resolution/proxy_resolver_factory.h"

#include "net/base/net_errors.h"
#include "net/proxy_resolution/proxy_resolver.h"

namespace net {

ProxyResolverFactory::ProxyResolverFactory(bool expects_pac_bytes)
    : expects_pac_bytes_(expects_pac_bytes) {
}

ProxyResolverFactory::~ProxyResolverFactory() = default;

}  // namespace net
