// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_RESOLUTION_PROXY_RESOLVER_APPLE_H_
#define NET_PROXY_RESOLUTION_PROXY_RESOLVER_APPLE_H_

#include <CoreFoundation/CoreFoundation.h>

#include "base/apple/scoped_cftyperef.h"
#include "base/compiler_specific.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_export.h"
#include "net/proxy_resolution/proxy_resolver_factory.h"

namespace net {

class ProxyList;

// Executes one PAC URL or inline script on the shared CFNetwork runloop.
// Construct and destroy on the caller's sequence. Completion is always posted
// back to that sequence; destruction cancels delivery without blocking it.
class NET_EXPORT ProxyResolverAppleRequest {
 public:
  // Returns a net error, the proxy list, and ownership of any CFNetwork error.
  using ResultCallback = base::OnceCallback<
      void(int, ProxyList, base::apple::ScopedCFTypeRef<CFErrorRef>)>;

  ProxyResolverAppleRequest(CFURLRef pac_url,
                            CFURLRef query_url,
                            ResultCallback callback);
  ProxyResolverAppleRequest(CFStringRef script,
                            CFURLRef query_url,
                            ResultCallback callback);
  ~ProxyResolverAppleRequest();

  ProxyResolverAppleRequest(const ProxyResolverAppleRequest&) = delete;
  ProxyResolverAppleRequest& operator=(const ProxyResolverAppleRequest&) =
      delete;

 private:
  class Context;

  ProxyResolverAppleRequest(CFURLRef pac_url,
                            CFStringRef script,
                            CFURLRef query_url,
                            ResultCallback callback);
  void OnResult(int net_error,
                ProxyList proxy_list,
                base::apple::ScopedCFTypeRef<CFErrorRef> error);

  ResultCallback callback_;
  scoped_refptr<Context> context_;
  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<ProxyResolverAppleRequest> weak_factory_{this};
};

// Implementation of ProxyResolverFactory that uses the Apple CFProxySupport to
// implement proxies.
// TODO(crbug.com/570599719): Remove legacy synchronous path once
// `features::kAsyncProxyResolverApple` is launched.
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
