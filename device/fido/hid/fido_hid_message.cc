// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/hid/fido_hid_message.h"

#include <algorithm>
#include <numeric>
#include <utility>

#include "base/check_op.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "device/fido/fido_parsing_utils.h"

namespace device {

// static
std::optional<FidoHidMessage> FidoHidMessage::Create(
    uint32_t channel_id,
    FidoHidDeviceCommand type,
    size_t max_report_size,
    base::span<const uint8_t> data) {
  if (data.size() > kHidMaxMessageSize)
    return std::nullopt;

  // Unsupported devices should have been dropped in fido_hid_discovery.cc.
  CHECK_GT(max_report_size, kHidInitPacketHeaderSize);
  CHECK_LE(max_report_size, kHidMaxPacketSize);

  switch (type) {
    case FidoHidDeviceCommand::kPing:
      break;
    case FidoHidDeviceCommand::kMsg:
    case FidoHidDeviceCommand::kCbor: {
      if (data.empty())
        return std::nullopt;
      break;
    }

    case FidoHidDeviceCommand::kCancel:
    case FidoHidDeviceCommand::kWink: {
      if (!data.empty())
        return std::nullopt;
      break;
    }
    case FidoHidDeviceCommand::kLock: {
      if (data.size() != 1 || data[0] > kHidMaxLockSeconds)
        return std::nullopt;
      break;
    }
    case FidoHidDeviceCommand::kInit: {
      if (data.size() != 8)
        return std::nullopt;
      break;
    }
    case FidoHidDeviceCommand::kKeepAlive:
    case FidoHidDeviceCommand::kError:
      if (data.size() != 1)
        return std::nullopt;
  }

  return FidoHidMessage(channel_id, type, max_report_size, data);
}

// static
std::optional<FidoHidMessage> FidoHidMessage::CreateFromSerializedData(
    base::span<const uint8_t> serialized_data) {
  size_t remaining_size = 0;
  if (serialized_data.size() > kHidMaxPacketSize ||
      serialized_data.size() < kHidInitPacketHeaderSize)
    return std::nullopt;

  auto init_packet = FidoHidInitPacket::CreateFromSerializedData(
      serialized_data, &remaining_size);

  if (init_packet == nullptr)
    return std::nullopt;

  return FidoHidMessage(std::move(init_packet), remaining_size);
}

FidoHidMessage::FidoHidMessage(FidoHidMessage&& that) = default;

FidoHidMessage& FidoHidMessage::operator=(FidoHidMessage&& other) = default;

FidoHidMessage::~FidoHidMessage() = default;

bool FidoHidMessage::MessageComplete() const {
  return remaining_size_ == 0;
}

std::vector<uint8_t> FidoHidMessage::GetMessagePayload() const {
  std::vector<uint8_t> data;
  size_t data_size = 0;
  for (const auto& packet : packets_) {
    data_size += packet->GetPacketPayload().size();
  }
  data.reserve(data_size);

  for (const auto& packet : packets_) {
    const auto& packet_data = packet->GetPacketPayload();
    data.insert(std::end(data), packet_data.cbegin(), packet_data.cend());
  }

  return data;
}

std::vector<uint8_t> FidoHidMessage::PopNextPacket() {
  if (packets_.empty())
    return {};

  std::vector<uint8_t> data = packets_.front()->GetSerializedData();
  packets_.pop_front();
  return data;
}

bool FidoHidMessage::AddContinuationPacket(base::span<const uint8_t> buf) {
  size_t remaining_size = remaining_size_;
  auto cont_packet =
      FidoHidContinuationPacket::CreateFromSerializedData(buf, &remaining_size);

  // Reject packets with a different channel id.
  if (!cont_packet || channel_id_ != cont_packet->channel_id())
    return false;

  remaining_size_ = remaining_size;
  packets_.push_back(std::move(cont_packet));
  return true;
}

size_t FidoHidMessage::NumPackets() const {
  return packets_.size();
}

FidoHidMessage::FidoHidMessage(uint32_t channel_id,
                               FidoHidDeviceCommand type,
                               size_t max_report_size,
                               base::span<const uint8_t> data)
    : channel_id_(channel_id) {
  static_assert(
      kHidInitPacketHeaderSize >= kHidContinuationPacketHeaderSize,
      "init header is expected to be larger than continuation header");
  DCHECK_GT(max_report_size, kHidInitPacketHeaderSize);

  const size_t init_packet_data_size =
      max_report_size - kHidInitPacketHeaderSize;
  const size_t continuation_packet_data_size =
      max_report_size - kHidContinuationPacketHeaderSize;
  uint8_t sequence = 0;

  auto init_data = data.first(std::min(init_packet_data_size, data.size()));
  packets_.push_back(std::make_unique<FidoHidInitPacket>(
      channel_id, type,
      std::vector<uint8_t>(init_data.begin(), init_data.end()), data.size()));
  data = data.subspan(init_data.size());

  for (auto cont_data :
       fido_parsing_utils::SplitSpan(data, continuation_packet_data_size)) {
    packets_.push_back(std::make_unique<FidoHidContinuationPacket>(
        channel_id, sequence++, fido_parsing_utils::Materialize(cont_data)));
  }
}

FidoHidMessage::FidoHidMessage(std::unique_ptr<FidoHidInitPacket> init_packet,
                               size_t remaining_size)
    : remaining_size_(remaining_size) {
  channel_id_ = init_packet->channel_id();
  cmd_ = init_packet->command();
  packets_.push_back(std::move(init_packet));
}

}  // namespace device
