// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/zucchini/rel32_finder.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/format_macros.h"
#include "base/strings/stringprintf.h"
#include "components/zucchini/buffer_view.h"
#include "components/zucchini/image_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace zucchini {

TEST(Abs32GapFinderTest, All) {
  const size_t kRegionTotal = 99;
  std::vector<uint8_t> buffer(kRegionTotal);
  ConstBufferView image(buffer.data(), buffer.size());

  // Common test code that returns the resulting segments as a string.
  auto run_test = [&](size_t rlo, size_t rhi,
                      std::vector<offset_t> abs32_locations,
                      std::ptrdiff_t abs32_width) -> std::string {
    CHECK_LE(rlo, kRegionTotal);
    CHECK_LE(rhi, kRegionTotal);
    CHECK(std::is_sorted(abs32_locations.begin(), abs32_locations.end()));
    CHECK_GT(abs32_width, 0);
    ConstBufferView region =
        ConstBufferView::FromRange(image.begin() + rlo, image.begin() + rhi);
    Abs32GapFinder gap_finder(image, region, abs32_locations, abs32_width);

    std::string out_str;
    while (gap_finder.FindNext()) {
      ConstBufferView gap = gap_finder.GetGap();
      size_t lo = static_cast<size_t>(gap.begin() - image.begin());
      size_t hi = static_cast<size_t>(gap.end() - image.begin());
      out_str.append(base::StringPrintf("[%" PRIuS ",%" PRIuS ")", lo, hi));
    }
    return out_str;
  };

  // Empty regions yield empty segments.
  EXPECT_EQ("", run_test(0, 0, std::vector<offset_t>(), 4));
  EXPECT_EQ("", run_test(9, 9, std::vector<offset_t>(), 4));
  EXPECT_EQ("", run_test(8, 8, {8}, 4));
  EXPECT_EQ("", run_test(8, 8, {0, 12}, 4));

  // If no abs32 locations exist then the segment is the main range.
  EXPECT_EQ("[0,99)", run_test(0, 99, std::vector<offset_t>(), 4));
  EXPECT_EQ("[20,21)", run_test(20, 21, std::vector<offset_t>(), 4));
  EXPECT_EQ("[51,55)", run_test(51, 55, std::vector<offset_t>(), 4));

  // abs32 locations found near start of main range.
  EXPECT_EQ("[10,20)", run_test(10, 20, {5}, 4));
  EXPECT_EQ("[10,20)", run_test(10, 20, {6}, 4));
  EXPECT_EQ("[11,20)", run_test(10, 20, {7}, 4));
  EXPECT_EQ("[12,20)", run_test(10, 20, {8}, 4));
  EXPECT_EQ("[13,20)", run_test(10, 20, {9}, 4));
  EXPECT_EQ("[14,20)", run_test(10, 20, {10}, 4));
  EXPECT_EQ("[10,11)[15,20)", run_test(10, 20, {11}, 4));

  // abs32 locations found near end of main range.
  EXPECT_EQ("[10,15)[19,20)", run_test(10, 20, {15}, 4));
  EXPECT_EQ("[10,16)", run_test(10, 20, {16}, 4));
  EXPECT_EQ("[10,17)", run_test(10, 20, {17}, 4));
  EXPECT_EQ("[10,18)", run_test(10, 20, {18}, 4));
  EXPECT_EQ("[10,19)", run_test(10, 20, {19}, 4));
  EXPECT_EQ("[10,20)", run_test(10, 20, {20}, 4));
  EXPECT_EQ("[10,20)", run_test(10, 20, {21}, 4));

  // Main range completely eclipsed by abs32 location.
  EXPECT_EQ("", run_test(10, 11, {7}, 4));
  EXPECT_EQ("", run_test(10, 11, {8}, 4));
  EXPECT_EQ("", run_test(10, 11, {9}, 4));
  EXPECT_EQ("", run_test(10, 11, {10}, 4));
  EXPECT_EQ("", run_test(10, 12, {8}, 4));
  EXPECT_EQ("", run_test(10, 12, {9}, 4));
  EXPECT_EQ("", run_test(10, 12, {10}, 4));
  EXPECT_EQ("", run_test(10, 13, {9}, 4));
  EXPECT_EQ("", run_test(10, 13, {10}, 4));
  EXPECT_EQ("", run_test(10, 14, {10}, 4));
  EXPECT_EQ("", run_test(10, 14, {8, 12}, 4));

  // Partial eclipses.
  EXPECT_EQ("[24,25)", run_test(20, 25, {20}, 4));
  EXPECT_EQ("[20,21)", run_test(20, 25, {21}, 4));
  EXPECT_EQ("[20,21)[25,26)", run_test(20, 26, {21}, 4));

  // abs32 location outside main range.
  EXPECT_EQ("[40,60)", run_test(40, 60, {36, 60}, 4));
  EXPECT_EQ("[41,61)", run_test(41, 61, {0, 10, 20, 30, 34, 62, 68, 80}, 4));

  // Change abs32 width.
  EXPECT_EQ("[10,11)[12,14)[16,19)", run_test(10, 20, {9, 11, 14, 15, 19}, 1));
  EXPECT_EQ("", run_test(10, 11, {10}, 1));
  EXPECT_EQ("[18,23)[29,31)", run_test(17, 31, {15, 23, 26, 31}, 3));
  EXPECT_EQ("[17,22)[25,26)[29,30)", run_test(17, 31, {14, 22, 26, 30}, 3));
  EXPECT_EQ("[10,11)[19,20)", run_test(10, 20, {11}, 8));

  // Mixed cases with abs32 width = 4.
  EXPECT_EQ("[10,15)[19,20)[24,25)", run_test(8, 25, {2, 6, 15, 20, 27}, 4));
  EXPECT_EQ("[0,25)[29,45)[49,50)", run_test(0, 50, {25, 45}, 4));
  EXPECT_EQ("[10,20)[28,50)", run_test(10, 50, {20, 24}, 4));
  EXPECT_EQ("[49,50)[54,60)[64,70)[74,80)[84,87)",
            run_test(49, 87, {10, 20, 30, 40, 50, 60, 70, 80, 90}, 4));
  EXPECT_EQ("[0,10)[14,20)[24,25)[29,50)", run_test(0, 50, {10, 20, 25}, 4));
}

