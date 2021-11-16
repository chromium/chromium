// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/connection_endpoint_metadata.h"

namespace net {

ConnectionEndpointMetadata::ConnectionEndpointMetadata() = default;
ConnectionEndpointMetadata::~ConnectionEndpointMetadata() = default;
ConnectionEndpointMetadata::ConnectionEndpointMetadata(
    const ConnectionEndpointMetadata&) = default;
ConnectionEndpointMetadata::ConnectionEndpointMetadata(
    ConnectionEndpointMetadata&&) = default;

}  // namespace net
