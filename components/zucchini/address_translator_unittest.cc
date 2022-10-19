// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/zucchini/address_translator.h"

#include <string>
#include <utility>

#include "base/format_macros.h"
#include "base/ranges/algorithm.h"
#include "base/strings/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace zucchini {

namespace {

// Test case structs. The convention of EXPECT() specifies "expectd" value
// before ""actual". However, AddressTranslator interfaces explicitly state "X
// to Y". So it is clearer in test cases to specify "input" before "expect".
struct OffsetToRvaTestCase {
  offset_t input;
  rva_t expect;
};

struct RvaToOffsetTestCase {
  rva_t input;
  offset_t expect;
};

class TestAddressTranslator : public AddressTranslator {
 public:
  using AddressTranslator::AddressTranslator;

  // Initialize() alternative that parses a visual representation of offset and
  // RVA ranges. Illustrative example ("special" means '.' or '!'):
  // "..AAA...|....aaaa" => "..AAA..." for offsets, and "....aaaa" for RVAs:
  // - "..AAA...": First non-period character is at 2, so |offset_begin| = 2.
  // - "..AAA...": There are 3 non-special characters, so |offset_size| = +3.
  // - "....aaaa": First non-period character is at 4, so |rva_begin| = 4.
  // - "....aaaa": There are 4 non-special characters, so |rva_size| = +4.
  // For the special case of length-0 range, '!' can be used. For example,
  // "...!...." specifies |begin| = 3 and |size| = +0.
  AddressTranslator::Status InitializeWithStrings(
      const std::vector<std::string>& specs) {
    std::vector<Unit> units;
    units.reserve(specs.size());
    for (const std::string& s : specs) {
      size_t sep = s.find('|');
      CHECK_NE(sep, std::string::npos);
      std::string s1 = s.substr(0, sep);
      std::string s2 = s.substr(sep + 1);

      auto first_non_blank = [](const std::string& t) {
        auto is_blank = [](char ch) { return ch == '.'; };
        return base::ranges::find_if_not(t, is_blank) - t.begin();
      };
      auto count_non_special = [](const std::string& t) {
        auto is_special = [](char ch) { return ch == '.' || ch == '!'; };
        return t.size() - base::ranges::count_if(t, is_special);
      };
      units.push_back({static_cast<offset_t>(first_non_blank(s1)),
                       static_cast<offset_t>(count_non_special(s1)),
                       static_cast<rva_t>(first_non_blank(s2)),
                       static_cast<rva_t>(count_non_special(s2))});
    }
    return Initialize(std::move(units));
  }
};

// Simple test: Initialize TestAddressTranslator using |specs|, and match
// |expected| results re. success or failure.
void SimpleTest(const std::vector<std::string>& specs,
                AddressTranslator::Status expected,
                const std::string& case_name) {
  TestAddressTranslator translator;
  auto result = translator.InitializeWithStrings(specs);
  EXPECT_EQ(expected, result) << case_name;
}

// Test AddressTranslator::Initialize's Unit overlap and error checks over
// multiple test cases, each case consists of a fixed unit (specified as
// string), and a variable string taken from an list.
class TwoUnitOverlapTester {
 public:
  struct TestCase {
    std::string unit_str;
    AddressTranslator::Status expected;
  };

  static void RunTest(const std::string& unit_str1,
                      const std::vector<TestCase>& test_cases) {
    for (size_t i = 0; i < test_cases.size(); ++i) {
      const auto& test_case = test_cases[i];
      const std::string& unit_str2 = test_case.unit_str;
      const std::string str =
          base::StringPrintf("Case #%" PRIuS ": %s", i, unit_str2.c_str());
      SimpleTest({unit_str1, unit_str2}, test_case.expected, str);
      // Switch order. Expect same results.
      SimpleTest({unit_str2, unit_str1}, test_case.expected, str);
    }
  }
};

}  // namespace