namespace {

// A mock Rel32Finder to inject next search result on Scan().
class TestRel32Finder : public Rel32Finder {
 public:
  using Rel32Finder::Rel32Finder;

  // Rel32Finder:
  NextIterators Scan(ConstBufferView region) override { return next_result; }

  NextIterators next_result;
};

AddressTranslator GetTrivialTranslator(size_t size) {
  AddressTranslator translator;
  EXPECT_EQ(AddressTranslator::kSuccess,
            translator.Initialize({{0, static_cast<offset_t>(size), 0U,
                                    static_cast<rva_t>(size)}}));
  return translator;
}

}  // namespace

TEST(Rel32FinderTest, Scan) {
  const size_t kRegionTotal = 99;
  std::vector<uint8_t> buffer(kRegionTotal);
  ConstBufferView image(buffer.data(), buffer.size());
  AddressTranslator translator(GetTrivialTranslator(image.size()));
  TestRel32Finder finder(image, translator);
  finder.SetRegion(image);

  auto check_finder_state = [&](const TestRel32Finder& finder,
                                size_t expected_cursor,
                                size_t expected_accept_it) {
    CHECK_LE(expected_cursor, kRegionTotal);
    CHECK_LE(expected_accept_it, kRegionTotal);

    EXPECT_EQ(image.begin() + expected_cursor, finder.region().begin());
    EXPECT_EQ(image.begin() + expected_accept_it, finder.accept_it());
  };

  check_finder_state(finder, 0, 0);

  finder.next_result = {image.begin() + 1, image.begin() + 1};
  EXPECT_TRUE(finder.FindNext());
  check_finder_state(finder, 1, 1);

  finder.next_result = {image.begin() + 2, image.begin() + 2};
  EXPECT_TRUE(finder.FindNext());
  check_finder_state(finder, 2, 2);

  finder.next_result = {image.begin() + 5, image.begin() + 6};
  EXPECT_TRUE(finder.FindNext());
  check_finder_state(finder, 5, 6);
  finder.Accept();
  check_finder_state(finder, 6, 6);

  finder.next_result = {image.begin() + 7, image.begin() + 7};
  EXPECT_TRUE(finder.FindNext());
  check_finder_state(finder, 7, 7);

  finder.next_result = {image.begin() + 8, image.begin() + 8};
  EXPECT_TRUE(finder.FindNext());
  check_finder_state(finder, 8, 8);

  finder.next_result = {image.begin() + 99, image.begin() + 99};
  EXPECT_TRUE(finder.FindNext());
  check_finder_state(finder, 99, 99);

  finder.next_result = {nullptr, nullptr};
  EXPECT_FALSE(finder.FindNext());
  check_finder_state(finder, 99, 99);
}

