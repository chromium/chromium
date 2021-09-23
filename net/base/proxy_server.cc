// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/proxy_server.h"

#include "base/check.h"

namespace net {

ProxyServer::ProxyServer(Scheme scheme, const HostPortPair& host_port_pair)
      : scheme_(scheme), host_port_pair_(host_port_pair) {
  if (scheme_ == SCHEME_DIRECT || scheme_ == SCHEME_INVALID) {
    // |host_port_pair| isn't relevant for these special schemes, so none should
    // have been specified. It is important for this to be consistent since we
    // do raw field comparisons in the equality and comparison functions.
    DCHECK(host_port_pair.Equals(HostPortPair()));
    host_port_pair_ = HostPortPair();
  }
}

const HostPortPair& ProxyServer::host_port_pair() const {
  // Doesn't make sense to call this if the URI scheme doesn't
  // have concept of a host.
  DCHECK(is_valid());
  DCHECK(!is_direct());
  return host_port_pair_;
}

// static
int ProxyServer::GetDefaultPortForScheme(Scheme scheme) {
  switch (scheme) {
    case SCHEME_HTTP:
      return 80;
    case SCHEME_SOCKS4:
    case SCHEME_SOCKS5:
      return 1080;
    case SCHEME_HTTPS:
    case SCHEME_QUIC:
      return 443;
    case SCHEME_INVALID:
    case SCHEME_DIRECT:
      break;
  }
  return -1;
}

}  // namespace net
