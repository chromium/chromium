// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_handle.h"

#include "base/check_is_test.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"

namespace net::handles {

namespace {
bool g_emulate_network_binding_for_testing = false;
}  // namespace

void SetEmulateNetworkBindingForTesting(bool enabled) {
  g_emulate_network_binding_for_testing = enabled;
}

bool GetEmulateNetworkBindingForTesting() {
  return g_emulate_network_binding_for_testing;
}

IPEndPoint MaybeTranslateEmulatedNetworkAddressForTesting(
    const IPEndPoint& address,
    NetworkHandle target_network) {
  if (GetEmulateNetworkBindingForTesting() &&
      target_network != kInvalidNetworkHandle &&
      address.address() == IPAddress::IPv4Localhost()) [[unlikely]] {
    CHECK_IS_TEST();
    CHECK_GE(target_network, 2);
    CHECK_LE(target_network, 254);
    return IPEndPoint(
        IPAddress(127, 0, 0, static_cast<uint8_t>(target_network)),
        address.port());
  }
  return address;
}

}  // namespace net::handles
