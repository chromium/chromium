// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_RESOLUTION_PROXY_RESOLVER_APPLE_H_
#define NET_PROXY_RESOLUTION_PROXY_RESOLVER_APPLE_H_

#include "base/compiler_specific.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_export.h"
#include "net/proxy_resolution/proxy_resolver_factory.h"
#include "url/gurl.h"

namespace net {

// Implementation of ProxyResolverFactory that uses the Apple CFProxySupport to
// implement proxies.
// TODO(kapishnikov): make ProxyResolverApple async as per
// https://bugs.chromium.org/p/chromium/issues/detail?id=166387#c95
class NET_EXPORT ProxyResolverFactoryApple : public ProxyResolverFactory {
 public:
  ProxyResolverFactoryApple();

  ProxyResolverFactoryApple(const ProxyResolverFactoryApple&) = delete;
  ProxyResolverFactoryApple& operator=(const ProxyResolverFactoryApple&) = delete;

  int CreateProxyResolver(const scoped_refptr<PacFileData>& pac_script,
                          std::unique_ptr<ProxyResolver>* resolver,
                          CompletionOnceCallback callback,
                          std::unique_ptr<Request>* request) override;
};

}  // namespace net

#endif  // NET_PROXY_RESOLUTION_PROXY_RESOLVER_APPLE_H_
