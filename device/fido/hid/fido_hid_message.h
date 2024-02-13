// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_HID_FIDO_HID_MESSAGE_H_
#define DEVICE_FIDO_HID_FIDO_HID_MESSAGE_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <optional>
#include <vector>

#include "base/component_export.h"
#include "base/containers/circular_deque.h"
#include "base/containers/queue.h"
#include "base/containers/span.h"
#include "device/fido/fido_constants.h"
#include "device/fido/hid/fido_hid_packet.h"

namespace device {

// Represents HID message format defined by the specification at
// https://fidoalliance.org/specs/fido-v2.0-rd-20161004/fido-client-to-authenticator-protocol-v2.0-rd-20161004.html#message-and-packet-structure
class COMPONENT_EXPORT(DEVICE_FIDO) FidoHidMessage {
 public:
  // Static functions to create CTAP/U2F HID commands.
  static std::optional<FidoHidMessage> Create(uint32_t channel_id,
                                              FidoHidDeviceCommand cmd,
                                              size_t max_report_size,
                                              base::span<const uint8_t> data);

  // Reconstruct a message from serialized message data.
  static std::optional<FidoHidMessage> CreateFromSerializedData(
      base::span<const uint8_t> serialized_data);

  FidoHidMessage(FidoHidMessage&& that);
  FidoHidMessage& operator=(FidoHidMessage&& other);

  FidoHidMessage(const FidoHidMessage&) = delete;
  FidoHidMessage& operator=(const FidoHidMessage&) = delete;

  ~FidoHidMessage();

  bool MessageComplete() const;
  std::vector<uint8_t> GetMessagePayload() const;
  // Pop front of queue with next packet.
  std::vector<uint8_t> PopNextPacket();
  // Adds a continuation packet to the packet list, from the serialized
  // response value.
  bool AddContinuationPacket(base::span<const uint8_t> packet_buf);

  size_t NumPackets() const;
  uint32_t channel_id() const { return channel_id_; }
  FidoHidDeviceCommand cmd() const { return cmd_; }
  const base::circular_deque<std::unique_ptr<FidoHidPacket>>&
  GetPacketsForTesting() const {
    return packets_;
  }

 private:
  FidoHidMessage(uint32_t channel_id,
                 FidoHidDeviceCommand type,
                 size_t max_report_size,
                 base::span<const uint8_t> data);
  FidoHidMessage(std::unique_ptr<FidoHidInitPacket> init_packet,
                 size_t remaining_size);
  uint32_t channel_id_ = kHidBroadcastChannel;
  FidoHidDeviceCommand cmd_ = FidoHidDeviceCommand::kMsg;
  base::circular_deque<std::unique_ptr<FidoHidPacket>> packets_;
  size_t remaining_size_ = 0;
};

}  // namespace device

#endif  // DEVICE_FIDO_HID_FIDO_HID_MESSAGE_H_
