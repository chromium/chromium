// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/proxy_chain.h"

#include <ostream>

#include "net/base/proxy_string_util.h"

namespace net {

std::string ProxyChain::ToDebugString() const {
  return ProxyServerToProxyUri(proxy_server());
}

bool ProxyChain::IsValid() const {
  return proxy_server().is_valid();
}

std::ostream& operator<<(std::ostream& os, const ProxyChain& proxy_chain) {
  return os << proxy_chain.proxy_server();
}

}  // namespace net
