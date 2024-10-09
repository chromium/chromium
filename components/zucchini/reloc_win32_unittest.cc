// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/zucchini/reloc_win32.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/test/gtest_util.h"
#include "components/zucchini/address_translator.h"
#include "components/zucchini/algorithm.h"
#include "components/zucchini/image_utils.h"
#include "components/zucchini/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace zucchini {

class RelocUtilsWin32Test : public testing::Test {
 protected:
  using Units = std::vector<RelocUnitWin32>;

  RelocUtilsWin32Test() = default;

  // Resets all tester data, calls RelocRvaReaderWin32::FindRelocBlocks(), and
  // returns its results.
  bool Initialize(const std::vector<uint8_t>& image_raw,
                  BufferRegion reloc_region) {
    image_ = BufferSource(image_raw.data(), image_raw.size());
    reloc_region_ = reloc_region;
    return RelocRvaReaderWin32::FindRelocBlocks(image_, reloc_region_,
                                                &reloc_block_offsets_);
  }

  // Uses RelocRvaReaderWin32 to get all relocs, returned as Units.
  Units EmitAll(offset_t lo, offset_t hi) {
    RelocRvaReaderWin32 reader(image_, reloc_region_, reloc_block_offsets_, lo,
                               hi);
    Units units;
    for (auto unit = reader.GetNext(); unit.has_value();
         unit = reader.GetNext()) {
      units.push_back(unit.value());
    }
    return units;
  }

  ConstBufferView image_;
  BufferRegion reloc_region_;
  std::vector<uint32_t> reloc_block_offsets_;
};

TEST_F(RelocUtilsWin32Test, RvaReaderEmpty) {
  {
    std::vector<uint8_t> image_raw = ParseHexString("");
    EXPECT_TRUE(Initialize(image_raw, {0U, 0U}));
    EXPECT_EQ(std::vector<uint32_t>(), reloc_block_offsets_);  // Nothing.
    EXPECT_EQ(Units(), EmitAll(0U, 0U));
  }
  {
    std::vector<uint8_t> image_raw = ParseHexString("AA BB CC DD EE FF");
    EXPECT_TRUE(Initialize(image_raw, {2U, 0U}));
    EXPECT_EQ(std::vector<uint32_t>(), reloc_block_offsets_);  // Nothing.
    EXPECT_EQ(Units(), EmitAll(2U, 2U));
  }
  {
    std::vector<uint8_t> image_raw = ParseHexString("00 C0 00 00 08 00 00 00");
    EXPECT_TRUE(Initialize(image_raw, {0U, image_raw.size()}));
    EXPECT_EQ(std::vector<uint32_t>({0U}),
              reloc_block_offsets_);  // Empty block.
    EXPECT_EQ(Units(), EmitAll(0U, 8U));
  }
}

TEST_F(RelocUtilsWin32Test, RvaReaderBad) {
  std::string test_cases[] = {
      "00 C0 00 00 07 00 00",           // Header too small.
      "00 C0 00 00 08 00 00",           // Header too small, lies about size.
      "00 C0 00 00 0A 00 00 00 66 31",  // Odd number of units.
      "00 C0 00 00 0C 00 00 00 66 31 88 31 FF",  // Trailing data.
  };
  for (const std::string& test_case : test_cases) {
    std::vector<uint8_t> image_raw = ParseHexString(test_case);
    EXPECT_FALSE(Initialize(image_raw, {0U, image_raw.size()}));
  }
}

TEST_F(RelocUtilsWin32Test, RvaReaderSingle) {
  // Block 0: All type 0x3: {0xC166, 0xC288, 0xC342, (padding) 0xCFFF}.
  std::vector<uint8_t> image_raw = ParseHexString(
      "FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF "
      "00 C0 00 00 10 00 00 00 66 31 88 32 42 33 FF 0F "
      "FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF");
  constexpr offset_t kBlock0 = 16U;
  Units exp0 = {{3, kBlock0 + 8U, 0xC166U},
                {3, kBlock0 + 10U, 0xC288U},
                {3, kBlock0 + 12U, 0xC342U},
                {0, kBlock0 + 14U, 0xCFFFU}};

  EXPECT_TRUE(Initialize(image_raw, {16U, 16U}));
  EXPECT_EQ(exp0, EmitAll(kBlock0, kBlock0 + 16U));
  EXPECT_EQ(Units(), EmitAll(kBlock0, kBlock0));
  EXPECT_EQ(Units(), EmitAll(kBlock0, kBlock0 + 8U));
  EXPECT_EQ(Units(), EmitAll(kBlock0, kBlock0 + 9U));
  EXPECT_EQ(Sub(exp0, 0, 1), EmitAll(kBlock0, kBlock0 + 10U));
  EXPECT_EQ(Sub(exp0, 0, 1), EmitAll(kBlock0 + 8U, kBlock0 + 10U));
  EXPECT_EQ(Units(), EmitAll(kBlock0 + 9U, kBlock0 + 10U));
  EXPECT_EQ(Sub(exp0, 0, 3), EmitAll(kBlock0, kBlock0 + 15U));
  EXPECT_EQ(Sub(exp0, 2, 3), EmitAll(kBlock0 + 11U, kBlock0 + 15U));
}

