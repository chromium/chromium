// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/core/http/http_encoder.h"
#include "net/third_party/quic/core/quic_data_writer.h"
#include "net/third_party/quic/platform/api/quic_bug_tracker.h"
#include "net/third_party/quic/platform/api/quic_fallthrough.h"
#include "net/third_party/quic/platform/api/quic_logging.h"
#include "net/third_party/quic/platform/api/quic_string.h"

namespace quic {

namespace {

// Set the first byte of a PRIORITY frame according to its fields.
uint8_t SetPriorityFields(uint8_t num,
                          PriorityElementType type,
                          bool prioritized) {
  switch (type) {
    case REQUEST_STREAM:
      return num;
    case PUSH_STREAM:
      if (prioritized) {
        return num | (1 << 6);
      }
      return num | (1 << 4);
    case PLACEHOLDER:
      if (prioritized) {
        return num | (1 << 7);
      }
      return num | (1 << 5);
    case ROOT_OF_TREE:
      if (prioritized) {
        num = num | (1 << 6);
        return num | (1 << 7);
      }
      num = num | (1 << 4);
      return num | (1 << 5);
    default:
      QUIC_NOTREACHED();
      return num;
  }
}

// Length of the type field of a frame.
static const size_t kFrameTypeLength = 1;
// Length of the weight field of a priority frame.
static const size_t kPriorityWeightLength = 1;
// Length of a priority frame's first byte.
static const size_t kPriorityFirstByteLength = 1;
// Length of a key in the map of a settings frame.
static const size_t kSettingsMapKeyLength = 2;

}  // namespace

HttpEncoder::HttpEncoder() {}

HttpEncoder::~HttpEncoder() {}

QuicByteCount HttpEncoder::SerializeDataFrameHeader(
    const DataFrame& data,
    std::unique_ptr<char[]>* output) {
  QuicByteCount header_length =
      QuicDataWriter::GetVarInt62Len(data.data.length()) + kFrameTypeLength;

  output->reset(new char[header_length]);
  QuicDataWriter writer(header_length, output->get(), NETWORK_BYTE_ORDER);

  if (WriteFrameHeader(data.data.length(), HttpFrameType::DATA, &writer)) {
    return header_length;
  }
  return 0;
}

QuicByteCount HttpEncoder::SerializeHeadersFrameHeader(
    const HeadersFrame& headers,
    std::unique_ptr<char[]>* output) {
  QuicByteCount header_length =
      QuicDataWriter::GetVarInt62Len(headers.headers.length()) +
      kFrameTypeLength;

  output->reset(new char[header_length]);
  QuicDataWriter writer(header_length, output->get(), NETWORK_BYTE_ORDER);

  if (WriteFrameHeader(headers.headers.length(), HttpFrameType::HEADERS,
                       &writer)) {
    return header_length;
  }
  return 0;
}

QuicByteCount HttpEncoder::SerializePriorityFrame(
    const PriorityFrame& priority,
    std::unique_ptr<char[]>* output) {
  QuicByteCount payload_length =
      kPriorityFirstByteLength +
      QuicDataWriter::GetVarInt62Len(priority.prioritized_element_id) +
      QuicDataWriter::GetVarInt62Len(priority.element_dependency_id) +
      kPriorityWeightLength;
  QuicByteCount total_length = GetTotalLength(payload_length);

  output->reset(new char[total_length]);
  QuicDataWriter writer(total_length, output->get(), NETWORK_BYTE_ORDER);

  if (!WriteFrameHeader(payload_length, HttpFrameType::PRIORITY, &writer)) {
    return 0;
  }

  // Set the first byte of the payload.
  uint8_t bits = 0;
  bits = SetPriorityFields(bits, priority.prioritized_type, true);
  bits = SetPriorityFields(bits, priority.dependency_type, false);
  if (priority.exclusive) {
    bits |= 1;
  }

  if (writer.WriteUInt8(bits) &&
      writer.WriteVarInt62(priority.prioritized_element_id) &&
      writer.WriteVarInt62(priority.element_dependency_id) &&
      writer.WriteUInt8(priority.weight)) {
    return total_length;
  }
  return 0;
}

QuicByteCount HttpEncoder::SerializeCancelPushFrame(
    const CancelPushFrame& cancel_push,
    std::unique_ptr<char[]>* output) {
  QuicByteCount payload_length =
      QuicDataWriter::GetVarInt62Len(cancel_push.push_id);
  QuicByteCount total_length = GetTotalLength(payload_length);

  output->reset(new char[total_length]);
  QuicDataWriter writer(total_length, output->get(), NETWORK_BYTE_ORDER);

  if (WriteFrameHeader(payload_length, HttpFrameType::CANCEL_PUSH, &writer) &&
      writer.WriteVarInt62(cancel_push.push_id)) {
    return total_length;
  }
  return 0;
}

QuicByteCount HttpEncoder::SerializeSettingsFrame(
    const SettingsFrame& settings,
    std::unique_ptr<char[]>* output) {
  // Calculate the key sizes.
  QuicByteCount payload_length = settings.values.size() * kSettingsMapKeyLength;
  // Calculate the value sizes.
  for (auto it = settings.values.begin(); it != settings.values.end(); ++it) {
    payload_length += QuicDataWriter::GetVarInt62Len(it->second);
  }

  QuicByteCount total_length = GetTotalLength(payload_length);

  output->reset(new char[total_length]);
  QuicDataWriter writer(total_length, output->get(), NETWORK_BYTE_ORDER);

  if (!WriteFrameHeader(payload_length, HttpFrameType::SETTINGS, &writer)) {
    return 0;
  }

  for (auto it = settings.values.begin(); it != settings.values.end(); ++it) {
    if (!writer.WriteUInt16(it->first) || !writer.WriteVarInt62(it->second)) {
      return 0;
    }
  }

  return total_length;
}

QuicByteCount HttpEncoder::SerializePushPromiseFrameWithOnlyPushId(
    const PushPromiseFrame& push_promise,
    std::unique_ptr<char[]>* output) {
  QuicByteCount payload_length =
      QuicDataWriter::GetVarInt62Len(push_promise.push_id) +
      push_promise.headers.length();
  // GetTotalLength() is not used because headers will not be serialized.
  QuicByteCount total_length =
      QuicDataWriter::GetVarInt62Len(payload_length) + kFrameTypeLength +
      QuicDataWriter::GetVarInt62Len(push_promise.push_id);

  output->reset(new char[total_length]);
  QuicDataWriter writer(total_length, output->get(), NETWORK_BYTE_ORDER);

  if (WriteFrameHeader(payload_length, HttpFrameType::PUSH_PROMISE, &writer) &&
      writer.WriteVarInt62(push_promise.push_id)) {
    return total_length;
  }
  return 0;
}

QuicByteCount HttpEncoder::SerializeGoAwayFrame(
    const GoAwayFrame& goaway,
    std::unique_ptr<char[]>* output) {
  QuicByteCount payload_length =
      QuicDataWriter::GetVarInt62Len(goaway.stream_id);
  QuicByteCount total_length = GetTotalLength(payload_length);

  output->reset(new char[total_length]);
  QuicDataWriter writer(total_length, output->get(), NETWORK_BYTE_ORDER);

  if (WriteFrameHeader(payload_length, HttpFrameType::GOAWAY, &writer) &&
      writer.WriteVarInt62(goaway.stream_id)) {
    return total_length;
  }
  return 0;
}

QuicByteCount HttpEncoder::SerializeMaxPushIdFrame(
    const MaxPushIdFrame& max_push_id,
    std::unique_ptr<char[]>* output) {
  QuicByteCount payload_length =
      QuicDataWriter::GetVarInt62Len(max_push_id.push_id);
  QuicByteCount total_length = GetTotalLength(payload_length);

  output->reset(new char[total_length]);
  QuicDataWriter writer(total_length, output->get(), NETWORK_BYTE_ORDER);

  if (WriteFrameHeader(payload_length, HttpFrameType::MAX_PUSH_ID, &writer) &&
      writer.WriteVarInt62(max_push_id.push_id)) {
    return total_length;
  }
  return 0;
}

bool HttpEncoder::WriteFrameHeader(QuicByteCount length,
                                   HttpFrameType type,
                                   QuicDataWriter* writer) {
  return writer->WriteVarInt62(length) &&
         writer->WriteUInt8(static_cast<uint8_t>(type));
}

QuicByteCount HttpEncoder::GetTotalLength(QuicByteCount payload_length) {
  return QuicDataWriter::GetVarInt62Len(payload_length) + kFrameTypeLength +
         payload_length;
}

}  // namespace quic