namespace {

// X86 test data. (x) and +x entries are covered by abs32 references, which have
// width = 4.
constexpr uint8_t kDataX86[] = {
    0x55,                                // 00: push  ebp
    0x8B,   0xEC,                        // 01: mov   ebp,esp
    0xE8,   0,      0,   0,   0,         // 03: call  08
    (0xE9), +0,     +0,  +0,  0,         // 08: jmp   0D
    0x0F,   0x80,   0,   0,   0,   0,    // 0D: jo    13
    0x0F,   0x81,   0,   0,   (0), +0,   // 13: jno   19
    +0x0F,  +0x82,  0,   0,   0,   0,    // 19: jb    1F
    0x0F,   0x83,   0,   0,   0,   0,    // 1F: jae   25
    0x0F,   (0x84), +0,  +0,  +0,  (0),  // 25: je    2B
    +0x0F,  +0x85,  +0,  0,   0,   0,    // 2B: jne   31
    0x0F,   0x86,   0,   0,   0,   0,    // 31: jbe   37
    0x0F,   0x87,   0,   0,   0,   0,    // 37: ja    3D
    0x0F,   0x88,   0,   (0), +0,  +0,   // 3D: js    43
    +0x0F,  0x89,   0,   0,   0,   0,    // 43: jns   49
    0x0F,   0x8A,   0,   0,   0,   0,    // 49: jp    4F
    0x0F,   0x8B,   (0), +0,  +0,  +0,   // 4F: jnp   55
    0x0F,   0x8C,   0,   0,   0,   0,    // 55: jl    5B
    0x0F,   0x8D,   0,   0,   (0), +0,   // 5B: jge   61
    +0x0F,  +0x8E,  (0), +0,  +0,  +0,   // 61: jle   67
    0x0F,   0x8F,   0,   0,   0,   0,    // 67: jg    6D
    0x5D,                                // 6D: pop   ebp
    0xC3,                                // C3: ret
};

// Abs32 locations corresponding to |kDataX86|, with width = 4.
constexpr uint8_t kAbs32X86[] = {0x08, 0x17, 0x26, 0x2A,
                                 0x40, 0x51, 0x5F, 0x63};

}  // namespace

TEST(Rel32FinderX86Test, FindNext) {
  ConstBufferView image =
      ConstBufferView::FromRange(std::begin(kDataX86), std::end(kDataX86));
  AddressTranslator translator(GetTrivialTranslator(image.size()));
  Rel32FinderX86 rel_finder(image, translator);
  rel_finder.SetRegion(image);

  // List of expected locations as pairs of {cursor offset, rel32 offset},
  // ignoring |kAbs32X86|.
  std::vector<std::pair<size_t, size_t>> expected_locations = {
      {0x04, 0x04}, {0x09, 0x09}, {0x0E, 0x0F}, {0x14, 0x15}, {0x1A, 0x1B},
      {0x20, 0x21}, {0x26, 0x27}, {0x2C, 0x2D}, {0x32, 0x33}, {0x38, 0x39},
      {0x3E, 0x3F}, {0x44, 0x45}, {0x4A, 0x4B}, {0x50, 0x51}, {0x56, 0x57},
      {0x5C, 0x5D}, {0x62, 0x63}, {0x68, 0x69},
  };
  for (auto location : expected_locations) {
    EXPECT_TRUE(rel_finder.FindNext());
    auto rel32 = rel_finder.GetRel32();

    EXPECT_EQ(location.first,
              size_t(rel_finder.region().begin() - image.begin()));
    EXPECT_EQ(location.second, rel32.location);
    EXPECT_EQ(image.begin() + (rel32.location + 4), rel_finder.accept_it());
    EXPECT_FALSE(rel32.can_point_outside_section);
    rel_finder.Accept();
  }
  EXPECT_FALSE(rel_finder.FindNext());
}

