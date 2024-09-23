// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/formats/mp4/hevc.h"

#include "media/formats/mp4/nalu_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace mp4 {

TEST(HEVCAnalyzeAnnexBTest, ValidAnnexBConstructs) {
  struct {
    const char* case_string;
    const bool is_keyframe;
  } test_cases[] = {
      {"I", true},          {"I I I I", true}, {"AUD I", true},
      {"AUD SPS I", true},  {"I EOS", true},   {"I EOS EOB", true},
      {"I EOB", true},      {"P", false},      {"P P P P", false},
      {"AUD SPS P", false}, {"AUD,I", true},   {"AUD,SPS,I", true},
  };

  for (size_t i = 0; i < std::size(test_cases); ++i) {
    std::vector<uint8_t> buf;
    std::vector<SubsampleEntry> subsamples;
    HevcStringToAnnexB(test_cases[i].case_string, &buf, nullptr);

    BitstreamConverter::AnalysisResult expected;
    expected.is_conformant = true;
    expected.is_keyframe = test_cases[i].is_keyframe;
    EXPECT_PRED2(AnalysesMatch,
                 HEVC::AnalyzeAnnexB(buf.data(), buf.size(), subsamples),
                 expected)
        << "'" << test_cases[i].case_string << "' failed";
  }
}

TEST(HEVCAnalyzeAnnexBTest, InvalidAnnexBConstructs) {
  struct {
    const char* case_string;
    const std::optional<bool> is_keyframe;
  } test_cases[] = {
      // For these cases, lack of conformance is determined before detecting any
      // IDR or non-IDR slices, so the non-conformant frames' keyframe analysis
      // reports std::nullopt (which means undetermined analysis result).
      {"AUD", std::nullopt},        // No VCL present.
      {"AUD,SPS", std::nullopt},    // No VCL present.
      {"SPS AUD I", std::nullopt},  // Parameter sets must come after AUD.
      {"EOS", std::nullopt},        // EOS must come after a VCL.
      {"EOB", std::nullopt},        // EOB must come after a VCL.

      // For these cases, IDR slice is first VCL and is detected before
      // conformance failure, so the non-conformant frame is reported as a
      // keyframe.
      {"I EOB EOS", true},  // EOS must come before EOB.
      {"I SPS", true},      // SPS must come before first VCL.

      // For this case, P slice is first VCL and is detected before conformance
      // failure, so the non-conformant frame is reported as a non-keyframe.
      {"P SPS P",
       false},  // SPS after first VCL would indicate a new access unit.
  };

  BitstreamConverter::AnalysisResult expected;
  expected.is_conformant = false;

  for (size_t i = 0; i < std::size(test_cases); ++i) {
    std::vector<uint8_t> buf;
    std::vector<SubsampleEntry> subsamples;
    HevcStringToAnnexB(test_cases[i].case_string, &buf, nullptr);
    expected.is_keyframe = test_cases[i].is_keyframe;
    EXPECT_PRED2(AnalysesMatch,
                 HEVC::AnalyzeAnnexB(buf.data(), buf.size(), subsamples),
                 expected)
        << "'" << test_cases[i].case_string << "' failed";
  }
}

TEST(HEVCAnalyzeAnnexBTest, HEVCDecoderConfigurationRecordTakenFromStream) {
  std::vector<uint8_t> test_data{
      0x01, 0x01, 0x60, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x3c, 0xf0, 0x00, 0xfc, 0xfd, 0xf8, 0xf8, 0x00, 0x00, 0x0f, 0x03, 0x20,
      0x00, 0x01, 0x00, 0x18, 0x40, 0x01, 0x0c, 0x01, 0xff, 0xff, 0x01, 0x60,
      0x00, 0x00, 0x03, 0x00, 0x80, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00,
      0x3c, 0x95, 0xc0, 0x90, 0x21, 0x00, 0x01, 0x00, 0x27, 0x42, 0x01, 0x01,
      0x01, 0x60, 0x00, 0x00, 0x03, 0x00, 0x80, 0x00, 0x00, 0x03, 0x00, 0x00,
      0x03, 0x00, 0x3c, 0xa0, 0x0a, 0x08, 0x0b, 0x9f, 0x79, 0x65, 0x79, 0x24,
      0xca, 0xe0, 0x10, 0x00, 0x00, 0x06, 0x40, 0x00, 0x00, 0xbb, 0x50, 0x80,
      0x22, 0x00, 0x01, 0x00, 0x06, 0x44, 0x01, 0xc1, 0x73, 0xd1, 0x89};
  HEVCDecoderConfigurationRecord record;
  EXPECT_TRUE(record.Parse(test_data.data(), test_data.size()));
  std::vector<uint8_t> output;
  EXPECT_TRUE(record.Serialize(output));
  EXPECT_TRUE(test_data == output);
}

}  // namespace mp4
}  // namespace media
