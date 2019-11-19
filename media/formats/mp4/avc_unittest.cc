// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <ostream>

#include "base/optional.h"
#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "media/base/decrypt_config.h"
#include "media/base/stream_parser_buffer.h"
#include "media/formats/mp4/avc.h"
#include "media/formats/mp4/bitstream_converter.h"
#include "media/formats/mp4/box_definitions.h"
#include "media/formats/mp4/nalu_test_helper.h"
#include "media/video/h264_parser.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace mp4 {

static const uint8_t kNALU1[] = {0x01, 0x02, 0x03};
static const uint8_t kNALU2[] = {0x04, 0x05, 0x06, 0x07};
static const uint8_t kExpected[] = {0x00, 0x00, 0x00, 0x01, 0x01,
                                    0x02, 0x03, 0x00, 0x00, 0x00,
                                    0x01, 0x04, 0x05, 0x06, 0x07};

static const uint8_t kExpectedParamSets[] = {
    0x00, 0x00, 0x00, 0x01, 0x67, 0x12, 0x00, 0x00, 0x00, 0x01,
    0x67, 0x34, 0x00, 0x00, 0x00, 0x01, 0x68, 0x56, 0x78};

static std::string NALUTypeToString(int type) {
  switch (type) {
    case H264NALU::kNonIDRSlice:
      return "P";
    case H264NALU::kSliceDataA:
      return "SDA";
    case H264NALU::kSliceDataB:
      return "SDB";
    case H264NALU::kSliceDataC:
      return "SDC";
    case H264NALU::kIDRSlice:
      return "I";
    case H264NALU::kSEIMessage:
      return "SEI";
    case H264NALU::kSPS:
      return "SPS";
    case H264NALU::kSPSExt:
      return "SPSExt";
    case H264NALU::kPPS:
      return "PPS";
    case H264NALU::kAUD:
      return "AUD";
    case H264NALU::kEOSeq:
      return "EOSeq";
    case H264NALU::kEOStream:
      return "EOStr";
    case H264NALU::kFiller:
      return "FILL";
    case H264NALU::kReserved14:
      return "R14";

    case H264NALU::kUnspecified:
    case H264NALU::kReserved15:
    case H264NALU::kReserved16:
    case H264NALU::kReserved17:
    case H264NALU::kReserved18:
    case H264NALU::kCodedSliceAux:
    case H264NALU::kCodedSliceExtension:
      CHECK(false) << "Unexpected type: " << type;
      break;
  };

  return "UnsupportedType";
}

// Helper output operator, for debugging/testability.
std::ostream& operator<<(std::ostream& os,
                         const BitstreamConverter::AnalysisResult& r) {
  os << "{ is_conformant: "
     << (r.is_conformant.has_value()
             ? (r.is_conformant.value() ? "true" : "false")
             : "nullopt/unknown")
     << ", is_keyframe: "
     << (r.is_keyframe.has_value() ? (r.is_keyframe.value() ? "true" : "false")
                                   : "nullopt/unknown")
     << " }";
  return os;
}

static std::string AnnexBToString(
    const std::vector<uint8_t>& buffer,
    const std::vector<SubsampleEntry>& subsamples) {
  std::stringstream ss;

  H264Parser parser;
  parser.SetEncryptedStream(&buffer[0], buffer.size(), subsamples);

  H264NALU nalu;
  bool first = true;
  size_t current_subsample_index = 0;
  while (parser.AdvanceToNextNALU(&nalu) == H264Parser::kOk) {
    size_t subsample_index = AVC::FindSubsampleIndex(buffer, &subsamples,
                                                     nalu.data);
    if (!first) {
      ss << (subsample_index == current_subsample_index ? "," : " ");
    } else {
      DCHECK_EQ(subsample_index, current_subsample_index);
      first = false;
    }

    ss << NALUTypeToString(nalu.nal_unit_type);
    current_subsample_index = subsample_index;
  }
  return ss.str();
}

class AVCConversionTest : public testing::TestWithParam<int> {
 protected:
  void WriteLength(int length_size, int length, std::vector<uint8_t>* buf) {
    DCHECK_GE(length, 0);
    DCHECK_LE(length, 255);

    for (int i = 1; i < length_size; i++)
      buf->push_back(0);
    buf->push_back(length);
  }

  void MakeInputForLength(int length_size, std::vector<uint8_t>* buf) {
    buf->clear();

    WriteLength(length_size, sizeof(kNALU1), buf);
    buf->insert(buf->end(), kNALU1, kNALU1 + sizeof(kNALU1));

    WriteLength(length_size, sizeof(kNALU2), buf);
    buf->insert(buf->end(), kNALU2, kNALU2 + sizeof(kNALU2));
  }

};

