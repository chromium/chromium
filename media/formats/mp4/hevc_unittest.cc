// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/mp4/hevc.h"
#include "base/stl_util.h"
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

  for (size_t i = 0; i < base::size(test_cases); ++i) {
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
    const base::Optional<bool> is_keyframe;
  } test_cases[] = {
      // For these cases, lack of conformance is determined before detecting any
      // IDR or non-IDR slices, so the non-conformant frames' keyframe analysis
      // reports base::nullopt (which means undetermined analysis result).
      {"AUD", base::nullopt},        // No VCL present.
      {"AUD,SPS", base::nullopt},    // No VCL present.
      {"SPS AUD I", base::nullopt},  // Parameter sets must come after AUD.
      {"EOS", base::nullopt},        // EOS must come after a VCL.
      {"EOB", base::nullopt},        // EOB must come after a VCL.

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

  for (size_t i = 0; i < base::size(test_cases); ++i) {
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

}  // namespace mp4
}  // namespace media
