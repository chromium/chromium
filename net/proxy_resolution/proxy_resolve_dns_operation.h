// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_RESOLUTION_PROXY_RESOLVE_DNS_OPERATION_H_
#define NET_PROXY_RESOLUTION_PROXY_RESOLVE_DNS_OPERATION_H_

namespace net {

// TODO(mmenke): Remove this enum in favor of
// proxy_resolver.mojom.HostResolveOperation.
enum class ProxyResolveDnsOperation {
  DNS_RESOLVE,
  DNS_RESOLVE_EX,
  MY_IP_ADDRESS,
  MY_IP_ADDRESS_EX,
};

}  // namespace net

#endif  // NET_PROXY_RESOLUTION_PROXY_RESOLVE_DNS_OPERATION_H_
