// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A simple class for resolving hostname synchronously.

#ifndef NET_TOOLS_QUIC_SYNCHRONOUS_HOST_RESOLVER_H_
#define NET_TOOLS_QUIC_SYNCHRONOUS_HOST_RESOLVER_H_

#include "net/base/address_list.h"
#include "net/dns/host_resolver.h"
#include "url/scheme_host_port.h"

namespace net {

class SynchronousHostResolver {
 public:
  static int Resolve(url::SchemeHostPort scheme_host_port,
                     AddressList* addresses);
};

}  // namespace net

#endif  // NET_TOOLS_QUIC_SYNCHRONOUS_HOST_RESOLVER_H_
