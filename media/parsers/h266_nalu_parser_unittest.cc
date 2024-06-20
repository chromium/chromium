// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/files/memory_mapped_file.h"
#include "base/logging.h"
#include "media/base/subsample_entry.h"
#include "media/base/test_data_util.h"
#include "media/parsers/h266_nalu_parser.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {
struct VvcTestData {
  std::string file_name;
  // Number of NALUs in the test stream to be parsed.
  int num_nalus;
};

}  // namespace

class H266NaluParserTest : public ::testing::Test {
 protected:
  void LoadParserFile(std::string file_name) {
    parser_.Reset();
    base::FilePath file_path = GetTestDataFilePath(file_name);
    stream_ = std::make_unique<base::MemoryMappedFile>();
    ASSERT_TRUE(stream_->Initialize(file_path))
        << "Couldn't open stream file: " << file_path.MaybeAsASCII();
    parser_.SetStream(stream_->data(), stream_->length());
  }
  bool ParseNalusUntilNut(H266NALU* target_nalu, H266NALU::Type nalu_type) {
    while (true) {
      H266NaluParser::Result res = parser_.AdvanceToNextNALU(target_nalu);
      if (res == H266NaluParser::kEndOfStream) {
        return false;
      }
      EXPECT_EQ(res, H266NaluParser::kOk);
      if (target_nalu->nal_unit_type == nalu_type) {
        return true;
      }
    }
  }
  H266NaluParser parser_;
  std::unique_ptr<base::MemoryMappedFile> stream_;
};

TEST_F(H266NaluParserTest, RawVvcStreamFileParsingShouldSucceed) {
  VvcTestData test_data[] = {
      {"bear_180p.vvc", 54},
      {"bbb_360p.vvc", 87},
  };
  for (const auto& data : test_data) {
    LoadParserFile(data.file_name);
    // Parse until the end of stream/unsupported stream/error in stream is
    // found.
    int num_parsed_nalus = 0;
    while (true) {
      H266NALU nalu;
      H266NaluParser::Result res = parser_.AdvanceToNextNALU(&nalu);
      if (res == H266NaluParser::kEndOfStream) {
        DVLOG(1) << "Number of successfully parsed NALUs before EOS: "
                 << num_parsed_nalus;
        EXPECT_EQ(data.num_nalus, num_parsed_nalus);
        break;
      }
      EXPECT_EQ(res, H266NaluParser::kOk);
      ++num_parsed_nalus;
      DVLOG(4) << "Found NALU " << nalu.nal_unit_type;
      EXPECT_EQ(res, H266NaluParser::kOk);
    }
  }
}

