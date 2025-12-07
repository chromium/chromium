// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/perfetto/trace_packet_tokenizer.h"

#include "base/check.h"
#include "base/check_op.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/trace_packet.h"

namespace tracing {
namespace {

static constexpr uint8_t kPacketTag =
    protozero::proto_utils::MakeTagLengthDelimited(
        perfetto::TracePacket::kPacketFieldNumber);

}  // namespace

TracePacketTokenizer::TracePacketTokenizer() = default;
TracePacketTokenizer::~TracePacketTokenizer() = default;
TracePacketTokenizer::Packet::Packet() = default;
TracePacketTokenizer::Packet::~Packet() = default;

std::vector<perfetto::TracePacket> TracePacketTokenizer::Parse(
    base::span<const uint8_t> data) {
  std::vector<perfetto::TracePacket> packets;
  // Only one fragmented packet can be finalized per call to Parse(), so clear
  // any previous one.
  assembled_packet_ = Packet();

  while (!data.empty()) {
    // First parse the packet header, i.e., the one byte field tag and the
    // variable sized packet length field.
    if (!next_packet_.parsed_size) {
      // Parse the field tag.
      auto prev_header_size = next_packet_.header.size();
      auto bytes_to_copy =
          data.first(std::min(kMaxHeaderSize - prev_header_size, data.size()));
      next_packet_.header.insert(next_packet_.header.end(),
                                 bytes_to_copy.begin(), bytes_to_copy.end());
      DCHECK(next_packet_.header.size() <= kMaxHeaderSize);
      if (next_packet_.header.size() < kMinHeaderSize) {
        // Not enough data -- try again later.
        return packets;
      }
      DCHECK_EQ(kPacketTag, next_packet_.header[0]);

      // Parse the size field.
      const auto* size_begin = &next_packet_.header[1];
      const auto* size_end = protozero::proto_utils::ParseVarInt(
          size_begin, &*next_packet_.header.end(), &next_packet_.parsed_size);
      size_t size_field_size = size_end - size_begin;
      if (!size_field_size) {
        // Size field overflows to next chunk. Try again later.
        return packets;
      }
      // Find the start of the packet data after the size field.
      data =
          data.subspan(sizeof(kPacketTag) + size_field_size - prev_header_size);
    }

    // We've now parsed the the proto preamble and the size field for our
    // packet. Let's see if the packet fits completely into this chunk.
    DCHECK(next_packet_.parsed_size);
    size_t remaining_size =
        next_packet_.parsed_size - next_packet_.partial_data.size();
    if (remaining_size > data.size()) {
      // Save remaining bytes into overflow buffer and try again later.
      next_packet_.partial_data.insert(next_packet_.partial_data.end(),
                                       data.begin(), data.end());
      return packets;
    }

    // The packet is now complete. It can have a slice overflowing from the
    // previous chunk(s) as well a a slice in the current chunk.
    packets.emplace_back();
    if (!next_packet_.partial_data.empty()) {
      DCHECK(assembled_packet_.partial_data.empty());
      assembled_packet_ = std::move(next_packet_);
      packets.back().AddSlice(&assembled_packet_.partial_data[0],
                              assembled_packet_.partial_data.size());
    }
    auto remaining_data = data.take_first(remaining_size);
    packets.back().AddSlice(remaining_data.data(), remaining_data.size());

    // Start a new packet.
    next_packet_ = Packet();
  }
  DCHECK(data.empty());
  return packets;
}

}  // namespace tracing