TEST(AddressTranslatorTest, Empty) {
  using AT = AddressTranslator;
  TestAddressTranslator translator;
  EXPECT_EQ(AT::kSuccess,
            translator.Initialize(std::vector<AddressTranslator::Unit>()));
  offset_t fake_offset_begin = translator.fake_offset_begin();

  // Optimized versions.
  AddressTranslator::OffsetToRvaCache offset_to_rva(translator);
  AddressTranslator::RvaToOffsetCache rva_to_offset(translator);

  EXPECT_EQ(kInvalidRva, translator.OffsetToRva(0U));
  EXPECT_EQ(kInvalidRva, translator.OffsetToRva(100U));
  EXPECT_EQ(kInvalidRva, offset_to_rva.Convert(0U));
  EXPECT_EQ(kInvalidRva, offset_to_rva.Convert(100U));

  EXPECT_EQ(kInvalidOffset, translator.RvaToOffset(0U));
  EXPECT_EQ(kInvalidOffset, translator.RvaToOffset(100U));
  EXPECT_EQ(kInvalidOffset, rva_to_offset.Convert(0U));
  EXPECT_EQ(kInvalidOffset, rva_to_offset.Convert(100U));

  EXPECT_EQ(kInvalidRva, translator.OffsetToRva(fake_offset_begin));
  EXPECT_EQ(kInvalidRva, offset_to_rva.Convert(fake_offset_begin));
}

TEST(AddressTranslatorTest, Single) {
  using AT = AddressTranslator;
  TestAddressTranslator translator;
  // Offsets to RVA: [10, 30) -> [100, 120).
  EXPECT_EQ(AT::kSuccess, translator.Initialize({{10U, +20U, 100U, +20U}}));
  offset_t fake_offset_begin = translator.fake_offset_begin();

  // Optimized versions.
  AddressTranslator::OffsetToRvaCache offset_to_rva(translator);
  AddressTranslator::RvaToOffsetCache rva_to_offset(translator);
  EXPECT_EQ(30U, fake_offset_begin);  // Test implementation detail.

  // Offsets to RVAs.
  OffsetToRvaTestCase test_cases1[] = {
      {0U, kInvalidRva}, {9U, kInvalidRva}, {10U, 100U},
      {20U, 110U},       {29U, 119U},       {30U, kInvalidRva},
  };
  for (auto& test_case : test_cases1) {
    EXPECT_EQ(test_case.expect, translator.OffsetToRva(test_case.input));
    EXPECT_EQ(test_case.expect, offset_to_rva.Convert(test_case.input));
  }

  // RVAs to offsets.
  RvaToOffsetTestCase test_cases2[] = {
      {0U, kInvalidOffset}, {99U, kInvalidOffset}, {100U, 10U},
      {110U, 20U},          {119U, 29U},           {120U, kInvalidOffset},
  };
  for (auto& test_case : test_cases2) {
    EXPECT_EQ(test_case.expect, translator.RvaToOffset(test_case.input));
    EXPECT_EQ(test_case.expect, rva_to_offset.Convert(test_case.input));
  }
}