// Verify that GetCurrentSubsamples works.
TEST_F(H266NaluParserTest, GetCurrentSubsamplesNormalShouldSucceed) {
  constexpr uint8_t kStream[] = {
      // First NALU.
      // Clear bytes = 5.
      0x00,
      0x00,
      0x01,  // start code.
      0x00,
      0x41,  // Nalu type = 8, IDR_N_LP picture
      // Below is bogus data.
      // Encrypted bytes = 15.
      0x00,
      0x01,
      0x02,
      0x03,
      0x04,
      0x05,
      0x06,
      0x07,
      0x00,
      0x01,
      0x02,
      0x03,
      0x04,
      0x05,
      0x06,
      // Clear bytes = 5.
      0x07,
      0x00,
      0x01,
      0x02,
      0x03,
      // Encrypted until next NALU. Encrypted bytes = 20.
      0x04,
      0x05,
      0x06,
      0x07,
      0x00,
      0x01,
      0x02,
      0x03,
      0x04,
      0x05,
      0x06,
      0x07,
      // Note that this is still in the encrypted region but looks like a start
      // code.
      0x00,
      0x00,
      0x01,
      0x03,
      0x04,
      0x05,
      0x06,
      0x07,
      // Second NALU. Completely clear.
      // Clear bytes = 11.
      0x00,
      0x00,
      0x01,  // start code.
      0x00,
      0x79,  // nalu type = 15, SPS.
      // Bogus data.
      0xff,
      0xfe,
      0xfd,
      0xee,
      0x12,
      0x33,
  };
  std::vector<SubsampleEntry> subsamples;
  subsamples.emplace_back(5u, 15u);
  subsamples.emplace_back(5u, 20u);
  subsamples.emplace_back(11u, 0u);
  H266NaluParser parser;
  parser.SetEncryptedStream(kStream, std::size(kStream), subsamples);
  H266NALU nalu;
  EXPECT_EQ(H266NaluParser::kOk, parser.AdvanceToNextNALU(&nalu));
  EXPECT_EQ(H266NALU::kIDRNoLeadingPicture, nalu.nal_unit_type);
  auto nalu_subsamples = parser.GetCurrentSubsamples();
  EXPECT_EQ(2u, nalu_subsamples.size());
  // Note that nalu->data starts from the NALU header, i.e. does not include
  // the start code.
  EXPECT_EQ(2u, nalu_subsamples[0].clear_bytes);
  EXPECT_EQ(15u, nalu_subsamples[0].cypher_bytes);
  EXPECT_EQ(5u, nalu_subsamples[1].clear_bytes);
  EXPECT_EQ(20u, nalu_subsamples[1].cypher_bytes);
  // Make sure that it reached the next NALU.
  EXPECT_EQ(H266NaluParser::kOk, parser.AdvanceToNextNALU(&nalu));
  EXPECT_EQ(H266NALU::kSPS, nalu.nal_unit_type);
  nalu_subsamples = parser.GetCurrentSubsamples();
  EXPECT_EQ(1u, nalu_subsamples.size());
  EXPECT_EQ(8u, nalu_subsamples[0].clear_bytes);
  EXPECT_EQ(0u, nalu_subsamples[0].cypher_bytes);
}

// Verify that subsamples starting at non-NALU boundary also works.
TEST_F(H266NaluParserTest,
       GetCurrentSubsamplesSubsampleNotStartingAtNaluBoundaryShouldSucceed) {
  constexpr uint8_t kStream[] = {
      // First NALU.
      // Clear bytes = 5.
      0x00,
      0x00,
      0x01,  // start code.
      0x00,
      0x41,  // Nalu type = 8, IDR_N_LP slice.
      // Below is bogus data.
      // Encrypted bytes = 24.
      0x00,
      0x01,
      0x02,
      0x03,
      0x04,
      0x05,
      0x06,
      0x07,
      0x00,
      0x01,
      0x02,
      0x03,
      0x04,
      0x05,
      0x06,
      0x07,
      0x00,
      0x01,
      0x02,
      0x03,
      0x04,
      0x05,
      0x06,
      0x07,
      // Clear bytes = 19. The rest is in the clear. Note that this is not at
      // a NALU boundary and a NALU starts below.
      0xaa,
      0x01,
      0x02,
      0x03,
      0x04,
      0x05,
      0x06,
      0x07,
      // Second NALU. Completely clear.
      0x00,
      0x00,
      0x01,  // start code.
      0x00,
      0x79,  // nalu type = 15, SPS.
      // Bogus data.
      0xff,
      0xfe,
      0xfd,
      0xee,
      0x12,
      0x33,
  };
  std::vector<SubsampleEntry> subsamples;
  subsamples.emplace_back(5u, 24u);
  subsamples.emplace_back(19u, 0u);
  H266NaluParser parser;
  parser.SetEncryptedStream(kStream, std::size(kStream), subsamples);
  H266NALU nalu;
  EXPECT_EQ(H266NaluParser::kOk, parser.AdvanceToNextNALU(&nalu));
  EXPECT_EQ(H266NALU::kIDRNoLeadingPicture, nalu.nal_unit_type);
  auto nalu_subsamples = parser.GetCurrentSubsamples();
  EXPECT_EQ(2u, nalu_subsamples.size());
  // Note that nalu->data starts from the NALU header, i.e. does not include
  // the start code.
  EXPECT_EQ(2u, nalu_subsamples[0].clear_bytes);
  EXPECT_EQ(24u, nalu_subsamples[0].cypher_bytes);
  // The nalu ends with 8 more clear bytes. The last 10 bytes should be
  // associated with the next nalu.
  EXPECT_EQ(8u, nalu_subsamples[1].clear_bytes);
  EXPECT_EQ(0u, nalu_subsamples[1].cypher_bytes);
  EXPECT_EQ(H266NaluParser::kOk, parser.AdvanceToNextNALU(&nalu));
  EXPECT_EQ(H266NALU::kSPS, nalu.nal_unit_type);
  nalu_subsamples = parser.GetCurrentSubsamples();
  EXPECT_EQ(1u, nalu_subsamples.size());
  // Although the input had 10 more bytes, since nalu->data starts from the nalu
  // header, there's only 7 more bytes left.
  EXPECT_EQ(8u, nalu_subsamples[0].clear_bytes);
  EXPECT_EQ(0u, nalu_subsamples[0].cypher_bytes);
}

