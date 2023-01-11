// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/functional/bind.h"
#include "base/time/time.h"
#include "media/base/stream_parser_buffer.h"
#include "media/formats/mp2t/es_parser_adts.h"
#include "media/formats/mp2t/es_parser_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
class AudioDecoderConfig;

namespace mp2t {
namespace {
const char kAac44100PacketTimestamp[] = "(0) (23) (46) (69)";
}

class EsParserAdtsTest : public EsParserTestBase,
                         public testing::Test {
 public:
  EsParserAdtsTest();

  EsParserAdtsTest(const EsParserAdtsTest&) = delete;
  EsParserAdtsTest& operator=(const EsParserAdtsTest&) = delete;

 protected:
  bool Process(const std::vector<Packet>& pes_packets, bool sbr_in_mimetype);
};

EsParserAdtsTest::EsParserAdtsTest() {
}

bool EsParserAdtsTest::Process(const std::vector<Packet>& pes_packets,
                               bool sbr_in_mimetype) {
  EsParserAdts es_parser(base::BindRepeating(&EsParserAdtsTest::NewAudioConfig,
                                             base::Unretained(this)),
                         base::BindRepeating(&EsParserAdtsTest::EmitBuffer,
                                             base::Unretained(this)),
                         sbr_in_mimetype);
  return ProcessPesPackets(&es_parser, pes_packets, false /* force_timing */);
}

TEST_F(EsParserAdtsTest, NoInitialPts) {
  LoadStream("bear.adts");
  std::vector<Packet> pes_packets = GenerateFixedSizePesPacket(512);
  // Process should succeed even without timing info, we should just skip the
  // audio frames without timing info, but still should be able to parse and
  // play the stream after that.
  EXPECT_TRUE(Process(pes_packets, false /* sbr_in_mimetype */));
  EXPECT_EQ(1u, config_count_);
  EXPECT_EQ(0u, buffer_count_);
}

TEST_F(EsParserAdtsTest, SinglePts) {
  LoadStream("bear.adts");

  std::vector<Packet> pes_packets = GenerateFixedSizePesPacket(512);
  pes_packets.front().pts = base::Seconds(10);

  EXPECT_TRUE(Process(pes_packets, false /* sbr_in_mimetype */));
  EXPECT_EQ(1u, config_count_);
  EXPECT_EQ(45u, buffer_count_);
}

TEST_F(EsParserAdtsTest, AacLcAdts) {
  LoadStream("sfx.adts");
  std::vector<Packet> pes_packets = GenerateFixedSizePesPacket(512);
  pes_packets.front().pts = base::Seconds(1);
  EXPECT_TRUE(Process(pes_packets, false /* sbr_in_mimetype */));
  EXPECT_EQ(1u, config_count_);
  EXPECT_EQ(14u, buffer_count_);
}

TEST_F(EsParserAdtsTest, AacSampleRate) {
  std::vector<Packet> pes_packets =
      LoadPacketsFromFiles("aac-44100-packet-%d", 4);

  pes_packets.front().pts = base::Seconds(0);
  EXPECT_TRUE(Process(pes_packets, true /* sbr_in_mimetype */));
  EXPECT_EQ(4u, buffer_count_);
  EXPECT_EQ(kAac44100PacketTimestamp, buffer_timestamps_);
}
}  // namespace mp2t
}  // namespace media
