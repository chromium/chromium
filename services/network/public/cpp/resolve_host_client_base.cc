// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/resolve_host_client_base.h"

#include "base/notreached.h"
#include "net/base/host_port_pair.h"

namespace network {

void ResolveHostClientBase::OnTextResults(
    const std::vector<std::string>& text_results) {
  // Should be overridden if text results are expected.
  NOTREACHED();
}

void ResolveHostClientBase::OnHostnameResults(
    const std::vector<net::HostPortPair>& hosts) {
  // Should be overridden if hostname results are expected.
  NOTREACHED();
}

}  // namespace network
