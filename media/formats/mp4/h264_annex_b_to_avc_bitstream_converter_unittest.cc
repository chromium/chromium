// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/mp4/h264_annex_b_to_avc_bitstream_converter.h"

#include <stdint.h>

#include <memory>
#include <string>

#include "base/files/file.h"
#include "base/path_service.h"
#include "media/formats/mp4/avc.h"
#include "media/formats/mp4/box_definitions.h"
#include "media/parsers/h264_parser.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

std::vector<uint8_t> ReadTestFile(std::string name) {
  base::FilePath path;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &path);
  path = path.Append(FILE_PATH_LITERAL(
                         "media/formats/mp4/h264_annex_b_fuzz_corpus"))
             .AppendASCII(name);
  base::File f(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  DCHECK(f.IsValid()) << "path: " << path.AsUTF8Unsafe();
  auto size = f.GetLength();

  std::vector<uint8_t> result(size);
  if (size > 0)
    f.ReadAtCurrentPosAndCheck(result);
  return result;
}

}  // namespace

namespace media {

TEST(H264AnnexBToAvcBitstreamConverterTest, Success) {
  std::string chunks[] = {
      "chunk1-config-idr.bin", "chunk2-non-idr.bin",
      "chunk3-non-idr.bin",    "chunk4-non-idr.bin",
      "chunk5-non-idr.bin",    "chunk6-config-idr.bin",
      "chunk7-non-idr.bin",    "pps_neq_sps_config_idr.bin"};
  for (bool add_parameter_sets_in_bitstream : {false, true}) {
    H264AnnexBToAvcBitstreamConverter converter(
        add_parameter_sets_in_bitstream);

    for (std::string& name : chunks) {
      SCOPED_TRACE(name);
      auto input = ReadTestFile(name);
      SCOPED_TRACE(input.size());
      std::vector<uint8_t> output;
      size_t desired_size = 0;
      bool config_changed = false;

      auto status =
          converter.ConvertChunk(input, output, &config_changed, &desired_size);
      ASSERT_EQ(status.code(), MP4Status::Codes::kBufferTooSmall);
      output.resize(desired_size);

      status = converter.ConvertChunk(input, output, &config_changed, nullptr);
      EXPECT_TRUE(status.is_ok()) << status.message();

      // Convert AVC bitstream back to Annex-B bitstream, and validate if
      // parameter set are write to the output or not.
      EXPECT_TRUE(mp4::AVC::ConvertAVCToAnnexBInPlaceForLengthSize4(&output));
      H264Parser parser;
      parser.SetStream(output.data(), output.size());
      std::vector<H264NALU> parameter_sets;
      while (true) {
        H264NALU nalu;
        H264Parser::Result res = parser.AdvanceToNextNALU(&nalu);
        if (res == H264Parser::kEOStream) {
          break;
        }
        EXPECT_EQ(res, H264Parser::kOk);
        switch (nalu.nal_unit_type) {
          case H264NALU::kSPS:
            int sps_id;
            EXPECT_EQ(parser.ParseSPS(&sps_id), H264Parser::kOk);
            EXPECT_TRUE(!!parser.GetSPS(sps_id));
            parameter_sets.push_back(nalu);
            break;
          case H264NALU::kPPS:
            int pps_id;
            EXPECT_EQ(parser.ParsePPS(&pps_id), H264Parser::kOk);
            EXPECT_TRUE(!!parser.GetPPS(pps_id));
            parameter_sets.push_back(nalu);
            break;
          default:
            break;
        }
      }

      if (add_parameter_sets_in_bitstream && config_changed) {
        // Parameter sets should write to the output stream in the order of SPS,
        // PPS.
        EXPECT_EQ(parameter_sets.size(), 2u);
        EXPECT_EQ(parameter_sets.at(0).nal_unit_type, H264NALU::kSPS);
        EXPECT_EQ(parameter_sets.at(1).nal_unit_type, H264NALU::kPPS);
      } else {
        // Parameter sets should not write to the output stream.
        EXPECT_EQ(parameter_sets.size(), 0u);
      }

      auto& config = converter.GetCurrentConfig();
      if (name.find("config") != std::string::npos) {
        // Chunks with configuration
        EXPECT_TRUE(config_changed);

        EXPECT_EQ(config.version, 1);
        EXPECT_EQ(config.profile_indication, 66);
        EXPECT_EQ(config.profile_compatibility, 0xC0);
        EXPECT_EQ(config.avc_level, 30);
        EXPECT_EQ(config.length_size, 4);
        EXPECT_EQ(config.sps_list.size(), 1ul);
        EXPECT_EQ(config.pps_list.size(), 1ul);
      } else {
        EXPECT_FALSE(config_changed);
      }

      std::vector<uint8_t> config_bin;
      EXPECT_TRUE(config.Serialize(config_bin)) << " file: " << name;
    }
  }
}

// Tests that stream can contain multiple picture parameter sets and switch
// between them without having to reconfigure the decoder.
TEST(H264AnnexBToAvcBitstreamConverterTest, PPS_SwitchWithoutReconfig) {
  std::vector<uint8_t> sps{0x00, 0x00, 0x00, 0x01, 0x27, 0x42, 0x00, 0x1E,
                           0x89, 0x8A, 0x12, 0x05, 0x01, 0x7F, 0xCA, 0x80};
  std::vector<uint8_t> pps1{0x00, 0x00, 0x00, 0x01, 0x28, 0xce, 0x3c, 0x80};
  std::vector<uint8_t> pps2{0x00, 0x00, 0x00, 0x01, 0x28, 0x53, 0x8f, 0x20};
  std::vector<uint8_t> pps3{0x00, 0x00, 0x00, 0x01, 0x28, 0x73, 0x8F, 0x20};
  std::vector<uint8_t> first_frame_idr{
      0x00, 0x00, 0x00, 0x01, 0x25, 0xb4, 0x00, 0x10, 0x00, 0x24, 0xff,
      0xff, 0xf8, 0x7a, 0x28, 0x00, 0x08, 0x0a, 0x7b, 0xdd
      // Encoded data omitted here, it's not important for NALU parsing
  };

  std::vector<uint8_t> first_chunk;
  first_chunk.insert(first_chunk.end(), sps.begin(), sps.end());
  first_chunk.insert(first_chunk.end(), pps1.begin(), pps1.end());
  first_chunk.insert(first_chunk.end(), pps2.begin(), pps2.end());
  first_chunk.insert(first_chunk.end(), pps3.begin(), pps3.end());
  first_chunk.insert(first_chunk.end(), first_frame_idr.begin(),
                     first_frame_idr.end());

  std::vector<uint8_t> second_non_idr_chunk{
      0x00, 0x00, 0x00, 0x01, 0x21, 0xd8, 0x00, 0x80, 0x04,
      0x95, 0x9d, 0x45, 0x70, 0xd9, 0xbe, 0x21, 0xff, 0x87,
      0x20, 0x03, 0x9c, 0x66, 0x84, 0xe1
      // Encoded data omitted here, it's not important for NALU parsing
  };

  for (bool add_parameter_sets_in_bitstream : {false, true}) {
    H264AnnexBToAvcBitstreamConverter converter(
        add_parameter_sets_in_bitstream);
    std::vector<uint8_t> output(10000);
    bool config_changed = false;

    auto status =
        converter.ConvertChunk(first_chunk, output, &config_changed, nullptr);
    EXPECT_TRUE(status.is_ok()) << status.message();
    EXPECT_TRUE(config_changed);

    // Convert AVC bitstream back to Annex-B bitstream, and validate if
    // parameter set are write to the output or not.
    EXPECT_FALSE(mp4::AVC::ConvertAVCToAnnexBInPlaceForLengthSize4(&output));
    H264Parser parser;
    parser.SetStream(output.data(), output.size());
    std::vector<H264NALU> parameter_sets;
    while (true) {
      H264NALU nalu;
      H264Parser::Result res = parser.AdvanceToNextNALU(&nalu);
      if (res == H264Parser::kEOStream) {
        break;
      }
      EXPECT_EQ(res, H264Parser::kOk);
      switch (nalu.nal_unit_type) {
        case H264NALU::kSPS:
          int sps_id;
          EXPECT_EQ(parser.ParseSPS(&sps_id), H264Parser::kOk);
          EXPECT_TRUE(!!parser.GetSPS(sps_id));
          parameter_sets.push_back(nalu);
          break;
        case H264NALU::kPPS:
          int pps_id;
          EXPECT_EQ(parser.ParsePPS(&pps_id), H264Parser::kOk);
          EXPECT_TRUE(!!parser.GetPPS(pps_id));
          parameter_sets.push_back(nalu);
          break;
        default:
          break;
      }
    }

    if (add_parameter_sets_in_bitstream) {
      // Parameter sets should write to the output stream in the order of SPS,
      // PPS1, PPS2, PPS3.
      EXPECT_EQ(parameter_sets.size(), 4u);
      EXPECT_EQ(parameter_sets.at(0).nal_unit_type, H264NALU::kSPS);
      EXPECT_EQ(parameter_sets.at(1).nal_unit_type, H264NALU::kPPS);
      EXPECT_EQ(parameter_sets.at(2).nal_unit_type, H264NALU::kPPS);
      EXPECT_EQ(parameter_sets.at(3).nal_unit_type, H264NALU::kPPS);
    } else {
      // Parameter sets should not write to the output stream.
      EXPECT_EQ(parameter_sets.size(), 0u);
    }

    status = converter.ConvertChunk(second_non_idr_chunk, output,
                                    &config_changed, nullptr);
    EXPECT_TRUE(status.is_ok()) << status.message();
    EXPECT_FALSE(config_changed);
  }
}

// Tests that REXT metadata is handled correctly.
TEST(H264AnnexBToAvcBitstreamConverterTest, REXT) {
  std::vector<uint8_t> sps{0,   0,  0, 1,   39,  100, 0, 31, 172, 86,
                           128, 80, 5, 186, 106, 12,  2, 12, 4};
  std::vector<uint8_t> spsext{0, 0, 0, 1, 45, 208};
  std::vector<uint8_t> pps{0, 0, 0, 1, 40, 238, 60, 176};
  std::vector<uint8_t> idr{
      0, 0, 0, 1, 37, 184, 32, 1, 239, 27, 89, 188, 127, 248, 98, 186,
      // Encoded data omitted here, it's not important for NALU parsing
  };

  std::vector<uint8_t> chunk;
  chunk.insert(chunk.end(), sps.begin(), sps.end());
  chunk.insert(chunk.end(), spsext.begin(), spsext.end());
  chunk.insert(chunk.end(), pps.begin(), pps.end());
  chunk.insert(chunk.end(), idr.begin(), idr.end());

  for (bool add_parameter_sets_in_bitstream : {false, true}) {
    H264AnnexBToAvcBitstreamConverter converter(
        add_parameter_sets_in_bitstream);
    std::vector<uint8_t> output(10000);
    bool config_changed = false;

    auto status =
        converter.ConvertChunk(chunk, output, &config_changed, nullptr);
    EXPECT_TRUE(status.is_ok()) << status.message();
    EXPECT_TRUE(config_changed);

    // Convert AVC bitstream back to Annex-B bitstream, and validate if
    // parameter set are write to the output or not.
    EXPECT_FALSE(mp4::AVC::ConvertAVCToAnnexBInPlaceForLengthSize4(&output));
    H264Parser parser;
    parser.SetStream(output.data(), output.size());
    std::vector<H264NALU> parameter_sets;
    while (true) {
      H264NALU nalu;
      H264Parser::Result res = parser.AdvanceToNextNALU(&nalu);
      if (res == H264Parser::kEOStream) {
        break;
      }
      EXPECT_EQ(res, H264Parser::kOk);
      switch (nalu.nal_unit_type) {
        case H264NALU::kSPS: {
          int sps_id;
          EXPECT_EQ(parser.ParseSPS(&sps_id), H264Parser::kOk);
          EXPECT_TRUE(!!parser.GetSPS(sps_id));
          parameter_sets.push_back(nalu);
          break;
        }
        case H264NALU::kSPSExt: {
          int sps_id;
          EXPECT_EQ(parser.ParseSPSExt(&sps_id), H264Parser::kOk);
          EXPECT_TRUE(!!parser.GetSPS(sps_id));
          parameter_sets.push_back(nalu);
          break;
        }
        case H264NALU::kPPS: {
          int pps_id;
          EXPECT_EQ(parser.ParsePPS(&pps_id), H264Parser::kOk);
          EXPECT_TRUE(!!parser.GetPPS(pps_id));
          parameter_sets.push_back(nalu);
          break;
        }
        default:
          break;
      }
    }

    if (add_parameter_sets_in_bitstream) {
      // Parameter sets should write to the output stream in the order of SPS,
      // SPS Extension, PPS.
      EXPECT_EQ(parameter_sets.size(), 3u);
      EXPECT_EQ(parameter_sets.at(0).nal_unit_type, H264NALU::kSPS);
      EXPECT_EQ(parameter_sets.at(1).nal_unit_type, H264NALU::kSPSExt);
      EXPECT_EQ(parameter_sets.at(2).nal_unit_type, H264NALU::kPPS);
    } else {
      // Parameter sets should not write to the output stream.
      EXPECT_EQ(parameter_sets.size(), 0u);
    }

    auto& config = converter.GetCurrentConfig();
    EXPECT_EQ(config.version, 1);
    EXPECT_EQ(config.profile_indication, 100);
    EXPECT_EQ(config.profile_compatibility, 0);
    EXPECT_EQ(config.avc_level, 31);
    EXPECT_EQ(config.length_size, 4);
    EXPECT_EQ(config.sps_list.size(), 1ul);
    EXPECT_EQ(config.pps_list.size(), 1ul);
    EXPECT_EQ(config.chroma_format, 1);
    EXPECT_EQ(config.bit_depth_luma_minus8, 0);
    EXPECT_EQ(config.bit_depth_chroma_minus8, 0);
    EXPECT_EQ(config.sps_ext_list.size(), 1ul);
  }
}

TEST(H264AnnexBToAvcBitstreamConverterTest, Failure) {
  for (bool add_parameter_sets_in_bitstream : {false, true}) {
    H264AnnexBToAvcBitstreamConverter converter(
        add_parameter_sets_in_bitstream);

    std::vector<uint8_t> input{0x0, 0x0, 0x0, 0x1, 0x9,  0x10,
                               0x0, 0x0, 0x0, 0x1, 0x67, 0x42};
    std::vector<uint8_t> output(input.size());

    auto status = converter.ConvertChunk(input, output, nullptr, nullptr);

    ASSERT_EQ(status.code(), MP4Status::Codes::kInvalidSPS);
  }
}

}  // namespace media