TEST_P(AVCConversionTest, ParseCorrectly) {
  std::vector<uint8_t> buf;
  std::vector<SubsampleEntry> subsamples;
  MakeInputForLength(GetParam(), &buf);
  EXPECT_TRUE(AVC::ConvertFrameToAnnexB(GetParam(), &buf, &subsamples));

  BitstreamConverter::AnalysisResult expected;
  expected.is_conformant = true;
  expected.is_keyframe = false;
  EXPECT_PRED2(AnalysesMatch,
               AVC::AnalyzeAnnexB(buf.data(), buf.size(), subsamples),
               expected);

  EXPECT_EQ(buf.size(), sizeof(kExpected));
  EXPECT_EQ(0, memcmp(kExpected, &buf[0], sizeof(kExpected)));
  EXPECT_EQ("P,SDC", AnnexBToString(buf, subsamples));
}

// Intentionally write NALU sizes that are larger than the buffer.
TEST_P(AVCConversionTest, NALUSizeTooLarge) {
  std::vector<uint8_t> buf;
  WriteLength(GetParam(), 10 * sizeof(kNALU1), &buf);
  buf.insert(buf.end(), kNALU1, kNALU1 + sizeof(kNALU1));
  EXPECT_FALSE(AVC::ConvertFrameToAnnexB(GetParam(), &buf, nullptr));
}

TEST_P(AVCConversionTest, NALUSizeIsZero) {
  std::vector<uint8_t> buf;
  WriteLength(GetParam(), 0, &buf);

  WriteLength(GetParam(), sizeof(kNALU1), &buf);
  buf.insert(buf.end(), kNALU1, kNALU1 + sizeof(kNALU1));

  WriteLength(GetParam(), 0, &buf);

  WriteLength(GetParam(), sizeof(kNALU2), &buf);
  buf.insert(buf.end(), kNALU2, kNALU2 + sizeof(kNALU2));

  EXPECT_FALSE(AVC::ConvertFrameToAnnexB(GetParam(), &buf, nullptr));
}

TEST_P(AVCConversionTest, SubsampleSizesUpdatedAfterAnnexBConversion) {
  std::vector<uint8_t> buf;
  std::vector<SubsampleEntry> subsamples;
  SubsampleEntry subsample;

  // Write the first subsample, consisting of only one NALU
  WriteLength(GetParam(), sizeof(kNALU1), &buf);
  buf.insert(buf.end(), kNALU1, kNALU1 + sizeof(kNALU1));

  subsample.clear_bytes = GetParam() + sizeof(kNALU1);
  subsample.cypher_bytes = 0;
  subsamples.push_back(subsample);

  // Write the second subsample, containing two NALUs
  WriteLength(GetParam(), sizeof(kNALU1), &buf);
  buf.insert(buf.end(), kNALU1, kNALU1 + sizeof(kNALU1));
  WriteLength(GetParam(), sizeof(kNALU2), &buf);
  buf.insert(buf.end(), kNALU2, kNALU2 + sizeof(kNALU2));

  subsample.clear_bytes = 2*GetParam() + sizeof(kNALU1) + sizeof(kNALU2);
  subsample.cypher_bytes = 0;
  subsamples.push_back(subsample);

  // Write the third subsample, containing a single one-byte NALU
  WriteLength(GetParam(), 1, &buf);
  buf.push_back(0);
  subsample.clear_bytes = GetParam() + 1;
  subsample.cypher_bytes = 0;
  subsamples.push_back(subsample);

  EXPECT_TRUE(AVC::ConvertFrameToAnnexB(GetParam(), &buf, &subsamples));
  EXPECT_EQ(subsamples.size(), 3u);
  EXPECT_EQ(subsamples[0].clear_bytes, 4 + sizeof(kNALU1));
  EXPECT_EQ(subsamples[0].cypher_bytes, 0u);
  EXPECT_EQ(subsamples[1].clear_bytes, 8 + sizeof(kNALU1) + sizeof(kNALU2));
  EXPECT_EQ(subsamples[1].cypher_bytes, 0u);
  EXPECT_EQ(subsamples[2].clear_bytes, 4 + 1u);
  EXPECT_EQ(subsamples[2].cypher_bytes, 0u);
}