TEST(Rel32FinderX86Test, Integrated) {
  // Truncated form of Rel32FinderIntel::Result.
  typedef std::pair<offset_t, rva_t> TruncatedResults;

  ConstBufferView image =
      ConstBufferView::FromRange(std::begin(kDataX86), std::end(kDataX86));
  std::vector<offset_t> abs32_locations(std::begin(kAbs32X86),
                                        std::end(kAbs32X86));
  std::vector<TruncatedResults> rel32_results;

  Abs32GapFinder gap_finder(image, image, abs32_locations, 4U);
  AddressTranslator translator(GetTrivialTranslator(image.size()));
  Rel32FinderX86 rel_finder(image, translator);
  while (gap_finder.FindNext()) {
    auto gap = gap_finder.GetGap();
    rel_finder.SetRegion(gap_finder.GetGap());
    while (rel_finder.FindNext()) {
      auto rel32 = rel_finder.GetRel32();
      rel32_results.emplace_back(
          TruncatedResults{rel32.location, rel32.target_rva});
    }
  }

  std::vector<TruncatedResults> expected_rel32_results = {
      {0x04, 0x08},
      /* {0x09, 0x0D}, */ {0x0F, 0x13},
      /* {0x15, 0x19}, */ /*{0x1B, 0x1F}, */
      {0x21, 0x25},
      /* {0x27, 0x2B}, */ /* {0x2D, 0x31}, */ {0x33, 0x37},
      {0x39, 0x3D},
      /* {0x3F, 0x43}, */ /* {0x45, 0x49}, */ {0x4B, 0x4F},
      /* {0x51, 0x55}, */ {0x57, 0x5B},
      /* {0x5D, 0x61}, */ /* {0x63, 0x67}, */ {0x69, 0x6D},
  };
  EXPECT_EQ(expected_rel32_results, rel32_results);
}

TEST(Rel32FinderX86Test, Accept) {
  constexpr uint8_t data[] = {
      0xB9, 0x00, 0x00, 0x00, 0xE9,  // 00: mov   E9000000
      0xE8, 0x00, 0x00, 0x00, 0xE9,  // 05: call  E900000A
      0xE8, 0x00, 0x00, 0x00, 0xE9,  // 0A: call  E900000F
  };

  ConstBufferView image =
      ConstBufferView::FromRange(std::begin(data), std::end(data));

  auto next_location = [](Rel32FinderX86& rel_finder) -> offset_t {
    EXPECT_TRUE(rel_finder.FindNext());
    auto rel32 = rel_finder.GetRel32();
    return rel32.location;
  };

  AddressTranslator translator(GetTrivialTranslator(image.size()));
  Rel32FinderX86 rel_finder(image, translator);
  rel_finder.SetRegion(image);

  EXPECT_EQ(0x05U, next_location(rel_finder));  // False positive.
  rel_finder.Accept();
  // False negative: shadowed by 0x05
  // EXPECT_EQ(0x06, next_location(rel_finder));
  EXPECT_EQ(0x0AU, next_location(rel_finder));  // False positive.
  EXPECT_EQ(0x0BU, next_location(rel_finder));  // Found if 0x0A is discarded.
}