TEST(AddressTranslatorTest, SingleDanglingRva) {
  using AT = AddressTranslator;
  TestAddressTranslator translator;
  // Offsets to RVA: [10, 30) -> [100, 120 + 7), so has dangling RVAs.
  EXPECT_EQ(AT::kSuccess,
            translator.Initialize({{10U, +20U, 100U, +20U + 7U}}));
  offset_t fake_offset_begin = translator.fake_offset_begin();

  EXPECT_EQ(30U, fake_offset_begin);  // Test implementation detail.

  // Optimized versions.
  AddressTranslator::OffsetToRvaCache offset_to_rva(translator);
  AddressTranslator::RvaToOffsetCache rva_to_offset(translator);

  // Offsets to RVAs.
  OffsetToRvaTestCase test_cases1[] = {
      {0U, kInvalidRva},
      {9U, kInvalidRva},
      {10U, 100U},
      {20U, 110U},
      {29U, 119U},
      {30U, kInvalidRva},
      // Fake offsets to dangling RVAs.
      {fake_offset_begin + 100U, kInvalidRva},
      {fake_offset_begin + 119U, kInvalidRva},
      {fake_offset_begin + 120U, 120U},
      {fake_offset_begin + 126U, 126U},
      {fake_offset_begin + 127U, kInvalidRva},
  };
  for (auto& test_case : test_cases1) {
    EXPECT_EQ(test_case.expect, translator.OffsetToRva(test_case.input));
    EXPECT_EQ(test_case.expect, offset_to_rva.Convert(test_case.input));
  }

  // RVAs to offsets.
  RvaToOffsetTestCase test_cases2[] = {
      {0U, kInvalidOffset},
      {99U, kInvalidOffset},
      {100U, 10U},
      {110U, 20U},
      {119U, 29U},
      // Dangling RVAs to fake offsets.
      {120U, fake_offset_begin + 120U},
      {126U, fake_offset_begin + 126U},
      {127U, kInvalidOffset},
  };
  for (auto& test_case : test_cases2) {
    EXPECT_EQ(test_case.expect, translator.RvaToOffset(test_case.input));
    EXPECT_EQ(test_case.expect, rva_to_offset.Convert(test_case.input));
  }
}

TEST(AddressTranslatorTest, BasicUsage) {
  using AT = AddressTranslator;
  TestAddressTranslator translator;
  // Offsets covered: [10, 30), [40, 70), [70, 110).
  // Map to RVAs: [200, 220 + 5), [300, 330), [100, 140), so has dangling RVAs.
  auto result = translator.Initialize({
      {10U, +20U, 200U, +20U + 5U},  // Has dangling RVAs.
      {40U, +30U, 300U, +20U},       // Extra offset truncated and ignored.
      {50U, +20U, 310U, +20U},       // Overlap with previous: Merged.
      {70U, +40U, 100U, +20U},  // Tangent with previous but inconsistent; extra
                                // offset truncated and ignored.
      {90U, +20U, 120U, +20U},  // Tangent with previous and consistent: Merged.
  });
  EXPECT_EQ(AT::kSuccess, result);
  offset_t fake_offset_begin = translator.fake_offset_begin();
  EXPECT_EQ(110U, fake_offset_begin);  // Test implementation detail.

  // Optimized versions.
  AddressTranslator::OffsetToRvaCache offset_to_rva(translator);
  AddressTranslator::RvaToOffsetCache rva_to_offset(translator);

  // Offsets to RVAs.
  OffsetToRvaTestCase test_cases1[] = {
      {0U, kInvalidRva},
      {9U, kInvalidRva},
      {10U, 200U},
      {20U, 210U},
      {29U, 219U},
      {30U, kInvalidRva},
      {39U, kInvalidRva},
      {40U, 300U},
      {55U, 315U},
      {69U, 329U},
      {70U, 100U},
      {90U, 120U},
      {109U, 139U},
      {110U, kInvalidRva},
      // Fake offsets to dangling RVAs.
      {fake_offset_begin + 220U, 220U},
      {fake_offset_begin + 224U, 224U},
      {fake_offset_begin + 225U, kInvalidRva},
  };
  for (auto& test_case : test_cases1) {
    EXPECT_EQ(test_case.expect, translator.OffsetToRva(test_case.input));
    EXPECT_EQ(test_case.expect, offset_to_rva.Convert(test_case.input));
  }

  // RVAs to offsets.
  RvaToOffsetTestCase test_cases2[] = {
      {0U, kInvalidOffset},
      {99U, kInvalidOffset},
      {100U, 70U},
      {120U, 90U},
      {139U, 109U},
      {140U, kInvalidOffset},
      {199U, kInvalidOffset},
      {200U, 10U},
      {210U, 20U},
      {219U, 29U},
      {225U, kInvalidOffset},
      {299U, kInvalidOffset},
      {300U, 40U},
      {315U, 55U},
      {329U, 69U},
      {330U, kInvalidOffset},
      // Dangling RVAs to fake offsets.
      {220U, fake_offset_begin + 220U},
      {224U, fake_offset_begin + 224U},
      {225U, kInvalidOffset},
  };
  for (auto& test_case : test_cases2) {
    EXPECT_EQ(test_case.expect, translator.RvaToOffset(test_case.input));
    EXPECT_EQ(test_case.expect, rva_to_offset.Convert(test_case.input));
  }
}

