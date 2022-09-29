// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_FUZZED_HOST_RESOLVER_UTIL_H_
#define NET_DNS_FUZZED_HOST_RESOLVER_UTIL_H_

#include <memory>

#include "net/dns/host_resolver.h"

class FuzzedDataProvider;

namespace net {

class ContextHostResolver;
class NetLog;

// Creates a ContextHostResolver that uses a fuzzer to determine what results to
// return. It inherits from ContextHostResolver, unlike MockHostResolver, so
// more closely matches real behavior.
//
// By default uses a mocked out system resolver, though can be configured to use
// the built-in async resolver (Built in DNS stub resolver) with a fuzzed set
// of UDP/TCP sockets by setting ManagerOptions.insecure_dns_client_enabled to
// true or calling SetInsecureDnsClientEnabled on the underlying
// HostResolverManager.
//
// To make behavior most deterministic, it is recommended that tests and fuzzers
// run all system DNS resolution on the current thread.
//
// Note that it does not attempt to sort the resulting AddressList when using
// the mock system resolver path.
//
// The async DNS client can make system calls in AddressSorterPosix, but other
// methods that make system calls are stubbed out.
std::unique_ptr<ContextHostResolver> CreateFuzzedContextHostResolver(
    const HostResolver::ManagerOptions& options,
    NetLog* net_log,
    FuzzedDataProvider* data_provider,
    bool enable_caching);

}  // namespace net

#endif  // NET_DNS_FUZZED_HOST_RESOLVER_UTIL_H_
