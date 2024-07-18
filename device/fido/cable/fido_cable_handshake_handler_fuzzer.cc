// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

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

constexpr std::array<uint8_t, 32> kTestSessionPreKey = {{
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
}};

constexpr std::array<uint8_t, 8> kTestNonce = {{
    0x15, 0x14, 0x13, 0x12, 0x11, 0x10, 0x09, 0x08,
}};

constexpr char kTestDeviceAddress[] = "Fake_Address";

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* raw_data, size_t size) {
  auto data_span = base::make_span(raw_data, size);
  auto adapter =
      base::MakeRefCounted<::testing::NiceMock<device::MockBluetoothAdapter>>();
  device::FidoCableDevice test_cable_device(adapter.get(), kTestDeviceAddress);

  device::FidoCableV1HandshakeHandler handshake_handler_v1(
      &test_cable_device, kTestNonce, kTestSessionPreKey);
  handshake_handler_v1.ValidateAuthenticatorHandshakeMessage(data_span);
  return 0;
}
