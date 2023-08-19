// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_PERFETTO_TRACE_PACKET_TOKENIZER_H_
#define SERVICES_TRACING_PUBLIC_CPP_PERFETTO_TRACE_PACKET_TOKENIZER_H_

#include <vector>

#include "base/component_export.h"
#include "third_party/abseil-cpp/absl/container/inlined_vector.h"
#include "third_party/perfetto/include/perfetto/protozero/proto_utils.h"

namespace perfetto {
class TracePacket;
}  // namespace perfetto

namespace tracing {

// Converts between a raw stream of perfetto::TracePacket bytes and a tokenized
// vector of delineated packets.
//
// The tracing service provides us with serialized proto bytes, while the
// Perfetto consumer expects a vector of TracePackets (i.e., without the field
// number and size header) without partial or truncated packets. To translate
// between the two, we find the position of each TracePacket and construct a
// vector pointing to the original data at the corresponding offsets.
//
// To make matters more complicated, mojo can split the data chunks
// arbitrarily, including in the middle of trace packets. To work around
// this, we tokenize as much data as we can and buffer any unprocessed bytes as
// long as needed.
class COMPONENT_EXPORT(TRACING_CPP) TracePacketTokenizer {
 public:
  TracePacketTokenizer();
  ~TracePacketTokenizer();

  // Given a chunk of trace data, tokenize as many trace packets as possible
  // (could be zero) and return the result. Note that the tokenized packets have
  // pointers to |data| as well as |this|, so they won't be valid after another
  // call to Parse().
  std::vector<perfetto::TracePacket> Parse(const uint8_t* data, size_t size);

  // Returns |true| if there is more data left to be consumed in the tokenizer.
  bool has_more() const {
    return !next_packet_.header.empty() || next_packet_.parsed_size ||
           !next_packet_.partial_data.empty();
  }

 private:
  static constexpr size_t kMinHeaderSize = sizeof(uint8_t) + sizeof(uint8_t);
  static constexpr size_t kMaxHeaderSize =
      sizeof(uint8_t) + protozero::proto_utils::kMessageLengthFieldSize;

  struct Packet {
    Packet();
    ~Packet();

    // Most trace packets are very small, so avoid heap allocations in the
    // common case where one packet straddles the boundary between chunks.
    absl::InlinedVector<uint8_t, 64> partial_data;

    uint64_t parsed_size = 0;
    absl::InlinedVector<uint8_t, kMaxHeaderSize> header;
  };

  // Packet currently being parsed.
  Packet next_packet_;

  // Most recently completed packet which spanned multiple chunks. Kept as a
  // member so that the memory remains valid while the caller is processing the
  // results.
  Packet assembled_packet_;
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_PERFETTO_TRACE_PACKET_TOKENIZER_H_