TEST(AddressTranslatorTest, Overflow) {
  using AT = AddressTranslator;
  // Test assumes that offset_t and rva_t to be 32-bit.
  static_assert(sizeof(offset_t) == 4 && sizeof(rva_t) == 4,
                "Needs to update test.");
  {
    AddressTranslator translator1;
    EXPECT_EQ(AT::kErrorOverflow,
              translator1.Initialize({{0, +0xC0000000U, 0, +0xC0000000U}}));
  }
  {
    AddressTranslator translator2;
    EXPECT_EQ(AT::kErrorOverflow,
              translator2.Initialize({{0, +0, 0, +0xC0000000U}}));
  }
  {
    // Units are okay, owing to but limitations of the heuristic to convert
    // dangling RVA to fake offset, AddressTranslator::Initialize() fails.
    AddressTranslator translator3;
    EXPECT_EQ(AT::kErrorFakeOffsetBeginTooLarge,
              translator3.Initialize(
                  {{32, +0, 32, +0x50000000U}, {0x50000000U, +16, 0, +16}}));
  }
}

// Sanity test for TestAddressTranslator::InitializeWithStrings();
TEST(AddressTranslatorTest, AddUnitAsString) {
  using AT = AddressTranslator;
  {
    TestAddressTranslator translator1;
    EXPECT_EQ(AT::kSuccess, translator1.InitializeWithStrings({"..A..|.aaa."}));
    AddressTranslator::Unit unit1 = translator1.units_sorted_by_offset()[0];
    EXPECT_EQ(2U, unit1.offset_begin);
    EXPECT_EQ(+1U, unit1.offset_size);
    EXPECT_EQ(1U, unit1.rva_begin);
    EXPECT_EQ(+3U, unit1.rva_size);
  }
  {
    TestAddressTranslator translator2;
    EXPECT_EQ(AT::kSuccess,
              translator2.InitializeWithStrings({".....!...|.bbbbbb..."}));
    AddressTranslator::Unit unit2 = translator2.units_sorted_by_offset()[0];
    EXPECT_EQ(5U, unit2.offset_begin);
    EXPECT_EQ(+0U, unit2.offset_size);
    EXPECT_EQ(1U, unit2.rva_begin);
    EXPECT_EQ(+6U, unit2.rva_size);
  }
}

// AddressTranslator::Initialize() lists Unit merging examples in comments. The
// format is different from that used by InitializeWithStrings(), but adapting
// them is easy, so we may as well do so.
TEST(AddressTranslatorTest, OverlapFromComment) {
  using AT = AddressTranslator;
  constexpr auto OK = AT::kSuccess;
  struct {
    const char* rva_str;  // RVA comes first in this case.
    const char* offset_str;
    AT::Status expected;
  } test_cases[] = {
      {"..ssssffff..", "..SSSSFFFF..", OK},
      {"..ssssffff..", "..SSSS..FFFF..", OK},
      {"..ssssffff..", "..FFFF..SSSS..", OK},
      {"..ssssffff..", "..SSOOFF..", AT::kErrorBadOverlap},
      {"..sssooofff..", "..SSSOOOFFF..", OK},
      {"..sssooofff..", "..SSSSSOFFFFF..", AT::kErrorBadOverlap},
      {"..sssooofff..", "..FFOOOOSS..", AT::kErrorBadOverlap},
      {"..sssooofff..", "..SSSOOOF..", OK},
      {"..sssooofff..", "..SSSOOOF..", OK},
      {"..sssooosss..", "..SSSOOOS..", OK},
      {"..sssooofff..", "..SSSOO..", OK},
      {"..sssooofff..", "..SSSOFFF..", AT::kErrorBadOverlapDanglingRva},
      {"..sssooosss..", "..SSSOOSSSS..", AT::kErrorBadOverlapDanglingRva},
      {"..oooooo..", "..OOO..", OK},
  };

  auto to_period = [](std::string s, char ch) {  // |s| passed by value.
    std::replace(s.begin(), s.end(), ch, '.');
    return s;
  };

  size_t idx = 0;
  for (const auto& test_case : test_cases) {
    std::string base_str =
        std::string(test_case.offset_str) + "|" + test_case.rva_str;
    std::string unit_str1 = to_period(to_period(base_str, 'S'), 's');
    std::string unit_str2 = to_period(to_period(base_str, 'F'), 'f');
    SimpleTest({unit_str1, unit_str2}, test_case.expected,
               base::StringPrintf("Case #%" PRIuS, idx));
    ++idx;
  }
}

