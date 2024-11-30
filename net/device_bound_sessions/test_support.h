// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DEVICE_BOUND_SESSIONS_TEST_SUPPORT_H_
#define NET_DEVICE_BOUND_SESSIONS_TEST_SUPPORT_H_

#include <string>
#include <utility>

#include "base/containers/span.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

namespace net::device_bound_sessions {

// Return a hard-coded RS256 public key's SPKI bytes and JWK string for testing.
std::pair<base::span<const uint8_t>, std::string>
GetRS256SpkiAndJwkForTesting();

// Returns a request handler suitable for use with
// `EmbeddedTestServer`. The server allows registration of device bound
// sessions.
EmbeddedTestServer::HandleRequestCallback GetTestRequestHandler(
    const GURL& base_url);

}  // namespace net::device_bound_sessions

#endif  // NET_DEVICE_BOUND_SESSIONS_TEST_SUPPORT_H_
