// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/test/mock_usb_mojo_device.h"

#include <utility>

namespace device {

MockUsbMojoDevice::~MockUsbMojoDevice() {}

MockUsbMojoDevice::MockUsbMojoDevice() {}

void MockUsbMojoDevice::IsochronousTransferIn(
    uint8_t endpoint_number,
    const std::vector<uint32_t>& packet_lengths,
    uint32_t timeout,
    IsochronousTransferInCallback callback) {
  std::vector<mojom::UsbIsochronousPacket> packets =
      IsochronousTransferInInternal(endpoint_number, packet_lengths, timeout);
  std::vector<mojom::UsbIsochronousPacketPtr> packet_ptrs;
  size_t total_length = 0;
  packet_ptrs.reserve(packets.size());
  for (const auto& packet : packets) {
    total_length += packet.length;
    packet_ptrs.push_back(packet.Clone());
  }
  std::move(callback).Run(std::vector<uint8_t>(total_length),
                          std::move(packet_ptrs));
}

void MockUsbMojoDevice::IsochronousTransferOut(
    uint8_t endpoint_number,
    const std::vector<uint8_t>& data,
    const std::vector<uint32_t>& packet_lengths,
    uint32_t timeout,
    IsochronousTransferOutCallback callback) {
  std::vector<mojom::UsbIsochronousPacket> packets =
      IsochronousTransferOutInternal(endpoint_number, data, packet_lengths,
                                     timeout);
  std::vector<mojom::UsbIsochronousPacketPtr> packet_ptrs;
  packet_ptrs.reserve(packets.size());
  for (const auto& packet : packets) {
    packet_ptrs.push_back(packet.Clone());
  }
  std::move(callback).Run(std::move(packet_ptrs));
}

}  // namespace device