TEST(AddressTranslatorTest, Overlap) {
  using AT = AddressTranslator;
  constexpr auto OK = AT::kSuccess;
  constexpr const char* unit_str1 = "....AAA.......|.....aaa......";

  std::vector<TwoUnitOverlapTester::TestCase> test_cases = {
      //....AAA.......|.....aaa......   The first Unit. NOLINT
      {"....BBB.......|.....bbb......", OK},
      {"..BBB.........|...bbb........", OK},
      {"......BBB.....|.......bbb....", OK},
      {"..BBBBBBBBB...|...bbb........", OK},  // Extra offset get truncated.
      {"......BBBBBBBB|.......bbb....", OK},
      {"....BBB.......|.......bbb....", AT::kErrorBadOverlap},
      {"..BBB.........|.......bbb....", AT::kErrorBadOverlap},
      {".......BBB....|.......bbb....", AT::kErrorBadOverlap},
      //....AAA.......|.....aaa......   The first Unit. NOLINT
      {"....BBB.......|..........bbb.", AT::kErrorBadOverlap},
      {"..........BBB.|.......bbb....", AT::kErrorBadOverlap},
      {"......BBB.....|.....bbb......", AT::kErrorBadOverlap},
      {"......BBB.....|..bbb.........", AT::kErrorBadOverlap},
      {"......BBB.....|bbb...........", AT::kErrorBadOverlap},
      {"BBB...........|bbb...........", OK},  // Disjoint.
      {"........BBB...|.........bbb..", OK},  // Disjoint.
      {"BBB...........|..........bbb.", OK},  // Disjoint, offset elsewhere.
      //....AAA.......|.....aaa......   The first Unit. NOLINT
      {".BBB..........|..bbb.........", OK},  // Tangent.
      {".......BBB....|........bbb...", OK},  // Tangent.
      {".BBB..........|........bbb...", OK},  // Tangent, offset elsewhere.
      {"BBBBBB........|bbb...........", OK},  // Repeat, with extra offsets.
      {"........BBBB..|.........bbb..", OK},
      {"BBBBBB........|..........bbb.", OK},
      {".BBBBBB.......|..bbb.........", OK},
      {".......BBBBB..|........bbb...", OK},
      //....AAA.......|.....aaa......   The first Unit. NOLINT
      {".BBB..........|........bbb...", OK},  // Tangent, offset elsewhere.
      {"..BBB.........|........bbb...", AT::kErrorBadOverlap},
      {"...BB.........|....bb........", OK},
      {"....BB........|.....bb.......", OK},
      {".......BB.....|........bb....", OK},
      {"...BBBBBB.....|....bbbbbb....", OK},
      {"..BBBBBB......|...bbbbbb.....", OK},
      {"......BBBBBB..|.......bbbbbb.", OK},
      //....AAA.......|.....aaa......   The first Unit. NOLINT
      {"BBBBBBBBBBBBBB|bbbbbbbbbbbbbb", AT::kErrorBadOverlap},
      {"B.............|b.............", OK},
      {"B.............|.............b", OK},
      {"....B.........|.....b........", OK},
      {"....B.........|......b.......", AT::kErrorBadOverlap},
      {"....B.........|......b.......", AT::kErrorBadOverlap},
      {"....BBB.......|.....bb.......", OK},
      {"....BBBB......|.....bbb......", OK},
      //....AAA.......|.....aaa......   The first Unit. NOLINT
      {".........BBBBB|.b............", OK},
      {"....AAA.......|.....!........", OK},
      {"....!.........|.....!........", OK},  // Empty units gets deleted early.
      {"....!.........|..........!...", OK},  // Forgiving!
  };

  TwoUnitOverlapTester::RunTest(unit_str1, test_cases);
}

