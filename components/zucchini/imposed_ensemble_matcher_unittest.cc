// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/zucchini/imposed_ensemble_matcher.h"

#include <stddef.h>
#include <stdint.h>

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "components/zucchini/buffer_view.h"
#include "components/zucchini/disassembler.h"
#include "components/zucchini/element_detection.h"
#include "components/zucchini/image_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace zucchini {

namespace {

// This test uses a mock archive format where regions are determined by their
// consecutive byte values rather than parsing real executables. In fact, since
// elements are imposed, only the first byte of the element is used to specify
// executable type of the mock data:
// - 'W' and 'w' specify kExeTypeWin32X86.
// - 'E' and 'e' specify kExeTypeElfX86.
// - Everything else specify kExeTypeUnknown.
class TestElementDetector {
 public:
  TestElementDetector() = default;

  std::optional<Element> Run(ConstBufferView image) const {
    DCHECK_GT(image.size(), 0U);
    char first_char = *image.begin();
    if (first_char == 'W' || first_char == 'w')
      return Element(image.local_region(), kExeTypeWin32X86);
    if (first_char == 'E' || first_char == 'e')
      return Element(image.local_region(), kExeTypeElfX86);
    return std::nullopt;
  }
};

}  // namespace

TEST(ImposedMatchParserTest, ImposedMatchParser) {
  std::vector<uint8_t> old_data;
  std::vector<uint8_t> new_data;
  auto populate = [](const std::string& s, std::vector<uint8_t>* data) {
    for (char ch : s)
      data->push_back(static_cast<uint8_t>(ch));
  };
  // Pos:             11111111
  //        012345678901234567
  populate("1WW222EEEE", &old_data);
  populate("33eee2222222wwww44", &new_data);

  ConstBufferView old_image(&old_data[0], old_data.size());
  ConstBufferView new_image(&new_data[0], new_data.size());

  TestElementDetector detector;

  // Reusable output values.
  std::string prev_imposed_matches;
  ImposedMatchParser::Status status;
  size_t num_identical;
  std::vector<ElementMatch> matches;
  std::vector<ElementMatch> bad_matches;

  auto run_test = [&](const std::string& imposed_matches) -> bool {
    prev_imposed_matches = imposed_matches;
    status = ImposedMatchParser::kSuccess;
    num_identical = 0;
    matches.clear();
    bad_matches.clear();
    ImposedMatchParser parser;
    status = parser.Parse(imposed_matches, old_image, new_image,
                          base::BindRepeating(&TestElementDetector::Run,
                                              base::Unretained(&detector)));
    num_identical = parser.num_identical();
    matches = std::move(*parser.mutable_matches());
    bad_matches = std::move(*parser.mutable_bad_matches());
    return status == ImposedMatchParser::kSuccess;
  };

  auto run_check = [&](const ElementMatch& match, ExecutableType exe_type,
                       offset_t old_offset, size_t old_size,
                       offset_t new_offset, size_t new_size) {
    EXPECT_EQ(exe_type, match.exe_type()) << prev_imposed_matches;
    EXPECT_EQ(exe_type, match.old_element.exe_type) << prev_imposed_matches;
    EXPECT_EQ(old_offset, match.old_element.offset) << prev_imposed_matches;
    EXPECT_EQ(old_size, match.old_element.size) << prev_imposed_matches;
    EXPECT_EQ(exe_type, match.new_element.exe_type) << prev_imposed_matches;
    EXPECT_EQ(new_offset, match.new_element.offset) << prev_imposed_matches;
    EXPECT_EQ(new_size, match.new_element.size) << prev_imposed_matches;
  };

  // Empty string: Vacuous but valid.
  EXPECT_TRUE(run_test(""));
  EXPECT_EQ(0U, num_identical);
  EXPECT_EQ(0U, matches.size());
  EXPECT_EQ(0U, bad_matches.size());

  // Full matches. Different permutations give same result.
  for (const std::string& imposed_matches :
       {"1+2=12+4,4+2=5+2,6+4=2+3", "1+2=12+4,6+4=2+3,4+2=5+2",
        "4+2=5+2,1+2=12+4,6+4=2+3", "4+2=5+2,6+4=2+3,1+2=12+4",
        "6+4=2+3,1+2=12+4,4+2=5+2", "6+4=2+3,1+2=12+4,4+2=5+2"}) {
    EXPECT_TRUE(run_test(imposed_matches));
    EXPECT_EQ(1U, num_identical);  // "4+2=5+2"
    EXPECT_EQ(2U, matches.size());
    // Results are sorted by "new" offsets.
    run_check(matches[0], kExeTypeElfX86, 6, 4, 2, 3);
    run_check(matches[1], kExeTypeWin32X86, 1, 2, 12, 4);
    EXPECT_EQ(0U, bad_matches.size());
  }

  // Single subregion match.
  EXPECT_TRUE(run_test("1+2=12+4"));
  EXPECT_EQ(0U, num_identical);
  EXPECT_EQ(1U, matches.size());
  run_check(matches[0], kExeTypeWin32X86, 1, 2, 12, 4);
  EXPECT_EQ(0U, bad_matches.size());

  // Single subregion match. We're lax with redundant 0.
  EXPECT_TRUE(run_test("6+04=02+10"));
  EXPECT_EQ(0U, num_identical);
  EXPECT_EQ(1U, matches.size());
  run_check(matches[0], kExeTypeElfX86, 6, 4, 2, 10);
  EXPECT_EQ(0U, bad_matches.size());

  // Successive elements, no overlap.
  EXPECT_TRUE(run_test("1+1=12+1,2+1=13+1"));
  EXPECT_EQ(0U, num_identical);
  EXPECT_EQ(2U, matches.size());
  run_check(matches[0], kExeTypeWin32X86, 1, 1, 12, 1);
  run_check(matches[1], kExeTypeWin32X86, 2, 1, 13, 1);
  EXPECT_EQ(0U, bad_matches.size());

  // Overlap in "old" file is okay.
  EXPECT_TRUE(run_test("1+2=12+2,1+2=14+2"));
  EXPECT_EQ(0U, num_identical);
  EXPECT_EQ(2U, matches.size());
  run_check(matches[0], kExeTypeWin32X86, 1, 2, 12, 2);
  run_check(matches[1], kExeTypeWin32X86, 1, 2, 14, 2);
  EXPECT_EQ(0U, bad_matches.size());

  // Entire files: Have unknown type, so are recognized as such, and ignored.
  EXPECT_TRUE(run_test("0+10=0+18"));
  EXPECT_EQ(0U, num_identical);
  EXPECT_EQ(0U, matches.size());
  EXPECT_EQ(1U, bad_matches.size());
  run_check(bad_matches[0], kExeTypeUnknown, 0, 10, 0, 18);

  // Forgive matches that mix known type with unknown type.
  EXPECT_TRUE(run_test("1+2=0+18"));
  EXPECT_EQ(0U, num_identical);
  EXPECT_EQ(0U, matches.size());
  EXPECT_EQ(1U, bad_matches.size());
  run_check(bad_matches[0], kExeTypeUnknown, 1, 2, 0, 18);

  EXPECT_TRUE(run_test("0+10=12+4"));
  EXPECT_EQ(0U, num_identical);
  EXPECT_EQ(0U, matches.size());
  EXPECT_EQ(1U, bad_matches.size());
  run_check(bad_matches[0], kExeTypeUnknown, 0, 10, 12, 4);

  // Test invalid delimiter.
  for (const std::string& imposed_matches :
       {"1+2=12+4,4+2=5+2x", "1+2=12+4 4+2=5+2", "1+2=12+4,4+2=5+2 ",
        "1+2=12+4 "}) {
    EXPECT_FALSE(run_test(imposed_matches));
    EXPECT_EQ(ImposedMatchParser::kInvalidDelimiter, status);
  }

  // Test parse errors, including uint32_t overflow.
  for (const std::string& imposed_matches :
       {"x1+2=12+4,4+2=5+2,6+4=2+3", "x1+2=12+4,4+2=5+2,6+4=2+3x", ",", " ",
        "+2=12+4", "1+2+12+4", "1=2+12+4", " 1+2=12+4", "1+2= 12+4", "1", "1+2",
        "1+2=", "1+2=12", "1+2=12+", "4294967296+2=12+4"}) {
    EXPECT_FALSE(run_test(imposed_matches));
    EXPECT_EQ(ImposedMatchParser::kParseError, status);
  }

  // Test bound errors, include 0-size.
  for (const std::string& imposed_matches :
       {"1+10=12+4", "1+2=12+7", "0+11=0+18", "0+12=0+17", "10+1=0+18",
        "0+10=18+1", "0+0=0+18", "0+10=0+0", "1000000000+1=0+1000000000"}) {
    EXPECT_FALSE(run_test(imposed_matches));
    EXPECT_EQ(ImposedMatchParser::kOutOfBound, status);
  }

  // Test overlap errors. Matches that get ignored are still tested.
  for (const std::string& imposed_matches :
       {"1+2=12+4,4+2=5+2,6+4=2+4", "0+10=0+18,1+2=12+4", "6+4=2+10,3+2=5+2"}) {
    EXPECT_FALSE(run_test(imposed_matches));
    EXPECT_EQ(ImposedMatchParser::kOverlapInNew, status);
  }

  // Test type mismatch errors.
  EXPECT_FALSE(run_test("1+2=2+3"));
  EXPECT_EQ(ImposedMatchParser::kTypeMismatch, status);

  EXPECT_FALSE(run_test("6+4=12+4"));
  EXPECT_EQ(ImposedMatchParser::kTypeMismatch, status);
}

}  // namespace zucchini
