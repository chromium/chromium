// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <array>

#include "base/containers/span.h"
#include "base/memory/ref_counted.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/fido/cable/fido_cable_device.h"
#include "device/fido/cable/fido_cable_handshake_handler.h"
#include "device/fido/fido_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr std::array<uint8_t, 32> kTestPSKGeneratorKey = {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
};
constexpr std::array<uint8_t, 8> kTestNonce = {1, 2, 3, 4, 5, 6, 7, 8};
constexpr std::array<uint8_t, 16> kTestEphemeralID = {
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
constexpr std::array<uint8_t, 65> kTestPeerIdentity = {
    0x04, 0x67, 0x80, 0xc5, 0xfc, 0x70, 0x27, 0x5e, 0x2c, 0x70, 0x61,
    0xa0, 0xe7, 0x87, 0x7b, 0xb1, 0x74, 0xde, 0xad, 0xeb, 0x98, 0x87,
    0x02, 0x7f, 0x3f, 0xa8, 0x36, 0x54, 0x15, 0x8b, 0xa7, 0xf5, 0x0c,
    0x3c, 0xba, 0x8c, 0x34, 0xbc, 0x35, 0xd2, 0x0e, 0x81, 0xf7, 0x30,
    0xac, 0x1c, 0x7b, 0xd6, 0xd6, 0x61, 0xa9, 0x42, 0xf9, 0x0c, 0x6a,
    0x9c, 0xa5, 0x5c, 0x51, 0x2f, 0x9e, 0x4a, 0x00, 0x12, 0x66,
};
constexpr char kTestDeviceAddress[] = "Fake_Address";

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* raw_data, size_t size) {
  auto input = base::make_span(raw_data, size);
  auto adapter =
      base::MakeRefCounted<::testing::NiceMock<device::MockBluetoothAdapter>>();
  device::FidoCableDevice test_cable_device(adapter.get(), kTestDeviceAddress);
  test_cable_device.SetStateForTesting(
      device::FidoCableDevice::State::kDeviceError);

  base::Optional<base::span<const uint8_t, 65>> peer_identity;
  if (!input.empty() && (input[0] & 1)) {
    peer_identity = kTestPeerIdentity;
    input = input.subspan(1);
  }

  device::FidoCableV2HandshakeHandler handshake_handler_v2(
      &test_cable_device, kTestPSKGeneratorKey, kTestNonce, kTestEphemeralID,
      peer_identity, base::DoNothing());
  handshake_handler_v2.InitiateCableHandshake(base::DoNothing());
  handshake_handler_v2.ValidateAuthenticatorHandshakeMessage(input);
  return 0;
}