TEST(AddressTranslatorTest, OverlapOffsetMultiple) {
  using AT = AddressTranslator;
  // Simple case. Note that RVA ranges don't get merged.
  SimpleTest({"A..|a....",  //
              ".A.|..a..",  //
              "..A|....a"},
             AT::kSuccess, "Case #0");

  // Offset range 1 overlaps 2 and 3, but truncation takes place to trim down
  // offset ranges, so still successful.
  SimpleTest({"..A|a....",  //
              ".AA|..a..",  //
              "AAA|....a"},
             AT::kSuccess, "Case #1");

  // Offset range 2 and 3 overlap, so fail.
  SimpleTest({"A..|a....",  //
              ".A.|..a..",  //
              ".A.|....a"},
             AT::kErrorBadOverlap, "Case #2");
}

TEST(AddressTranslatorTest, OverlapDangling) {
  using AT = AddressTranslator;
  constexpr auto OK = AT::kSuccess;
  // First Unit has dangling offsets at
  constexpr const char* unit_str1 = "....AAA.......|.....aaaaaa...";

  std::vector<TwoUnitOverlapTester::TestCase> test_cases = {
      //....AAA.......|.....aaaaaa...   The first Unit. NOLINT
      {"....BBB.......|.....bbbbbb...", OK},
      {"....BBB.......|.....bbbbb....", OK},
      {"....BBB.......|.....bbbb.....", OK},
      {"....BBB.......|.....bbb......", OK},
      {".....BBB......|......bbb.....", AT::kErrorBadOverlapDanglingRva},
      {".....BB.......|......bbb.....", OK},
      {"....BBB.......|.....bbbbbbbb.", OK},
      {"..BBBBB.......|...bbbbbbbb...", OK},
      //....AAA.......|.....aaaaaa...   The first Unit. NOLINT
      {"......!.......|.bbb..........", AT::kErrorBadOverlap},
      {"..BBBBB.......|...bbbbb......", OK},
      {".......BBB....|.bbb..........", OK},  // Just tangent: Can go elsewhere.
      {".......BBB....|.bbbb.........", OK},  // Can be another dangling RVA.
      {".......!......|.bbbb.........", OK},  // Same with empty.
      {"......!.......|.......!......", OK},  // Okay, but gets deleted.
      {"......!.......|.......b......", AT::kErrorBadOverlapDanglingRva},
      {"......B.......|.......b......", OK},
      //....AAA.......|.....aaaaaa...   The first Unit. NOLINT
      {"......BBBB....|.......bbbb...", AT::kErrorBadOverlapDanglingRva},
      {"......BB......|.......bb.....", AT::kErrorBadOverlapDanglingRva},
      {"......BB......|bb............", AT::kErrorBadOverlap},
  };

  TwoUnitOverlapTester::RunTest(unit_str1, test_cases);
}

