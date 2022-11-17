// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PROXY_RESOLVER_PROXY_RESOLVER_V8_H_
#define SERVICES_PROXY_RESOLVER_PROXY_RESOLVER_V8_H_

#include <stddef.h>

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/memory/scoped_refptr.h"
#include "net/proxy_resolution/proxy_resolve_dns_operation.h"

class GURL;

namespace net {
class ProxyInfo;
class PacFileData;
}  // namespace net

namespace proxy_resolver {

// A synchronous ProxyResolver-like that uses V8 to evaluate PAC scripts.
class ProxyResolverV8 {
 public:
  // Interface for the javascript bindings.
  class JSBindings {
   public:
    JSBindings() {}

    // Handler for "dnsResolve()", "dnsResolveEx()", "myIpAddress()",
    // "myIpAddressEx()". Returns true on success and fills |*output| with the
    // result. If |*terminate| is set to true, then the script execution will
    // be aborted. Note that termination may not happen right away.
    virtual bool ResolveDns(const std::string& host,
                            net::ProxyResolveDnsOperation op,
                            std::string* output,
                            bool* terminate) = 0;

    // Handler for "alert(message)"
    virtual void Alert(const std::u16string& message) = 0;

    // Handler for when an error is encountered. |line_number| may be -1
    // if a line number is not applicable to this error.
    virtual void OnError(int line_number, const std::u16string& error) = 0;

   protected:
    virtual ~JSBindings() {}
  };

  // Constructs a ProxyResolverV8.
  static int Create(const scoped_refptr<net::PacFileData>& script_data,
                    JSBindings* bindings,
                    std::unique_ptr<ProxyResolverV8>* resolver);

  ProxyResolverV8(const ProxyResolverV8&) = delete;
  ProxyResolverV8& operator=(const ProxyResolverV8&) = delete;

  ~ProxyResolverV8();

  int GetProxyForURL(const GURL& url,
                     net::ProxyInfo* results,
                     JSBindings* bindings);

  // Get total/used heap memory usage of all v8 instances used by the proxy
  // resolver.
  static size_t GetTotalHeapSize();
  static size_t GetUsedHeapSize();

 private:
  // Context holds the Javascript state for the PAC script.
  class Context;

  explicit ProxyResolverV8(std::unique_ptr<Context> context);

  std::unique_ptr<Context> context_;
};

}  // namespace proxy_resolver

#endif  // SERVICES_PROXY_RESOLVER_PROXY_RESOLVER_V8_H_