TEST_F(RelocUtilsWin32Test, RvaReaderMulti) {
  // The sample image encodes 3 reloc blocks:
  // Block 0: All type 0x3: {0xC166, 0xC288, 0xC344, (padding) 0xCFFF}.
  // Block 1: All type 0x3: {0x12166, 0x12288}.
  // Block 2: All type 0xA: {0x24000, 0x24010, 0x24020, 0x24028, 0x24A3C,
  //                         0x24170}.
  std::vector<uint8_t> image_raw = ParseHexString(
      "FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF "
      "00 C0 00 00 10 00 00 00 66 31 88 32 42 33 FF 0F "
      "00 20 01 00 0C 00 00 00 66 31 88 32 "
      "00 40 02 00 14 00 00 00 00 A0 10 A0 20 A0 28 A0 3C A0 70 A1 "
      "FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF");
  offset_t image_size = base::checked_cast<offset_t>(image_raw.size());
  constexpr offset_t kBlock0 = 16U;
  constexpr offset_t kBlock1 = kBlock0 + 16U;
  constexpr offset_t kBlock2 = kBlock1 + 12U;
  constexpr offset_t kBlockEnd = kBlock2 + 20U;
  Units exp0 = {{3, kBlock0 + 8U, 0xC166U},
                {3, kBlock0 + 10U, 0xC288U},
                {3, kBlock0 + 12U, 0xC342U},
                {0, kBlock0 + 14U, 0xCFFFU}};
  Units exp1 = {{3, kBlock0 + 24U, 0x12166U}, {3, kBlock0 + 26U, 0x12288U}};
  Units exp2 = {{10, kBlock0 + 36U, 0x24000U}, {10, kBlock0 + 38U, 0x24010U},
                {10, kBlock0 + 40U, 0x24020U}, {10, kBlock0 + 42U, 0x24028U},
                {10, kBlock0 + 44U, 0x2403CU}, {10, kBlock0 + 46U, 0x24170U}};

  EXPECT_TRUE(Initialize(image_raw, {kBlock0, kBlockEnd - kBlock0}));
  EXPECT_EQ(std::vector<uint32_t>({kBlock0, kBlock1, kBlock2}),
            reloc_block_offsets_);

  // Everything.
  EXPECT_EQ(Cat(Cat(exp0, exp1), exp2), EmitAll(kBlock0, kBlockEnd));
  EXPECT_EQ(Cat(Cat(exp0, exp1), exp2), EmitAll(0, image_size));
  // Entire blocks.
  EXPECT_EQ(exp0, EmitAll(kBlock0, kBlock1));
  EXPECT_EQ(exp1, EmitAll(kBlock1, kBlock2));
  EXPECT_EQ(exp2, EmitAll(kBlock2, kBlockEnd));
  EXPECT_EQ(Units(), EmitAll(0, kBlock0));
  EXPECT_EQ(Units(), EmitAll(kBlockEnd, image_size));
  // Within blocks, clipped at boundaries.
  EXPECT_EQ(exp0, EmitAll(kBlock0 + 5U, kBlock1));
  EXPECT_EQ(exp0, EmitAll(kBlock0 + 8U, kBlock1));
  EXPECT_EQ(Sub(exp0, 1, 4), EmitAll(kBlock0 + 9U, kBlock1));
  EXPECT_EQ(Sub(exp0, 0, 3), EmitAll(kBlock0, kBlock0 + 15U));
  EXPECT_EQ(Sub(exp0, 0, 3), EmitAll(kBlock0, kBlock0 + 14U));
  EXPECT_EQ(Sub(exp0, 0, 1), EmitAll(kBlock0 + 8U, kBlock0 + 10U));
  EXPECT_EQ(Sub(exp1, 1, 2), EmitAll(kBlock1 + 10U, kBlock1 + 12U));
  EXPECT_EQ(Sub(exp2, 2, 4), EmitAll(kBlock2 + 12U, kBlock2 + 16U));
  EXPECT_EQ(Units(), EmitAll(kBlock0, kBlock0));
  EXPECT_EQ(Units(), EmitAll(kBlock0, kBlock0 + 8U));
  EXPECT_EQ(Units(), EmitAll(kBlock2 + 10U, kBlock2 + 11U));
  EXPECT_EQ(Units(), EmitAll(kBlock2 + 11U, kBlock2 + 12U));
  // Across blocks.
  EXPECT_EQ(Cat(Cat(exp0, exp1), exp2), EmitAll(kBlock0 - 5U, kBlockEnd));
  EXPECT_EQ(Cat(Cat(exp0, exp1), exp2), EmitAll(kBlock0 + 6U, kBlockEnd));
  EXPECT_EQ(Cat(Cat(exp0, exp1), Sub(exp2, 0, 5)),
            EmitAll(kBlock0 + 6U, kBlock2 + 18U));
  EXPECT_EQ(Cat(Sub(exp0, 2, 4), Sub(exp1, 0, 1)),
            EmitAll(kBlock0 + 12U, kBlock1 + 10U));
  EXPECT_EQ(Cat(Sub(exp0, 2, 4), Sub(exp1, 0, 1)),
            EmitAll(kBlock0 + 11U, kBlock1 + 10U));
  EXPECT_EQ(Cat(Sub(exp0, 2, 4), Sub(exp1, 0, 1)),
            EmitAll(kBlock0 + 12U, kBlock1 + 11U));
  EXPECT_EQ(Sub(exp1, 1, 2), EmitAll(kBlock1 + 10U, kBlock2 + 5U));
  EXPECT_EQ(Cat(Sub(exp1, 1, 2), exp2), EmitAll(kBlock1 + 10U, kBlockEnd + 5));
  EXPECT_EQ(Units(), EmitAll(kBlock0 + 15, kBlock1 + 9));
}

