// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_RESOLUTION_MULTI_THREADED_PROXY_RESOLVER_H_
#define NET_PROXY_RESOLUTION_MULTI_THREADED_PROXY_RESOLVER_H_

#include <stddef.h>

#include <memory>
#include <set>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_export.h"
#include "net/proxy_resolution/proxy_resolver_factory.h"

namespace net {
class ProxyResolver;

// MultiThreadedProxyResolverFactory creates instances of a ProxyResolver
// implementation that runs synchronous ProxyResolver implementations on worker
// threads.
//
// Threads are created lazily on demand, up to a maximum total. The advantage
// of having a pool of threads, is faster performance. In particular, being
// able to keep servicing PAC requests even if one blocks its execution.
//
// During initialization (CreateProxyResolver), a single thread is spun up to
// test the script. If this succeeds, we cache the input script, and will re-use
// this to lazily provision any new threads as needed.
//
// For each new thread that we spawn in a particular MultiThreadedProxyResolver
// instance, a corresponding new ProxyResolver is created using the
// ProxyResolverFactory returned by CreateProxyResolverFactory().
//
// Because we are creating multiple ProxyResolver instances, this means we
// are duplicating script contexts for what is ordinarily seen as being a
// single script. This can affect compatibility on some classes of PAC
// script:
//
// (a) Scripts whose initialization has external dependencies on network or
//     time may end up successfully initializing on some threads, but not
//     others. So depending on what thread services the request, the result
//     may jump between several possibilities.
//
// (b) Scripts whose FindProxyForURL() depends on side-effects may now
//     work differently. For example, a PAC script which was incrementing
//     a global counter and using that to make a decision. In the
//     multi-threaded model, each thread may have a different value for this
//     counter, so it won't globally be seen as monotonically increasing!
class NET_EXPORT_PRIVATE MultiThreadedProxyResolverFactory
    : public ProxyResolverFactory {
 public:
  MultiThreadedProxyResolverFactory(size_t max_num_threads,
                                    bool factory_expects_bytes);
  ~MultiThreadedProxyResolverFactory() override;

  int CreateProxyResolver(const scoped_refptr<PacFileData>& pac_script,
                          std::unique_ptr<ProxyResolver>* resolver,
                          CompletionOnceCallback callback,
                          std::unique_ptr<Request>* request) override;

 private:
  class Job;

  // Invoked to create a ProxyResolverFactory instance to pass to a
  // MultiThreadedProxyResolver instance.
  virtual std::unique_ptr<ProxyResolverFactory>
  CreateProxyResolverFactory() = 0;

  void RemoveJob(Job* job);

  const size_t max_num_threads_;

  std::set<raw_ptr<Job, SetExperimental>> jobs_;
};

}  // namespace net

#endif  // NET_PROXY_RESOLUTION_MULTI_THREADED_PROXY_RESOLVER_H_