namespace {

// X64 test data. (x) and +x entries are covered by abs32 references, which have
// width = 8.
constexpr uint8_t kDataX64[] = {
    0x55,                                      // 00: push  ebp
    0x8B,   0xEC,                              // 01: mov   ebp,esp
    0xE8,   0,      0,     0,   0,             // 03: call  08
    0xE9,   0,      0,     0,   (0),           // 08: jmp   0D
    +0x0F,  +0x80,  +0,    +0,  +0,  +0,       // 0D: jo    13
    +0x0F,  0x81,   0,     0,   0,   0,        // 13: jno   19
    0x0F,   0x82,   0,     0,   0,   0,        // 19: jb    1F
    (0x0F), +0x83,  +0,    +0,  +0,  +0,       // 1F: jae   25
    +0x0F,  +0x84,  0,     0,   0,   0,        // 25: je    2B
    0x0F,   0x85,   0,     0,   0,   0,        // 2B: jne   31
    0x0F,   0x86,   (0),   +0,  +0,  +0,       // 31: jbe   37
    +0x0F,  +0x87,  +0,    +0,  (0), +0,       // 37: ja    3D
    +0x0F,  +0x88,  +0,    +0,  +0,  +0,       // 3D: js    43
    0x0F,   0x89,   0,     0,   0,   0,        // 43: jns   49
    (0x0F), +0x8A,  +0,    +0,  +0,  +0,       // 49: jp    4F
    +0x0F,  +0x8B,  0,     0,   0,   0,        // 4F: jnp   55
    0x0F,   0x8C,   0,     0,   0,   0,        // 55: jl    5B
    0x0F,   0x8D,   0,     0,   0,   0,        // 5B: jge   61
    0x0F,   0x8E,   0,     0,   0,   0,        // 61: jle   67
    0x0F,   0x8F,   0,     (0), +0,  +0,       // 67: jg    6F
    +0xFF,  +0x15,  +0,    +0,  +0,  0,        // 6D: call  [rip+00]      # 73
    0xFF,   0x25,   0,     0,   0,   0,        // 73: jmp   [rip+00]      # 79
    0x8B,   0x05,   0,     0,   0,   0,        // 79: mov   eax,[rip+00]  # 7F
    0x8B,   0x3D,   0,     0,   0,   0,        // 7F: mov   edi,[rip+00]  # 85
    0x8D,   0x05,   0,     0,   0,   0,        // 85: lea   eax,[rip+00]  # 8B
    0x8D,   0x3D,   0,     0,   0,   0,        // 8B: lea   edi,[rip+00]  # 91
    0x48,   0x8B,   0x05,  0,   0,   0,  0,    // 91: mov   rax,[rip+00]  # 98
    0x48,   (0x8B), +0x3D, +0,  +0,  +0, +0,   // 98: mov   rdi,[rip+00]  # 9F
    +0x48,  +0x8D,  0x05,  0,   0,   0,  0,    // 9F: lea   rax,[rip+00]  # A6
    0x48,   0x8D,   0x3D,  0,   0,   0,  0,    // A6: lea   rdi,[rip+00]  # AD
    0x4C,   0x8B,   0x05,  0,   0,   0,  (0),  // AD: mov   r8,[rip+00]   # B4
    +0x4C,  +0x8B,  +0x3D, +0,  +0,  +0, +0,   // B4: mov   r15,[rip+00]  # BB
    0x4C,   0x8D,   0x05,  0,   0,   0,  0,    // BB: lea   r8,[rip+00]   # C2
    0x4C,   0x8D,   0x3D,  0,   0,   0,  0,    // C2: lea   r15,[rip+00]  # C9
    0x66,   0x8B,   0x05,  (0), +0,  +0, +0,   // C9: mov   ax,[rip+00]   # D0
    +0x66,  +0x8B,  +0x3D, +0,  0,   0,  0,    // D0: mov   di,[rip+00]   # D7
    0x66,   0x8D,   0x05,  0,   0,   0,  0,    // D7: lea   ax,[rip+00]   # DE
    0x66,   0x8D,   0x3D,  0,   0,   0,  0,    // DE: lea   di,[rip+00]   # E5
    0x5D,                                      // E5: pop   ebp
    0xC3,                                      // E6: ret
};

// Abs32 locations corresponding to |kDataX64|, with width = 8.
constexpr uint8_t kAbs32X64[] = {0x0C, 0x1F, 0x33, 0x3B, 0x49,
                                 0x6A, 0x99, 0xB3, 0xCC};

}  // namespace

