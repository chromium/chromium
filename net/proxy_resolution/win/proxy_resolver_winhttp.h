// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_RESOLUTION_WIN_PROXY_RESOLVER_WINHTTP_H_
#define NET_PROXY_RESOLUTION_WIN_PROXY_RESOLVER_WINHTTP_H_

#include "base/compiler_specific.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_export.h"
#include "net/proxy_resolution/proxy_resolver_factory.h"
#include "url/gurl.h"

namespace net {

// An implementation of ProxyResolverFactory that uses WinHTTP and the system
// proxy settings.
class NET_EXPORT_PRIVATE ProxyResolverFactoryWinHttp
    : public ProxyResolverFactory {
 public:
  ProxyResolverFactoryWinHttp();

  ProxyResolverFactoryWinHttp(const ProxyResolverFactoryWinHttp&) = delete;
  ProxyResolverFactoryWinHttp& operator=(const ProxyResolverFactoryWinHttp&) =
      delete;

  int CreateProxyResolver(const scoped_refptr<PacFileData>& pac_script,
                          std::unique_ptr<ProxyResolver>* resolver,
                          CompletionOnceCallback callback,
                          std::unique_ptr<Request>* request) override;
};

}  // namespace net

#endif  // NET_PROXY_RESOLUTION_WIN_PROXY_RESOLVER_WINHTTP_H_
