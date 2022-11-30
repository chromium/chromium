// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_RESOLVE_HOST_CLIENT_BASE_H_
#define SERVICES_NETWORK_PUBLIC_CPP_RESOLVE_HOST_CLIENT_BASE_H_

#include <string>
#include <vector>

#include "base/component_export.h"
#include "services/network/public/mojom/host_resolver.mojom.h"

namespace net {
class HostPortPair;
}

namespace network {

// Partial ResolveHostClient implementation with DCHECKing implementations for
// optional On...Result() methods.  Allows implementers to override just the
// methods for expected result types.
class COMPONENT_EXPORT(NETWORK_CPP) ResolveHostClientBase
    : public mojom::ResolveHostClient {
 public:
  void OnTextResults(const std::vector<std::string>& text_results) override;

  void OnHostnameResults(const std::vector<net::HostPortPair>& hosts) override;
};

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_RESOLVE_HOST_CLIENT_BASE_H_
