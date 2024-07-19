// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "services/tracing/public/cpp/perfetto/trace_packet_tokenizer.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/trace_packet.h"
#include "third_party/perfetto/protos/perfetto/trace/trace.pb.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pb.h"

#include <list>

namespace tracing {
namespace {

constexpr char kTestString[] = "This little packet went to the market";

class TracePacketTokenizerTest : public testing::Test {
 protected:
  // Parse a trace chunk using an indirect memory allocation so ASAN can detect
  // any out-of-bounds reads.
  std::vector<perfetto::TracePacket> ParseChunk(const uint8_t* data,
                                                size_t size) {
    input_chunks_.emplace_back(data, data + size);
    auto& it = input_chunks_.back();
    return tokenizer_.Parse(it.data(), it.size());
  }

  void Reset() {
    input_chunks_.clear();
    tokenizer_ = TracePacketTokenizer();
  }

  TracePacketTokenizer& tokenizer() { return tokenizer_; }

 private:
  std::list<std::vector<uint8_t>> input_chunks_;
  TracePacketTokenizer tokenizer_;
};

TEST_F(TracePacketTokenizerTest, Basic) {
  perfetto::protos::Trace trace;
  auto* packet = trace.add_packet();
  packet->mutable_for_testing()->set_str(kTestString);
  auto packet_data = trace.SerializeAsString();

  auto packets = ParseChunk(
      reinterpret_cast<const uint8_t*>(packet_data.data()), packet_data.size());
  EXPECT_EQ(1u, packets.size());
  perfetto::protos::TracePacket parsed_packet;
  EXPECT_TRUE(
      parsed_packet.ParseFromString(packets[0].GetRawBytesForTesting()));
  EXPECT_EQ(kTestString, parsed_packet.for_testing().str());
  EXPECT_FALSE(tokenizer().has_more());
}

TEST_F(TracePacketTokenizerTest, PartialParse) {
  perfetto::protos::Trace trace;
  auto* packet = trace.add_packet();
  packet->mutable_for_testing()->set_str(kTestString);
  auto packet_data = trace.SerializeAsString();

  auto packets =
      ParseChunk(reinterpret_cast<const uint8_t*>(packet_data.data()),
                 packet_data.size() / 2);
  EXPECT_TRUE(packets.empty());
  EXPECT_TRUE(tokenizer().has_more());

  packets = ParseChunk(reinterpret_cast<const uint8_t*>(packet_data.data() +
                                                        packet_data.size() / 2),
                       packet_data.size() / 2);
  EXPECT_EQ(1u, packets.size());
  perfetto::protos::TracePacket parsed_packet;
  EXPECT_TRUE(
      parsed_packet.ParseFromString(packets[0].GetRawBytesForTesting()));
  EXPECT_EQ(kTestString, parsed_packet.for_testing().str());
  EXPECT_FALSE(tokenizer().has_more());
}

TEST_F(TracePacketTokenizerTest, MultiplePackets) {
  constexpr size_t kPacketCount = 32;
  perfetto::protos::Trace trace;
  for (size_t i = 0; i < kPacketCount; i++) {
    auto* packet = trace.add_packet();
    packet->set_timestamp(i);
    packet->mutable_for_testing()->set_str(kTestString);
  }
  auto packet_data = trace.SerializeAsString();

  auto packets = ParseChunk(
      reinterpret_cast<const uint8_t*>(packet_data.data()), packet_data.size());
  EXPECT_EQ(kPacketCount, packets.size());

  for (size_t i = 0; i < kPacketCount; i++) {
    perfetto::protos::TracePacket parsed_packet;
    EXPECT_TRUE(
        parsed_packet.ParseFromString(packets[i].GetRawBytesForTesting()));
    EXPECT_EQ(i, parsed_packet.timestamp());
    EXPECT_EQ(kTestString, parsed_packet.for_testing().str());
  }
  EXPECT_FALSE(tokenizer().has_more());
}

TEST_F(TracePacketTokenizerTest, Fragmentation) {
  constexpr size_t kPacketCount = 17;
  perfetto::protos::Trace trace;
  for (size_t i = 0; i < kPacketCount; i++) {
    auto* packet = trace.add_packet();
    packet->set_timestamp(i + 1);
    packet->mutable_for_testing()->set_str(kTestString);
  }
  auto packet_data = trace.SerializeAsString();

  for (size_t chunk_size = 1; chunk_size < packet_data.size(); chunk_size++) {
    size_t packet_count = 0;
    for (size_t offset = 0; offset < packet_data.size(); offset += chunk_size) {
      const auto* chunk_start =
          reinterpret_cast<const uint8_t*>(packet_data.data()) + offset;
      const auto* chunk_end =
          std::min(chunk_start + chunk_size,
                   reinterpret_cast<const uint8_t*>(&*packet_data.end()));
      auto packets = ParseChunk(chunk_start, chunk_end - chunk_start);
      if (packets.empty())
        continue;
      packet_count += packets.size();

      for (auto& packet : packets) {
        perfetto::protos::TracePacket parsed_packet;
        EXPECT_TRUE(
            parsed_packet.ParseFromString(packet.GetRawBytesForTesting()));
        EXPECT_GT(parsed_packet.timestamp(), 0u);
        EXPECT_EQ(kTestString, parsed_packet.for_testing().str());
      }
    }
    EXPECT_EQ(kPacketCount, packet_count);
    EXPECT_FALSE(tokenizer().has_more());
    Reset();
  }
}

}  // namespace
}  // namespace tracing