TEST_F(RelocUtilsWin32Test, ReadWrite) {
  // Set up mock image: Size = 0x3000, .reloc at 0x600. RVA is 0x40000 + offset.
  constexpr rva_t kBaseRva = 0x40000;
  std::vector<uint8_t> image_data(0x3000, 0xFF);
  // 4 x86 relocs (xx 3x), 3 x64 relocs (xx Ax), 1 padding (xx 0X).
  std::vector<uint8_t> reloc_data = ParseHexString(
      "00 10 04 00 10 00 00 00 C0 32 18 A3 F8 A7 FF 0F "
      "00 20 04 00 10 00 00 00 80 A0 65 31 F8 37 BC 3A");
  reloc_region_ = {0x600, reloc_data.size()};
  base::ranges::copy(reloc_data, image_data.begin() + reloc_region_.lo());
  image_ = {image_data.data(), image_data.size()};
  offset_t image_size = base::checked_cast<offset_t>(image_.size());

  AddressTranslator translator;
  translator.Initialize({{0, image_size, kBaseRva, image_size}});

  // Precompute |reloc_block_offsets_|.
  EXPECT_TRUE(RelocRvaReaderWin32::FindRelocBlocks(image_, reloc_region_,
                                                   &reloc_block_offsets_));
  EXPECT_EQ(std::vector<uint32_t>({0x600U, 0x610U}), reloc_block_offsets_);

  // Focus on x86.
  constexpr uint16_t kRelocTypeX86 = 3;
  constexpr offset_t kVAWidthX86 = 4;

  // Make RelocRvaReaderWin32.
  RelocRvaReaderWin32 reloc_rva_reader(image_, reloc_region_,
                                       reloc_block_offsets_, 0, image_size);
  offset_t offset_bound = image_size - kVAWidthX86 + 1;

  // Make RelocReaderWin32 that wraps |reloc_rva_reader|.
  auto reader = std::make_unique<RelocReaderWin32>(
      std::move(reloc_rva_reader), kRelocTypeX86, offset_bound, translator);

  // Read all references and check.
  std::vector<Reference> refs;
  for (std::optional<Reference> ref = reader->GetNext(); ref.has_value();
       ref = reader->GetNext()) {
    refs.push_back(ref.value());
  }
  std::vector<Reference> exp_refs{
      {0x608, 0x12C0}, {0x61A, 0x2165}, {0x61C, 0x27F8}, {0x61E, 0x2ABC}};
  EXPECT_EQ(exp_refs, refs);

  // Write reference, extract bytes and check.
  MutableBufferView mutable_image(&image_data[0], image_data.size());
  auto writer = std::make_unique<RelocWriterWin32>(
      kRelocTypeX86, mutable_image, reloc_region_, reloc_block_offsets_,
      translator);

  writer->PutNext({0x608, 0x1F83});
  std::vector<uint8_t> exp_reloc_data1 = ParseHexString(
      "00 10 04 00 10 00 00 00 83 3F 18 A3 F8 A7 FF 0F "
      "00 20 04 00 10 00 00 00 80 A0 65 31 F8 37 BC 3A");
  EXPECT_EQ(exp_reloc_data1,
            Sub(image_data, reloc_region_.lo(), reloc_region_.hi()));

  writer->PutNext({0x61C, 0x2950});
  std::vector<uint8_t> exp_reloc_data2 = ParseHexString(
      "00 10 04 00 10 00 00 00 83 3F 18 A3 F8 A7 FF 0F "
      "00 20 04 00 10 00 00 00 80 A0 65 31 50 39 BC 3A");
  EXPECT_EQ(exp_reloc_data2,
            Sub(image_data, reloc_region_.lo(), reloc_region_.hi()));
}

}  // namespace zucchini
