// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/preconnect_request.h"

namespace content {

PreconnectRequest::PreconnectRequest(
    const url::Origin& origin,
    int num_sockets,
    const net::NetworkAnonymizationKey& network_anonymization_key)
    : origin(origin),
      num_sockets(num_sockets),
      network_anonymization_key(network_anonymization_key) {
  CHECK_GE(num_sockets, 0);
  CHECK(!network_anonymization_key.IsEmpty() ||
        !net::NetworkAnonymizationKey::IsPartitioningEnabled());
}

}  // namespace content