TEST_P(AVCConversionTest, ParsePartial) {
  std::vector<uint8_t> buf;
  MakeInputForLength(GetParam(), &buf);
  buf.pop_back();
  EXPECT_FALSE(AVC::ConvertFrameToAnnexB(GetParam(), &buf, nullptr));
  // This tests a buffer ending in the middle of a NAL length. For length size
  // of one, this can't happen, so we skip that case.
  if (GetParam() != 1) {
    MakeInputForLength(GetParam(), &buf);
    buf.erase(buf.end() - (sizeof(kNALU2) + 1), buf.end());
    EXPECT_FALSE(AVC::ConvertFrameToAnnexB(GetParam(), &buf, nullptr));
  }
}

TEST_P(AVCConversionTest, ParseEmpty) {
  std::vector<uint8_t> buf;
  EXPECT_TRUE(AVC::ConvertFrameToAnnexB(GetParam(), &buf, nullptr));
  EXPECT_EQ(0u, buf.size());
}

INSTANTIATE_TEST_SUITE_P(AVCConversionTestValues,
                         AVCConversionTest,
                         ::testing::Values(1, 2, 4));

TEST_F(AVCConversionTest, ConvertConfigToAnnexB) {
  AVCDecoderConfigurationRecord avc_config;
  avc_config.sps_list.resize(2);
  avc_config.sps_list[0].push_back(0x67);
  avc_config.sps_list[0].push_back(0x12);
  avc_config.sps_list[1].push_back(0x67);
  avc_config.sps_list[1].push_back(0x34);
  avc_config.pps_list.resize(1);
  avc_config.pps_list[0].push_back(0x68);
  avc_config.pps_list[0].push_back(0x56);
  avc_config.pps_list[0].push_back(0x78);

  std::vector<uint8_t> buf;
  std::vector<SubsampleEntry> subsamples;
  EXPECT_TRUE(AVC::ConvertConfigToAnnexB(avc_config, &buf));
  EXPECT_EQ(0, memcmp(kExpectedParamSets, &buf[0],
                      sizeof(kExpectedParamSets)));
  EXPECT_EQ("SPS,SPS,PPS", AnnexBToString(buf, subsamples));
}

// Verify that we can round trip string -> Annex B -> string.
TEST_F(AVCConversionTest, StringConversionFunctions) {
  std::string str =
      "AUD SPS SPSExt SPS PPS SEI SEI R14 I P FILL EOSeq EOStr";
  std::vector<uint8_t> buf;
  std::vector<SubsampleEntry> subsamples;
  AvcStringToAnnexB(str, &buf, &subsamples);

  BitstreamConverter::AnalysisResult expected;
  expected.is_conformant = true;
  expected.is_keyframe = true;
  EXPECT_PRED2(AnalysesMatch,
               AVC::AnalyzeAnnexB(buf.data(), buf.size(), subsamples),
               expected);

  EXPECT_EQ(str, AnnexBToString(buf, subsamples));
}

TEST_F(AVCConversionTest, ValidAnnexBConstructs) {
  struct {
    const char* case_string;
    const bool is_keyframe;
  } test_cases[] = {
      {"I", true},
      {"I I I I", true},
      {"AUD I", true},
      {"AUD SPS PPS I", true},
      {"I EOSeq", true},
      {"I EOSeq EOStr", true},
      {"I EOStr", true},
      {"P", false},
      {"P P P P", false},
      {"AUD SPS PPS P", false},
      {"SEI SEI I", true},
      {"SEI SEI R14 I", true},
      {"SPS SPSExt SPS PPS I P", true},
      {"R14 SEI I", true},
      {"AUD,I", true},
      {"AUD,SEI I", true},
      {"AUD,SEI,SPS,PPS,I", true},

      // In reality, these might not always be conformant/valid, but assuming
      // they are, they're not keyframes because a non-IDR slice preceded the
      // IDR slice, if any.
      {"SDA SDB SDC", false},
      {"P I", false},
      {"SDA I", false},
      {"SDB I", false},
      {"SDC I", false},
  };

  for (size_t i = 0; i < base::size(test_cases); ++i) {
    std::vector<uint8_t> buf;
    std::vector<SubsampleEntry> subsamples;
    AvcStringToAnnexB(test_cases[i].case_string, &buf, NULL);

    BitstreamConverter::AnalysisResult expected;
    expected.is_conformant = true;
    expected.is_keyframe = test_cases[i].is_keyframe;
    EXPECT_PRED2(AnalysesMatch,
                 AVC::AnalyzeAnnexB(buf.data(), buf.size(), subsamples),
                 expected)
        << "'" << test_cases[i].case_string << "' failed";
  }
}

