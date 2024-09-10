// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/files/memory_mapped_file.h"
#include "base/logging.h"
#include "media/base/subsample_entry.h"
#include "media/base/test_data_util.h"
#include "media/parsers/h265_nalu_parser.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {
struct HevcTestData {
  std::string file_name;
  // Number of NALUs in the test stream to be parsed.
  int num_nalus;
};

}  // namespace

class H265NaluParserTest : public ::testing::Test {
 protected:
  void LoadParserFile(std::string file_name) {
    parser_.Reset();
    base::FilePath file_path = GetTestDataFilePath(file_name);

    stream_ = std::make_unique<base::MemoryMappedFile>();
    ASSERT_TRUE(stream_->Initialize(file_path))
        << "Couldn't open stream file: " << file_path.MaybeAsASCII();

    parser_.SetStream(stream_->data(), stream_->length());
  }

  bool ParseNalusUntilNut(H265NALU* target_nalu, H265NALU::Type nalu_type) {
    while (true) {
      H265NaluParser::Result res = parser_.AdvanceToNextNALU(target_nalu);
      if (res == H265NaluParser::kEOStream) {
        return false;
      }
      EXPECT_EQ(res, H265NaluParser::kOk);
      if (target_nalu->nal_unit_type == nalu_type)
        return true;
    }
  }

  H265NaluParser parser_;
  std::unique_ptr<base::MemoryMappedFile> stream_;
};

TEST_F(H265NaluParserTest, RawHevcStreamFileParsing) {
  HevcTestData test_data[] = {
      {"bear.hevc", 35},
      {"bbb.hevc", 64},
  };

  for (const auto& data : test_data) {
    LoadParserFile(data.file_name);
    // Parse until the end of stream/unsupported stream/error in stream is
    // found.
    int num_parsed_nalus = 0;
    while (true) {
      H265NALU nalu;
      H265NaluParser::Result res = parser_.AdvanceToNextNALU(&nalu);
      if (res == H265NaluParser::kEOStream) {
        DVLOG(1) << "Number of successfully parsed NALUs before EOS: "
                 << num_parsed_nalus;
        EXPECT_EQ(data.num_nalus, num_parsed_nalus);
        break;
      }
      EXPECT_EQ(res, H265NaluParser::kOk);

      ++num_parsed_nalus;
      DVLOG(4) << "Found NALU " << nalu.nal_unit_type;

      EXPECT_EQ(res, H265NaluParser::kOk);
    }
  }
}

// Verify that GetCurrentSubsamples works.
TEST_F(H265NaluParserTest, GetCurrentSubsamplesNormal) {
  constexpr uint8_t kStream[] = {
      // First NALU.
      // Clear bytes = 5.
      0x00, 0x00, 0x01,  // start code.
      0x28, 0x01,        // Nalu type = 20, IDR slice.
      // Below is bogus data.
      // Encrypted bytes = 15.
      0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x00, 0x01, 0x02, 0x03,
      0x04, 0x05, 0x06,
      // Clear bytes = 5.
      0x07, 0x00, 0x01, 0x02, 0x03,
      // Encrypted until next NALU. Encrypted bytes = 20.
      0x04, 0x05, 0x06, 0x07, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
      // Note that this is still in the encrypted region but looks like a start
      // code.
      0x00, 0x00, 0x01, 0x03, 0x04, 0x05, 0x06, 0x07,
      // Second NALU. Completely clear.
      // Clear bytes = 11.
      0x00, 0x00, 0x01,  // start code.
      0x42, 0x01,        // nalu type = 33, SPS.
      // Bogus data.
      0xff, 0xfe, 0xfd, 0xee, 0x12, 0x33,
  };
  std::vector<SubsampleEntry> subsamples;
  subsamples.emplace_back(5u, 15u);
  subsamples.emplace_back(5u, 20u);
  subsamples.emplace_back(11u, 0u);
  H265NaluParser parser;
  parser.SetEncryptedStream(kStream, std::size(kStream), subsamples);

  H265NALU nalu;
  EXPECT_EQ(H265NaluParser::kOk, parser.AdvanceToNextNALU(&nalu));
  EXPECT_EQ(H265NALU::IDR_N_LP, nalu.nal_unit_type);
  auto nalu_subsamples = parser.GetCurrentSubsamples();
  EXPECT_EQ(2u, nalu_subsamples.size());

  // Note that nalu->data starts from the NALU header, i.e. does not include
  // the start code.
  EXPECT_EQ(2u, nalu_subsamples[0].clear_bytes);
  EXPECT_EQ(15u, nalu_subsamples[0].cypher_bytes);
  EXPECT_EQ(5u, nalu_subsamples[1].clear_bytes);
  EXPECT_EQ(20u, nalu_subsamples[1].cypher_bytes);

  // Make sure that it reached the next NALU.
  EXPECT_EQ(H265NaluParser::kOk, parser.AdvanceToNextNALU(&nalu));
  EXPECT_EQ(H265NALU::SPS_NUT, nalu.nal_unit_type);
  nalu_subsamples = parser.GetCurrentSubsamples();
  EXPECT_EQ(1u, nalu_subsamples.size());

  EXPECT_EQ(8u, nalu_subsamples[0].clear_bytes);
  EXPECT_EQ(0u, nalu_subsamples[0].cypher_bytes);
}

// Verify that subsamples starting at non-NALU boundary also works.
TEST_F(H265NaluParserTest,
       GetCurrentSubsamplesSubsampleNotStartingAtNaluBoundary) {
  constexpr uint8_t kStream[] = {
      // First NALU.
      // Clear bytes = 5.
      0x00, 0x00, 0x01,  // start code.
      0x28, 0x01,        // Nalu type = 20, IDR slice.
      // Below is bogus data.
      // Encrypted bytes = 24.
      0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x00, 0x01, 0x02, 0x03,
      0x04, 0x05, 0x06, 0x07, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
      // Clear bytes = 19. The rest is in the clear. Note that this is not at
      // a NALU boundary and a NALU starts below.
      0xaa, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
      // Second NALU. Completely clear.
      0x00, 0x00, 0x01,  // start code.
      0x42, 0x01,        // nalu type = 33, SPS.
      // Bogus data.
      0xff, 0xfe, 0xfd, 0xee, 0x12, 0x33,
  };

  std::vector<SubsampleEntry> subsamples;
  subsamples.emplace_back(5u, 24u);
  subsamples.emplace_back(19u, 0u);
  H265NaluParser parser;
  parser.SetEncryptedStream(kStream, std::size(kStream), subsamples);

  H265NALU nalu;
  EXPECT_EQ(H265NaluParser::kOk, parser.AdvanceToNextNALU(&nalu));
  EXPECT_EQ(H265NALU::IDR_N_LP, nalu.nal_unit_type);
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

  EXPECT_EQ(H265NaluParser::kOk, parser.AdvanceToNextNALU(&nalu));
  EXPECT_EQ(H265NALU::SPS_NUT, nalu.nal_unit_type);
  nalu_subsamples = parser.GetCurrentSubsamples();
  EXPECT_EQ(1u, nalu_subsamples.size());

  // Although the input had 10 more bytes, since nalu->data starts from the nalu
  // header, there's only 7 more bytes left.
  EXPECT_EQ(8u, nalu_subsamples[0].clear_bytes);
  EXPECT_EQ(0u, nalu_subsamples[0].cypher_bytes);
}

}  // namespace media
