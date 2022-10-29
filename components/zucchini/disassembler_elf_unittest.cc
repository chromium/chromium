// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/zucchini/disassembler_elf.h"

#include <stddef.h>
#include <stdint.h>

#include <random>
#include <string>
#include <vector>

#include "base/ranges/algorithm.h"
#include "components/zucchini/test_utils.h"
#include "components/zucchini/type_elf.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace zucchini {

TEST(DisassemblerElfTest, IsTargetOffsetInElfSectionList) {
  // Minimal required fields for IsTargetOffsetInElfSectionList().
  struct FakeElfShdr {
    offset_t sh_offset;
    offset_t sh_size;
  };

  // Calls IsTargetOffsetInElfSectionList() for fixed |sorted_list|, and sweeps
  // offsets in [lo, hi). Renders results into a string consisting of '.' (not
  // in list) and '*' (in list).
  auto test = [&](const std::vector<FakeElfShdr>& sorted_list, offset_t lo,
                  offset_t hi) -> std::string {
    // Ensure |sorted_list| is indeed sorted, without overlaps.
    for (size_t i = 1; i < sorted_list.size(); ++i) {
      if (sorted_list[i].sh_offset <
          sorted_list[i - 1].sh_offset + sorted_list[i - 1].sh_size) {
        return "(Bad input)";
      }
    }
    // The interface to IsTargetOffsetInElfSectionList() takes a list of
    // pointers (since data can be casted from images), so make the conversion.
    std::vector<const FakeElfShdr*> ptr_list;
    for (const FakeElfShdr& header : sorted_list)
      ptr_list.push_back(&header);
    std::string result;
    for (offset_t offset = lo; offset < hi; ++offset) {
      result += IsTargetOffsetInElfSectionList(ptr_list, offset) ? '*' : '.';
    }
    return result;
  };

  EXPECT_EQ("..........", test(std::vector<FakeElfShdr>(), 0, 10));
  EXPECT_EQ("*.........", test({{0, 1}}, 0, 10));
  EXPECT_EQ("...*......", test({{3, 1}}, 0, 10));
  EXPECT_EQ("...****...", test({{3, 4}}, 0, 10));
  EXPECT_EQ("...****...", test({{10003, 4}}, 10000, 10010));
  EXPECT_EQ("...********...", test({{3, 4}, {7, 4}}, 0, 14));
  EXPECT_EQ("...****.****...", test({{3, 4}, {8, 4}}, 0, 15));
  EXPECT_EQ("...****..****...", test({{3, 4}, {9, 4}}, 0, 16));
  EXPECT_EQ("..****...*****..", test({{2, 4}, {9, 5}}, 0, 16));
  EXPECT_EQ("...***......***..", test({{3, 3}, {12, 3}}, 0, 17));

  // Many small ranges.
  EXPECT_EQ("..**.**.*.*...*.*.**...**.*.**.*..",  // (Comment strut).
            test({{2, 2},
                  {5, 2},
                  {8, 1},
                  {10, 1},
                  {14, 1},
                  {16, 1},
                  {18, 2},
                  {23, 2},
                  {26, 1},
                  {28, 2},
                  {31, 1}},
                 0, 34));
  EXPECT_EQ("..*****.****.***.**.*..",
            test({{137, 5}, {143, 4}, {148, 3}, {152, 2}, {155, 1}}, 135, 158));
  // Consecutive.
  EXPECT_EQ("..***************..",
            test({{137, 5}, {142, 4}, {146, 3}, {149, 2}, {151, 1}}, 135, 154));
  // Hover around 32 (power of 2).
  EXPECT_EQ("..*******************************..",
            test({{2002, 31}}, 2000, 2035));
  EXPECT_EQ("..********************************..",
            test({{5002, 32}}, 5000, 5036));
  EXPECT_EQ("..*********************************..",
            test({{8002, 33}}, 8000, 8037));
  // Consecutive + small gap.
  EXPECT_EQ(
      "..*****************.***********..",
      test({{9876543, 8}, {9876551, 9}, {9876561, 11}}, 9876541, 9876574));
  // Sample internal of big range.
  EXPECT_EQ("**************************************************",
            test({{100, 1000000}}, 5000, 5050));
  // Sample boundaries of big range.
  EXPECT_EQ(".........................*************************",
            test({{100, 1000000}}, 75, 125));
  EXPECT_EQ("*************************.........................",
            test({{100, 1000000}}, 1000075, 1000125));
  // 1E9 is still good.
  EXPECT_EQ(".....*.....", test({{1000000000, 1}}, 999999995, 1000000006));
}

