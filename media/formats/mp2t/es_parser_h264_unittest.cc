// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/mp2t/es_parser_h264.h"

#include <stddef.h>
#include <stdint.h>

#include <sstream>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "media/base/stream_parser_buffer.h"
#include "media/formats/mp2t/es_parser_test_base.h"
#include "media/parsers/h264_parser.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
class VideoDecoderConfig;

namespace mp2t {

class EsParserH264Test : public EsParserTestBase,
                         public testing::Test {
 public:
  EsParserH264Test() {}

  EsParserH264Test(const EsParserH264Test&) = delete;
  EsParserH264Test& operator=(const EsParserH264Test&) = delete;

 protected:
  void LoadH264Stream(const char* filename);
  void GetPesTimestamps(std::vector<Packet>* pes_packets);
  bool Process(const std::vector<Packet>& pes_packets, bool force_timing);
  void CheckAccessUnits();

  // Access units of the stream with AUD NALUs.
  std::vector<Packet> access_units_;

 private:
  // Get the offset of the start of each access unit of |stream_|.
  // This function assumes there is only one slice per access unit.
  // This is a very simplified access unit segmenter that is good
  // enough for unit tests.
  void GetAccessUnits();

  // Insert an AUD before each access unit.
  // Update |stream_| and |access_units_| accordingly.
  void InsertAUD();
};

void EsParserH264Test::LoadH264Stream(const char* filename) {
  // Load the input H264 file and segment it into access units.
  LoadStream(filename);
  GetAccessUnits();
  ASSERT_GT(access_units_.size(), 0u);

  // Insert AUDs into the stream.
  InsertAUD();

  // Generate some timestamps based on a 25fps stream.
  for (size_t k = 0; k < access_units_.size(); k++)
    access_units_[k].pts = base::Milliseconds(k * 40u);
}

void EsParserH264Test::GetAccessUnits() {
  access_units_.resize(0);
  bool start_access_unit = true;

  // In a first pass, retrieve the offsets of all access units.
  size_t offset = 0;
  while (true) {
    // Find the next start code.
    off_t relative_offset = 0;
    off_t start_code_size = 0;
    bool success = H264Parser::FindStartCode(
        &stream_[offset], stream_.size() - offset,
        &relative_offset, &start_code_size);
    if (!success)
      break;
    offset += relative_offset;

    if (start_access_unit) {
      Packet cur_access_unit;
      cur_access_unit.offset = offset;
      access_units_.push_back(cur_access_unit);
      start_access_unit = false;
    }

    // Get the NALU type.
    offset += start_code_size;
    if (offset >= stream_.size())
      break;
    int nal_unit_type = stream_[offset] & 0x1f;

    // We assume there is only one slice per access unit.
    if (nal_unit_type == H264NALU::kIDRSlice ||
        nal_unit_type == H264NALU::kNonIDRSlice) {
      start_access_unit = true;
    }
  }

  ComputePacketSize(&access_units_);
}

void EsParserH264Test::InsertAUD() {
  uint8_t aud[] = {0x00, 0x00, 0x01, 0x09};

  std::vector<uint8_t> stream_with_aud(stream_.size() +
                                       access_units_.size() * sizeof(aud));
  std::vector<EsParserTestBase::Packet> access_units_with_aud(
      access_units_.size());

  size_t offset = 0;
  for (size_t k = 0; k < access_units_.size(); k++) {
    access_units_with_aud[k].offset = offset;
    access_units_with_aud[k].size = access_units_[k].size + sizeof(aud);

    memcpy(&stream_with_aud[offset], aud, sizeof(aud));
    offset += sizeof(aud);

    memcpy(&stream_with_aud[offset],
           &stream_[access_units_[k].offset], access_units_[k].size);
    offset += access_units_[k].size;
  }

  // Update the stream and access units used for the test.
  stream_ = stream_with_aud;
  access_units_ = access_units_with_aud;
}

void EsParserH264Test::GetPesTimestamps(std::vector<Packet>* pes_packets_ptr) {
  DCHECK(pes_packets_ptr);
  const std::vector<Packet>& pes_packets = *pes_packets_ptr;

  // Default: set to a negative timestamp to be able to differentiate from
  // real timestamps.
  // Note: we don't use kNoTimestamp here since this one has already
  // a special meaning in EsParserH264. The negative timestamps should be
  // ultimately discarded by the H264 parser since not relevant.
  for (size_t k = 0; k < pes_packets.size(); k++) {
    (*pes_packets_ptr)[k].pts = base::Milliseconds(-1);
  }

  // Set a valid timestamp for PES packets which include the start
  // of an H264 access unit.
  size_t pes_idx = 0;
  for (size_t k = 0; k < access_units_.size(); k++) {
    for (; pes_idx < pes_packets.size(); pes_idx++) {
      size_t pes_start = pes_packets[pes_idx].offset;
      size_t pes_end = pes_packets[pes_idx].offset + pes_packets[pes_idx].size;
      if (pes_start <= access_units_[k].offset &&
          pes_end > access_units_[k].offset) {
        (*pes_packets_ptr)[pes_idx].pts = access_units_[k].pts;
        break;
      }
    }
  }
}

bool EsParserH264Test::Process(
    const std::vector<Packet>& pes_packets,
    bool force_timing) {
  EsParserH264 es_parser(base::BindRepeating(&EsParserH264Test::NewVideoConfig,
                                             base::Unretained(this)),
                         base::BindRepeating(&EsParserH264Test::EmitBuffer,
                                             base::Unretained(this)));
  return ProcessPesPackets(&es_parser, pes_packets, force_timing);
}

void EsParserH264Test::CheckAccessUnits() {
  EXPECT_EQ(buffer_count_, access_units_.size());

  std::stringstream buffer_timestamps_stream;
  for (size_t k = 0; k < access_units_.size(); k++) {
    buffer_timestamps_stream << "("
                             << access_units_[k].pts.InMilliseconds()
                             << ") ";
  }
  std::string buffer_timestamps = buffer_timestamps_stream.str();
  base::TrimWhitespaceASCII(
      buffer_timestamps, base::TRIM_ALL, &buffer_timestamps);
  EXPECT_EQ(buffer_timestamps_, buffer_timestamps);
}

TEST_F(EsParserH264Test, OneAccessUnitPerPes) {
  LoadH264Stream("bear.h264");

  // One to one equivalence between PES packets and access units.
  std::vector<Packet> pes_packets(access_units_);
  GetPesTimestamps(&pes_packets);

  // Process each PES packet.
  EXPECT_TRUE(Process(pes_packets, false));
  CheckAccessUnits();
}

TEST_F(EsParserH264Test, NonAlignedPesPacket) {
  LoadH264Stream("bear.h264");

  // Generate the PES packets.
  std::vector<Packet> pes_packets;
  Packet cur_pes_packet;
  cur_pes_packet.offset = 0;
  for (size_t k = 0; k < access_units_.size(); k++) {
    pes_packets.push_back(cur_pes_packet);

    // The current PES packet includes the remaining bytes of the previous
    // access unit and some bytes of the current access unit
    // (487 bytes in this unit test but no more than the current access unit
    // size).
    cur_pes_packet.offset = access_units_[k].offset +
        std::min<size_t>(487u, access_units_[k].size);
  }
  ComputePacketSize(&pes_packets);
  GetPesTimestamps(&pes_packets);

  // Process each PES packet.
  EXPECT_TRUE(Process(pes_packets, false));
  CheckAccessUnits();
}

TEST_F(EsParserH264Test, SeveralPesPerAccessUnit) {
  LoadH264Stream("bear.h264");

  // Get the minimum size of an access unit.
  size_t min_access_unit_size = stream_.size();
  for (size_t k = 0; k < access_units_.size(); k++) {
    if (min_access_unit_size >= access_units_[k].size)
      min_access_unit_size = access_units_[k].size;
  }

  // Use a small PES packet size or the minimum access unit size
  // if it is even smaller.
  size_t pes_size = 512;
  if (min_access_unit_size < pes_size)
    pes_size = min_access_unit_size;

  std::vector<Packet> pes_packets;
  Packet cur_pes_packet;
  cur_pes_packet.offset = 0;
  while (cur_pes_packet.offset < stream_.size()) {
    pes_packets.push_back(cur_pes_packet);
    cur_pes_packet.offset += pes_size;
  }
  ComputePacketSize(&pes_packets);
  GetPesTimestamps(&pes_packets);

  // Process each PES packet.
  EXPECT_TRUE(Process(pes_packets, false));
  CheckAccessUnits();

  // Process PES packets forcing timings for each PES packet.
  EXPECT_TRUE(Process(pes_packets, true));
  CheckAccessUnits();
}

}  // namespace mp2t
}  // namespace media