TEST_F(AVCConversionTest, InvalidAnnexBConstructs) {
  struct {
    const char* case_string;
    const base::Optional<bool> is_keyframe;
  } test_cases[] = {
      // For these cases, lack of conformance is determined before detecting any
      // IDR or non-IDR slices, so the non-conformant frames' keyframe analysis
      // reports base::nullopt (which means undetermined analysis result).
      {"AUD", base::nullopt},            // No VCL present.
      {"AUD,SEI", base::nullopt},        // No VCL present.
      {"SPS PPS", base::nullopt},        // No VCL present.
      {"SPS PPS AUD I", base::nullopt},  // Parameter sets must come after AUD.
      {"SPSExt SPS P", base::nullopt},   // SPS must come before SPSExt.
      {"SPS PPS SPSExt P", base::nullopt},  // SPSExt must follow an SPS.
      {"EOSeq", base::nullopt},             // EOSeq must come after a VCL.
      {"EOStr", base::nullopt},             // EOStr must come after a VCL.

      // For these cases, IDR slice is first VCL and is detected before
      // conformance failure, so the non-conformant frame is reported as a
      // keyframe.
      {"I EOStr EOSeq", true},  // EOSeq must come before EOStr.
      {"I R14", true},          // Reserved14-18 must come before first VCL.
      {"I SEI", true},          // SEI must come before first VCL.

      // For this case, P slice is first VCL and is detected before conformance
      // failure, so the non-conformant frame is reported as a non-keyframe.
      {"P SPS P",
       false},  // SPS after first VCL would indicate a new access unit.
  };

  BitstreamConverter::AnalysisResult expected;
  expected.is_conformant = false;

  for (size_t i = 0; i < base::size(test_cases); ++i) {
    std::vector<uint8_t> buf;
    std::vector<SubsampleEntry> subsamples;
    AvcStringToAnnexB(test_cases[i].case_string, &buf, NULL);
    expected.is_keyframe = test_cases[i].is_keyframe;
    EXPECT_PRED2(AnalysesMatch,
                 AVC::AnalyzeAnnexB(buf.data(), buf.size(), subsamples),
                 expected)
        << "'" << test_cases[i].case_string << "' failed";
  }
}

typedef struct {
  const char* input;
  const char* expected;
} InsertTestCases;

TEST_F(AVCConversionTest, InsertParamSetsAnnexB) {
  static const InsertTestCases test_cases[] = {
    { "I", "SPS,SPS,PPS,I" },
    { "AUD I", "AUD SPS,SPS,PPS,I" },

    // Cases where param sets in |avc_config| are placed before
    // the existing ones.
    { "SPS,PPS,I", "SPS,SPS,PPS,SPS,PPS,I" },
    { "AUD,SPS,PPS,I", "AUD,SPS,SPS,PPS,SPS,PPS,I" },  // Note: params placed
                                                       // after AUD.

    // One or more NALUs might follow AUD in the first subsample, we need to
    // handle this correctly. Params should be inserted right after AUD.
    { "AUD,SEI I", "AUD,SPS,SPS,PPS,SEI I" },
  };

  AVCDecoderConfigurationRecord avc_config;
  avc_config.sps_list.resize(2);
  avc_config.sps_list[0].push_back(0x67);
  avc_config.sps_list[0].push_back(0x12);
  avc_config.sps_list[1].push_back(0x67);
  avc_config.sps_list[1].push_back(0x34);
  avc_config.pps_list.resize(1);
  avc_config.pps_list[0].push_back(0x68);
  avc_config.pps_list[0].push_back(0x56);
  avc_config.pps_list[0].push_back(0x78);

  BitstreamConverter::AnalysisResult expected;
  expected.is_conformant = true;
  expected.is_keyframe = true;

  for (size_t i = 0; i < base::size(test_cases); ++i) {
    std::vector<uint8_t> buf;
    std::vector<SubsampleEntry> subsamples;

    AvcStringToAnnexB(test_cases[i].input, &buf, &subsamples);

    EXPECT_TRUE(AVC::InsertParamSetsAnnexB(avc_config, &buf, &subsamples))
        << "'" << test_cases[i].input << "' insert failed.";
    EXPECT_PRED2(AnalysesMatch,
                 AVC::AnalyzeAnnexB(buf.data(), buf.size(), subsamples),
                 expected)
        << "'" << test_cases[i].input << "' created invalid AnnexB.";
    EXPECT_EQ(test_cases[i].expected, AnnexBToString(buf, subsamples))
        << "'" << test_cases[i].input << "' generated unexpected output.";
  }
}

}  // namespace mp4
}  // namespace media