TEST(DisassemblerElfTest, QuickDetect) {
  std::vector<uint8_t> image_data;
  ConstBufferView image;

  // Empty.
  EXPECT_FALSE(DisassemblerElfX86::QuickDetect(image));
  EXPECT_FALSE(DisassemblerElfX64::QuickDetect(image));

  // Unrelated.
  image_data = ParseHexString("DE AD");
  image = {image_data.data(), image_data.size()};
  EXPECT_FALSE(DisassemblerElfX86::QuickDetect(image));
  EXPECT_FALSE(DisassemblerElfX64::QuickDetect(image));

  // Only Magic.
  image_data = ParseHexString("7F 45 4C 46");
  image = {image_data.data(), image_data.size()};
  EXPECT_FALSE(DisassemblerElfX86::QuickDetect(image));
  EXPECT_FALSE(DisassemblerElfX64::QuickDetect(image));

  // Only identification.
  image_data =
      ParseHexString("7F 45 4C 46 01 01 01 00 00 00 00 00 00 00 00 00");
  image = {image_data.data(), image_data.size()};
  EXPECT_FALSE(DisassemblerElfX86::QuickDetect(image));
  EXPECT_FALSE(DisassemblerElfX64::QuickDetect(image));

  // Large enough, filled with zeros.
  image_data.assign(sizeof(elf::Elf32_Ehdr), 0);
  image = {image_data.data(), image_data.size()};
  EXPECT_FALSE(DisassemblerElfX86::QuickDetect(image));
  EXPECT_FALSE(DisassemblerElfX64::QuickDetect(image));

  // Random.
  std::random_device rd;
  std::mt19937 gen{rd()};
  std::generate(image_data.begin(), image_data.end(), gen);
  image = {image_data.data(), image_data.size()};
  EXPECT_FALSE(DisassemblerElfX86::QuickDetect(image));
  EXPECT_FALSE(DisassemblerElfX64::QuickDetect(image));

  // Typical x86 elf header.
  {
    elf::Elf32_Ehdr header = {};
    auto e_ident =
        ParseHexString("7F 45 4C 46 01 01 01 00 00 00 00 00 00 00 00 00");
    base::ranges::copy(e_ident, header.e_ident);
    header.e_type = elf::ET_EXEC;
    header.e_machine = elf::EM_386;
    header.e_version = 1;
    header.e_shentsize = sizeof(elf::Elf32_Shdr);
    image = {reinterpret_cast<const uint8_t*>(&header), sizeof(header)};
    EXPECT_TRUE(DisassemblerElfX86::QuickDetect(image));
    EXPECT_FALSE(DisassemblerElfX64::QuickDetect(image));
  }

  // Typical x64 elf header.
  {
    elf::Elf64_Ehdr header = {};
    auto e_ident =
        ParseHexString("7F 45 4C 46 02 01 01 00 00 00 00 00 00 00 00 00");
    base::ranges::copy(e_ident, header.e_ident);
    header.e_type = elf::ET_EXEC;
    header.e_machine = elf::EM_X86_64;
    header.e_version = 1;
    header.e_shentsize = sizeof(elf::Elf64_Shdr);
    image = {reinterpret_cast<const uint8_t*>(&header), sizeof(header)};
    EXPECT_FALSE(DisassemblerElfX86::QuickDetect(image));
    EXPECT_TRUE(DisassemblerElfX64::QuickDetect(image));
  }
}

}  // namespace zucchini