TEST(Rel32FinderX64Test, FindNext) {
  ConstBufferView image =
      ConstBufferView::FromRange(std::begin(kDataX64), std::end(kDataX64));
  AddressTranslator translator(GetTrivialTranslator(image.size()));
  Rel32FinderX64 rel_finder(image, translator);
  rel_finder.SetRegion(image);

  // Lists of expected locations as pairs of {cursor offset, rel32 offset},
  // ignoring |kAbs32X64|.
  std::vector<std::pair<size_t, size_t>> expected_locations = {
      {0x04, 0x04}, {0x09, 0x09}, {0x0E, 0x0F}, {0x14, 0x15}, {0x1A, 0x1B},
      {0x20, 0x21}, {0x26, 0x27}, {0x2C, 0x2D}, {0x32, 0x33}, {0x38, 0x39},
      {0x3E, 0x3F}, {0x44, 0x45}, {0x4A, 0x4B}, {0x50, 0x51}, {0x56, 0x57},
      {0x5C, 0x5D}, {0x62, 0x63}, {0x68, 0x69},
  };
  std::vector<std::pair<size_t, size_t>> expected_locations_rip = {
      {0x6E, 0x6F}, {0x74, 0x75}, {0x7A, 0x7B}, {0x80, 0x81}, {0x86, 0x87},
      {0x8C, 0x8D}, {0x93, 0x94}, {0x9A, 0x9B}, {0xA1, 0xA2}, {0xA8, 0xA9},
      {0xAF, 0xB0}, {0xB6, 0xB7}, {0xBD, 0xBE}, {0xC4, 0xC5}, {0xCB, 0xCC},
      {0xD2, 0xD3}, {0xD9, 0xDA}, {0xE0, 0xE1},
  };
  // Jump instructions, which cannot point outside section.
  for (auto location : expected_locations) {
    EXPECT_TRUE(rel_finder.FindNext());
    auto rel32 = rel_finder.GetRel32();
    EXPECT_EQ(location.first,
              size_t(rel_finder.region().begin() - image.begin()));
    EXPECT_EQ(location.second, rel32.location);
    EXPECT_EQ(image.begin() + (rel32.location + 4), rel_finder.accept_it());
    EXPECT_FALSE(rel32.can_point_outside_section);
    rel_finder.Accept();
  }
  // PC-relative data access instructions, which can point outside section.
  for (auto location : expected_locations_rip) {
    EXPECT_TRUE(rel_finder.FindNext());
    auto rel32 = rel_finder.GetRel32();
    EXPECT_EQ(location.first,
              size_t(rel_finder.region().begin() - image.begin()));
    EXPECT_EQ(location.second, rel32.location);
    EXPECT_EQ(image.begin() + (rel32.location + 4), rel_finder.accept_it());
    EXPECT_TRUE(rel32.can_point_outside_section);  // Different from before.
    rel_finder.Accept();
  }
  EXPECT_FALSE(rel_finder.FindNext());
}

TEST(Rel32FinderX64Test, Integrated) {
  // Truncated form of Rel32FinderIntel::Result.
  typedef std::pair<offset_t, rva_t> TruncatedResults;

  ConstBufferView image =
      ConstBufferView::FromRange(std::begin(kDataX64), std::end(kDataX64));
  std::vector<offset_t> abs32_locations(std::begin(kAbs32X64),
                                        std::end(kAbs32X64));
  std::vector<TruncatedResults> rel32_results;

  Abs32GapFinder gap_finder(image, image, abs32_locations, 8U);
  AddressTranslator translator(GetTrivialTranslator(image.size()));
  Rel32FinderX64 rel_finder(image, translator);
  while (gap_finder.FindNext()) {
    auto gap = gap_finder.GetGap();
    rel_finder.SetRegion(gap_finder.GetGap());
    while (rel_finder.FindNext()) {
      auto rel32 = rel_finder.GetRel32();
      rel32_results.emplace_back(
          TruncatedResults{rel32.location, rel32.target_rva});
    }
  }

  std::vector<TruncatedResults> expected_rel32_results = {
      {0x04, 0x08},
      /* {0x09, 0x0D}, */
      /*{0x0F, 0x13}, */ /* {0x15, 0x19}, */ {0x1B, 0x1F},
      /* {0x21, 0x25}, */ /* {0x27, 0x2B}, */ {0x2D, 0x31},
      /* {0x33, 0x37}, */ /* {0x39, 0x3D}, */
      /* {0x3F, 0x43}, */ {0x45, 0x49},
      /* {0x4B, 0x4F}, */ /* {0x51, 0x55}, */
      {0x57, 0x5B},
      {0x5D, 0x61},
      {0x63, 0x67}, /* {0x69, 0x6F}, */
      /* {0x6F, 0x73}, */ {0x75, 0x79},
      {0x7B, 0x7F},
      {0x81, 0x85},
      {0x87, 0x8B},
      {0x8D, 0x91},
      {0x94, 0x98},
      /* {0x9B, 0x9F}, */ /* {0xA2, 0xA6}, */ {0xA9, 0xAD},
      /* {0xB0, 0xB4}, */ /* {0xB7, 0xBB}, */ {0xBE, 0xC2},
      {0xC5, 0xC9},
      /* {0xCC, 0xD0}, */ /* {0xD3, 0xD7}, */ {0xDA, 0xDE},
      {0xE1, 0xE5},
  };
  EXPECT_EQ(expected_rel32_results, rel32_results);
}

}  // namespace zucchini
