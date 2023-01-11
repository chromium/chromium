// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/functional/bind.h"
#include "base/time/time.h"
#include "media/base/media_util.h"
#include "media/base/stream_parser_buffer.h"
#include "media/formats/mp2t/es_parser_mpeg1audio.h"
#include "media/formats/mp2t/es_parser_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
class AudioDecoderConfig;

namespace mp2t {

class EsParserMpeg1AudioTest : public EsParserTestBase,
                               public testing::Test {
 public:
  EsParserMpeg1AudioTest();

  EsParserMpeg1AudioTest(const EsParserMpeg1AudioTest&) = delete;
  EsParserMpeg1AudioTest& operator=(const EsParserMpeg1AudioTest&) = delete;

 protected:
  bool Process(const std::vector<Packet>& pes_packets, bool force_timing);

 private:
  NullMediaLog media_log_;
};

EsParserMpeg1AudioTest::EsParserMpeg1AudioTest() {
}

bool EsParserMpeg1AudioTest::Process(
    const std::vector<Packet>& pes_packets,
    bool force_timing) {
  EsParserMpeg1Audio es_parser(
      base::BindRepeating(&EsParserMpeg1AudioTest::NewAudioConfig,
                          base::Unretained(this)),
      base::BindRepeating(&EsParserMpeg1AudioTest::EmitBuffer,
                          base::Unretained(this)),
      &media_log_);
  return ProcessPesPackets(&es_parser, pes_packets, force_timing);
}

TEST_F(EsParserMpeg1AudioTest, SinglePts) {
  LoadStream("sfx.mp3");

  std::vector<Packet> pes_packets = GenerateFixedSizePesPacket(512);
  pes_packets.front().pts = base::Seconds(10);

  // Note: there is no parsing of metadata as part of Mpeg2 TS,
  // so the tag starting at 0x80d with 0x54 0x41 0x47 (ascii for "TAG")
  // is not a valid Mpeg1 audio frame header. This makes the previous frame
  // invalid since there is no start code following the previous frame.
  // So instead of the 13 Mpeg1 audio frames, only 12 are considered valid.
  // Offset of frames in the file:
  // {0x20,  0x1c1, 0x277, 0x2f9, 0x3fd, 0x47f, 0x501, 0x583,
  //  0x605, 0x687, 0x73d, 0x7a5, 0x80d}
  // TODO(damienv): find a file that would be more relevant for Mpeg1 audio
  // as part of Mpeg2 TS.
  EXPECT_TRUE(Process(pes_packets, false));
  EXPECT_EQ(1u, config_count_);
  EXPECT_EQ(12u, buffer_count_);
}

TEST_F(EsParserMpeg1AudioTest, NoTimingInfo) {
  LoadStream("sfx.mp3");
  std::vector<Packet> pes_packets = GenerateFixedSizePesPacket(512);

  // Process should succeed even without timing info, we should just skip the
  // audio frames without timing info, but still should be able to parse and
  // play the stream after that.
  EXPECT_TRUE(Process(pes_packets, false));
  EXPECT_EQ(1u, config_count_);
  EXPECT_EQ(0u, buffer_count_);
}

}  // namespace mp2t
}  // namespace media
