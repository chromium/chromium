// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_DIRECT_SOCKETS_TEST_HELPERS_H_
#define CONTENT_PUBLIC_TEST_DIRECT_SOCKETS_TEST_HELPERS_H_

#include <stddef.h>

#include <string>
#include <vector>

namespace content {

// Returns `count` IPv4 unicast addresses on the same subnet as one of the
// machine's non-loopback interfaces but distinct from the machine's own
// address, for use as source addresses in source-specific multicast (SSM)
// tests. Returns an empty vector when the machine has no non-loopback IPv4
// interface; callers should skip the test in that case. The machine's own
// address is avoided because using it as an SSM source would cause macOS to
// route via lo0, making getsockname() return 127.0.0.1 which is excluded from
// GetNetworkList(), breaking interface detection for MCAST_JOIN_SOURCE_GROUP.
std::vector<std::string> DeriveSsmSourceAddresses(size_t count);

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_DIRECT_SOCKETS_TEST_HELPERS_H_
