// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/hid/fido_hid_packet.h"

#include <algorithm>
#include <utility>

#include "base/containers/contains.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"

namespace device {

FidoHidPacket::FidoHidPacket(std::vector<uint8_t> data, uint32_t channel_id)
    : data_(std::move(data)), channel_id_(channel_id) {}

FidoHidPacket::FidoHidPacket() = default;

FidoHidPacket::~FidoHidPacket() = default;

// static
std::unique_ptr<FidoHidInitPacket> FidoHidInitPacket::CreateFromSerializedData(
    base::span<const uint8_t> serialized,
    size_t* remaining_size) {
  if (serialized.size() <= kHidInitPacketHeaderSize)
    return nullptr;

  size_t index = 0;
  auto channel_id = (serialized[index++] & 0xff) << 24;
  channel_id |= (serialized[index++] & 0xff) << 16;
  channel_id |= (serialized[index++] & 0xff) << 8;
  channel_id |= serialized[index++] & 0xff;

  auto command = static_cast<FidoHidDeviceCommand>(serialized[index++] & 0x7f);
  if (!base::Contains(GetFidoHidDeviceCommandList(), command))
    return nullptr;

  uint16_t payload_size = serialized[index++] << 8;
  payload_size |= serialized[index++];
  DCHECK_EQ(index, kHidInitPacketHeaderSize);

  // Check to see if payload is less than maximum size and padded with 0s.
  uint16_t data_size =
      std::min(payload_size, static_cast<uint16_t>(serialized.size() - index));

  // Update remaining size to determine the payload size of follow on packets.
  *remaining_size = payload_size - data_size;

  auto data = std::vector<uint8_t>(serialized.begin() + index,
                                   serialized.begin() + index + data_size);

  return std::make_unique<FidoHidInitPacket>(channel_id, command,
                                             std::move(data), payload_size);
}

// U2F Initialization packet is defined as:
// Offset Length
// 0      4       Channel ID
// 4      1       Command ID
// 5      1       High order packet payload size
// 6      1       Low order packet payload size
// 7      (s-7)   Payload data
FidoHidInitPacket::FidoHidInitPacket(uint32_t channel_id,
                                     FidoHidDeviceCommand cmd,
                                     std::vector<uint8_t> data,
                                     uint16_t payload_length)
    : FidoHidPacket(std::move(data), channel_id),
      command_(cmd),
      payload_length_(payload_length) {}

FidoHidInitPacket::~FidoHidInitPacket() = default;

std::vector<uint8_t> FidoHidInitPacket::GetSerializedData() const {
  std::vector<uint8_t> serialized;
  serialized.reserve(kHidMaxPacketSize);
  serialized.push_back((channel_id_ >> 24) & 0xff);
  serialized.push_back((channel_id_ >> 16) & 0xff);
  serialized.push_back((channel_id_ >> 8) & 0xff);
  serialized.push_back(channel_id_ & 0xff);
  serialized.push_back(base::strict_cast<uint8_t>(command_) | 0x80);
  serialized.push_back((payload_length_ >> 8) & 0xff);
  serialized.push_back(payload_length_ & 0xff);
  serialized.insert(serialized.end(), data_.begin(), data_.end());

  return serialized;
}

// static
std::unique_ptr<FidoHidContinuationPacket>
FidoHidContinuationPacket::CreateFromSerializedData(
    base::span<const uint8_t> serialized,
    size_t* remaining_size) {
  if (serialized.size() <= kHidContinuationPacketHeaderSize)
    return nullptr;

  size_t index = 0;
  auto channel_id = (serialized[index++] & 0xff) << 24;
  channel_id |= (serialized[index++] & 0xff) << 16;
  channel_id |= (serialized[index++] & 0xff) << 8;
  channel_id |= serialized[index++] & 0xff;
  auto sequence = serialized[index++];
  DCHECK_EQ(index, kHidContinuationPacketHeaderSize);

  // Check to see if packet payload is less than maximum size and padded with
  // 0s.
  size_t data_size = std::min(*remaining_size, serialized.size() - index);
  *remaining_size -= data_size;
  auto data = std::vector<uint8_t>(serialized.begin() + index,
                                   serialized.begin() + index + data_size);

  return std::make_unique<FidoHidContinuationPacket>(channel_id, sequence,
                                                     std::move(data));
}

// U2F Continuation packet is defined as:
// Offset Length
// 0      4       Channel ID
// 4      1       Packet sequence 0x00..0x7f
// 5      (s-5)   Payload data
FidoHidContinuationPacket::FidoHidContinuationPacket(const uint32_t channel_id,
                                                     const uint8_t sequence,
                                                     std::vector<uint8_t> data)
    : FidoHidPacket(std::move(data), channel_id), sequence_(sequence) {}

FidoHidContinuationPacket::~FidoHidContinuationPacket() = default;

std::vector<uint8_t> FidoHidContinuationPacket::GetSerializedData() const {
  std::vector<uint8_t> serialized;
  serialized.reserve(kHidMaxPacketSize);
  serialized.push_back((channel_id_ >> 24) & 0xff);
  serialized.push_back((channel_id_ >> 16) & 0xff);
  serialized.push_back((channel_id_ >> 8) & 0xff);
  serialized.push_back(channel_id_ & 0xff);
  serialized.push_back(sequence_);
  serialized.insert(serialized.end(), data_.begin(), data_.end());

  return serialized;
}

}  // namespace device