// Verify that nalus with invalid nuh_layer_id should be ignored.
TEST_F(H266NaluParserTest, NaluWithOutOfRangeNuhLayerIdShouldBeIgnored) {
  constexpr uint8_t kStream[] = {0x00, 0x00,
                                 0x01,  // start code.
                                 0x56,
                                 0x41,  // Nalu type = 8, IDR_N_LP slice.
                                 // Below is bogus data.
                                 0x00, 0x01, 0x02, 0x03, 0x04};
  H266NaluParser parser;
  parser.SetStream(kStream, std::size(kStream));
  H266NALU nalu;
  EXPECT_EQ(H266NaluParser::kIgnored, parser.AdvanceToNextNALU(&nalu));
}

// Verify that nalus with invalid nuh_temproal_id_plus1 should not be allowed.
TEST_F(H266NaluParserTest, ParseNaluWithInvalidTemporalIdShouldFail) {
  constexpr uint8_t kStream[] = {
      0x00, 0x00,
      0x01,  // start code.
      0x00,
      0x00,  // Nalu type = 0, TRAIL_NUT slice, but nuh_temporal_id_plus1 is 0
      // Below is bogus data.
      0x50, 0x01, 0x02, 0x03, 0x04,
      // Second Nalu.
      0x00, 0x00,
      0x01,  // start code.
      0x00,
      0x42,  // Nalu type = 8, IDR_N_LP slice, but nuh_temporal_id_plus1 is 2
      // Below is bogus data.
      0x00, 0x01, 0x02, 0x03, 0x04};
  H266NaluParser parser;
  parser.SetStream(kStream, std::size(kStream));
  H266NALU nalu;
  // First Nalu with nuh_temporal_id_plus1 of 0 is not allowed.
  EXPECT_EQ(H266NaluParser::kInvalidStream, parser.AdvanceToNextNALU(&nalu));
  // IRAP picture should be with nuh_temporal_id_plus1 of 1.
  EXPECT_EQ(H266NaluParser::kInvalidStream, parser.AdvanceToNextNALU(&nalu));
}

// Verify that non-IRAP pictures with nuh_temporal_id_plus1 not equal to 1
// should succeed.
TEST_F(H266NaluParserTest, ParseNonIrapNaluWithNonZeroTemporalIdShouldSucceed) {
  constexpr uint8_t kStream[] = {
      0x00, 0x00,
      0x01,  // start code.
      0x00,
      0x0e,  // Nalu type = 0, TRAIL_NUT slice, and nuh_temporal_id_plus1 is 6
      // Below is bogus data.
      0x94, 0x55, 0x48, 0x03, 0x04};
  H266NaluParser parser;
  parser.SetStream(kStream, std::size(kStream));
  H266NALU nalu;
  EXPECT_EQ(H266NaluParser::kOk, parser.AdvanceToNextNALU(&nalu));
}

}  // namespace media