// Tests implementation since algorithm is tricky.
TEST(AddressTranslatorTest, Merge) {
  using AT = AddressTranslator;
  // Merge a bunch of overlapping Units into one big Unit.
  std::vector<std::string> test_case1 = {
      "AAA.......|.aaa......",  // Comment to prevent wrap by formatter.
      "AA........|.aa.......",  //
      "..AAA.....|...aaa....",  //
      "....A.....|.....a....",  //
      ".....AAA..|......aaa.",  //
      "........A.|.........a",  //
  };
  // Try all 6! permutations.
  std::sort(test_case1.begin(), test_case1.end());
  do {
    TestAddressTranslator translator1;
    EXPECT_EQ(AT::kSuccess, translator1.InitializeWithStrings(test_case1));
    EXPECT_EQ(9U, translator1.fake_offset_begin());

    AT::Unit expected{0U, +9U, 1U, +9U};
    EXPECT_EQ(1U, translator1.units_sorted_by_offset().size());
    EXPECT_EQ(expected, translator1.units_sorted_by_offset()[0]);
    EXPECT_EQ(1U, translator1.units_sorted_by_rva().size());
    EXPECT_EQ(expected, translator1.units_sorted_by_rva()[0]);
  } while (std::next_permutation(test_case1.begin(), test_case1.end()));

  // Merge RVA-adjacent Units into two Units.
  std::vector<std::string> test_case2 = {
      ".....A..|.a......",  // First Unit.
      "......A.|..a.....",  //
      "A.......|...a....",  // Second Unit: RVA-adjacent to first Unit, but
      ".A......|....a...",  // offset would become inconsistent, so a new
      "..A.....|.....a..",  // Unit gets created.
  };
  // Try all 5! permutations.
  std::sort(test_case2.begin(), test_case2.end());
  do {
    TestAddressTranslator translator2;
    EXPECT_EQ(AT::kSuccess, translator2.InitializeWithStrings(test_case2));
    EXPECT_EQ(7U, translator2.fake_offset_begin());

    AT::Unit expected1{0U, +3U, 3U, +3U};
    AT::Unit expected2{5U, +2U, 1U, +2U};
    EXPECT_EQ(2U, translator2.units_sorted_by_offset().size());
    EXPECT_EQ(expected1, translator2.units_sorted_by_offset()[0]);
    EXPECT_EQ(expected2, translator2.units_sorted_by_offset()[1]);
    EXPECT_EQ(2U, translator2.units_sorted_by_rva().size());
    EXPECT_EQ(expected2, translator2.units_sorted_by_rva()[0]);
    EXPECT_EQ(expected1, translator2.units_sorted_by_rva()[1]);
  } while (std::next_permutation(test_case2.begin(), test_case2.end()));
}

TEST(AddressTranslatorTest, RvaToOffsetCache_IsValid) {
  AddressTranslator translator;
  // Notice that the second section has dangling RVA.
  ASSERT_EQ(AddressTranslator::kSuccess,
            translator.Initialize(
                {{0x04, +0x28, 0x1A00, +0x28}, {0x30, +0x10, 0x3A00, +0x30}}));
  AddressTranslator::RvaToOffsetCache rva_checker(translator);

  EXPECT_FALSE(rva_checker.IsValid(kInvalidRva));

  for (int i = 0; i < 0x28; ++i)
    EXPECT_TRUE(rva_checker.IsValid(0x1A00 + i));
  EXPECT_FALSE(rva_checker.IsValid(0x1A00 + 0x28));
  EXPECT_FALSE(rva_checker.IsValid(0x1A00 + 0x29));
  EXPECT_FALSE(rva_checker.IsValid(0x1A00 - 1));
  EXPECT_FALSE(rva_checker.IsValid(0x1A00 - 2));

  for (int i = 0; i < 0x30; ++i)
    EXPECT_TRUE(rva_checker.IsValid(0x3A00 + i));
  EXPECT_FALSE(rva_checker.IsValid(0x3A00 + 0x30));
  EXPECT_FALSE(rva_checker.IsValid(0x3A00 + 0x31));
  EXPECT_FALSE(rva_checker.IsValid(0x3A00 - 1));
  EXPECT_FALSE(rva_checker.IsValid(0x3A00 - 2));

  EXPECT_FALSE(rva_checker.IsValid(0));
  EXPECT_FALSE(rva_checker.IsValid(0x10));
  EXPECT_FALSE(rva_checker.IsValid(0x7FFFFFFFU));
  EXPECT_FALSE(rva_checker.IsValid(0xFFFFFFFFU));
}

}  // namespace zucchini
