// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/zucchini/rel32_utils.h"

#include <stdint.h>

#include <deque>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/test/gtest_util.h"
#include "components/zucchini/address_translator.h"
#include "components/zucchini/arm_utils.h"
#include "components/zucchini/image_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace zucchini {

namespace {

// A trivial AddressTranslator that applies constant shift.
class TestAddressTranslator : public AddressTranslator {
 public:
  TestAddressTranslator(offset_t image_size, rva_t rva_begin) {
    DCHECK_GE(rva_begin, 0U);
    CHECK_EQ(AddressTranslator::kSuccess,
             Initialize({{0, image_size, rva_begin, image_size}}));
  }
};

// Checks that |reader| emits and only emits |expected_refs|, in order.
void CheckReader(const std::vector<Reference>& expected_refs,
                 std::unique_ptr<ReferenceReader> reader) {
  for (Reference expected_ref : expected_refs) {
    auto ref = reader->GetNext();
    EXPECT_TRUE(ref.has_value());
    EXPECT_EQ(expected_ref, ref.value());
  }
  EXPECT_EQ(std::nullopt, reader->GetNext());  // Nothing should be left.
}

using ArmCopyDispFun = bool (*)(ConstBufferView src_view,
                                offset_t src_idx,
                                MutableBufferView dst_view,
                                offset_t dst_idx);

// Copies displacements from |bytes1| to |bytes2| and checks results against
// |bytes_exp_1_to_2|. Then repeats for |*bytes2| , |*byte1|, and
// |bytes_exp_2_to_1|. Empty expected bytes mean failure is expected. The copy
// function is specified by |copier|.
void CheckCopy(const std::vector<uint8_t>& bytes_exp_1_to_2,
               const std::vector<uint8_t>& bytes_exp_2_to_1,
               const std::vector<uint8_t>& bytes1,
               const std::vector<uint8_t>& bytes2,
               ArmCopyDispFun copier) {
  auto run_test = [&copier](const std::vector<uint8_t>& bytes_exp,
                            const std::vector<uint8_t>& bytes_in,
                            std::vector<uint8_t> bytes_out) {
    ConstBufferView buffer_in(&bytes_in[0], bytes_in.size());
    MutableBufferView buffer_out(&bytes_out[0], bytes_out.size());
    if (bytes_exp.empty()) {
      EXPECT_FALSE(copier(buffer_in, 0U, buffer_out, 0U));
    } else {
      EXPECT_TRUE(copier(buffer_in, 0U, buffer_out, 0U));
      EXPECT_EQ(bytes_exp, bytes_out);
    }
  };
  run_test(bytes_exp_1_to_2, bytes1, bytes2);
  run_test(bytes_exp_2_to_1, bytes2, bytes1);
}

}  // namespace

TEST(Rel32UtilsTest, Rel32ReaderX86) {
  constexpr offset_t kTestImageSize = 0x00100000U;
  constexpr rva_t kRvaBegin = 0x00030000U;
  TestAddressTranslator translator(kTestImageSize, kRvaBegin);

  // For simplicity, test data is not real X86 machine code. We are only
  // including rel32 targets, without the full instructions.
  std::vector<uint8_t> bytes = {
      0xFF, 0xFF, 0xFF, 0xFF,  // 00030000: (Filler)
      0xFF, 0xFF, 0xFF, 0xFF,  // 0003000C: (Filler)
      0x04, 0x00, 0x00, 0x00,  // 00030008: 00030010
      0xFF, 0xFF, 0xFF, 0xFF,  // 0003000C: (Filler)
      0x00, 0x00, 0x00, 0x00,  // 00030010: 00030014
      0xFF, 0xFF, 0xFF, 0xFF,  // 00030014: (Filler)
      0xF4, 0xFF, 0xFF, 0xFF,  // 00030018: 00030010
      0xE4, 0xFF, 0xFF, 0xFF,  // 0003001C: 00030004
  };
  ConstBufferView buffer(bytes.data(), bytes.size());
  // Specify rel32 locations directly, instead of parsing.
  std::deque<offset_t> rel32_locations = {0x0008U, 0x0010U, 0x0018U, 0x001CU};

  // Generate everything.
  auto reader1 = std::make_unique<Rel32ReaderX86>(buffer, 0x0000U, 0x0020U,
                                                  &rel32_locations, translator);
  CheckReader({{0x0008U, 0x0010U},
               {0x0010U, 0x0014U},
               {0x0018U, 0x0010U},
               {0x001CU, 0x0004U}},
              std::move(reader1));

  // Exclude last.
  auto reader2 = std::make_unique<Rel32ReaderX86>(buffer, 0x0000U, 0x001CU,
                                                  &rel32_locations, translator);
  CheckReader({{0x0008U, 0x0010U}, {0x0010U, 0x0014U}, {0x0018U, 0x0010U}},
              std::move(reader2));

  // Only find one.
  auto reader3 = std::make_unique<Rel32ReaderX86>(buffer, 0x000CU, 0x0018U,
                                                  &rel32_locations, translator);
  CheckReader({{0x0010U, 0x0014U}}, std::move(reader3));
}

TEST(Rel32UtilsTest, Rel32WriterX86) {
  constexpr offset_t kTestImageSize = 0x00100000U;
  constexpr rva_t kRvaBegin = 0x00030000U;
  TestAddressTranslator translator(kTestImageSize, kRvaBegin);

  std::vector<uint8_t> bytes(32, 0xFF);
  MutableBufferView buffer(bytes.data(), bytes.size());

  Rel32WriterX86 writer(buffer, translator);
  writer.PutNext({0x0008U, 0x0010U});
  EXPECT_EQ(0x00000004U, buffer.read<uint32_t>(0x08));  // 00030008: 00030010

  writer.PutNext({0x0010U, 0x0014U});
  EXPECT_EQ(0x00000000U, buffer.read<uint32_t>(0x10));  // 00030010: 00030014

  writer.PutNext({0x0018U, 0x0010U});
  EXPECT_EQ(0xFFFFFFF4U, buffer.read<uint32_t>(0x18));  // 00030018: 00030010

  writer.PutNext({0x001CU, 0x0004U});
  EXPECT_EQ(0xFFFFFFE4U, buffer.read<uint32_t>(0x1C));  // 0003001C: 00030004

  EXPECT_EQ(std::vector<uint8_t>({
                0xFF, 0xFF, 0xFF, 0xFF,  // 00030000: (Filler)
                0xFF, 0xFF, 0xFF, 0xFF,  // 00030004: (Filler)
                0x04, 0x00, 0x00, 0x00,  // 00030008: 00030010
                0xFF, 0xFF, 0xFF, 0xFF,  // 0003000C: (Filler)
                0x00, 0x00, 0x00, 0x00,  // 00030010: 00030014
                0xFF, 0xFF, 0xFF, 0xFF,  // 00030014: (Filler)
                0xF4, 0xFF, 0xFF, 0xFF,  // 00030018: 00030010
                0xE4, 0xFF, 0xFF, 0xFF,  // 0003001C: 00030004
            }),
            bytes);
}

TEST(Rel32UtilsTest, Rel32ReaderArm_AArch32) {
  constexpr offset_t kTestImageSize = 0x00100000U;
  constexpr rva_t kRvaBegin = 0x00030000U;
  TestAddressTranslator translator(kTestImageSize, kRvaBegin);

  // A24.
  std::vector<uint8_t> bytes = {
      0xFF, 0xFF, 0xFF, 0xFF,  // 00030000: (Filler)
      0xFF, 0xFF, 0xFF, 0xFF,  // 00030004: (Filler)
      0x00, 0x00, 0x00, 0xEA,  // 00030008: B   00030010 ; A24
      0xFF, 0xFF, 0xFF, 0xFF,  // 0003000C: (Filler)
      0xFF, 0xFF, 0xFF, 0xEB,  // 00030010: BL  00030014 ; A24
      0xFF, 0xFF, 0xFF, 0xFF,  // 00030014: (Filler)
      0xFC, 0xFF, 0xFF, 0xEB,  // 00030018: BL  00030010 ; A24
      0xF8, 0xFF, 0xFF, 0xEA,  // 0003001C: B   00030004 ; A24
  };
  ConstBufferView region(&bytes[0], bytes.size());
  // Specify rel32 locations directly, instead of parsing.
  std::deque<offset_t> rel32_locations_A24 = {0x0008U, 0x0010U, 0x0018U,
                                              0x001CU};

  // Generate everything.
  auto reader1 =
      std::make_unique<Rel32ReaderArm<AArch32Rel32Translator::AddrTraits_A24>>(
          translator, region, rel32_locations_A24, 0x0000U, 0x0020U);
  CheckReader({{0x0008U, 0x0010U},
               {0x0010U, 0x0014U},
               {0x0018U, 0x0010U},
               {0x001CU, 0x0004U}},
              std::move(reader1));

  // Exclude last.
  auto reader2 =
      std::make_unique<Rel32ReaderArm<AArch32Rel32Translator::AddrTraits_A24>>(
          translator, region, rel32_locations_A24, 0x0000U, 0x001CU);
  CheckReader({{0x0008U, 0x0010U}, {0x0010U, 0x0014U}, {0x0018U, 0x0010U}},
              std::move(reader2));

  // Only find one.
  auto reader3 =
      std::make_unique<Rel32ReaderArm<AArch32Rel32Translator::AddrTraits_A24>>(
          translator, region, rel32_locations_A24, 0x000CU, 0x0018U);
  CheckReader({{0x0010U, 0x0014U}}, std::move(reader3));
}

TEST(Rel32UtilsTest, Rel32WriterArm_AArch32_Easy) {
  constexpr offset_t kTestImageSize = 0x00100000U;
  constexpr rva_t kRvaBegin = 0x00030000U;
  TestAddressTranslator translator(kTestImageSize, kRvaBegin);

  std::vector<uint8_t> bytes = {
      0xFF, 0xFF,              // 00030000: (Filler)
      0x01, 0xDE,              // 00030002: B   00030008 ; T8
      0xFF, 0xFF, 0xFF, 0xFF,  // 00030004: (Filler)
      0x01, 0xE0,              // 00030008: B   0003000E ; T11
      0xFF, 0xFF,              // 0003000A: (Filler)
      0x80, 0xF3, 0x00, 0x80,  // 0003000C: B   00030010 ; T20
  };
  MutableBufferView region(&bytes[0], bytes.size());

  auto writer1 =
      std::make_unique<Rel32WriterArm<AArch32Rel32Translator::AddrTraits_T8>>(
          translator, region);
  writer1->PutNext({0x0002U, 0x0004U});
  EXPECT_EQ(0xFF, bytes[0x02]);  // 00030002: B   00030004 ; T8
  EXPECT_EQ(0xDE, bytes[0x03]);

  writer1->PutNext({0x0002U, 0x000AU});
  EXPECT_EQ(0x02, bytes[0x02]);  // 00030002: B   0003000A ; T8
  EXPECT_EQ(0xDE, bytes[0x03]);

  auto writer2 =
      std::make_unique<Rel32WriterArm<AArch32Rel32Translator::AddrTraits_T11>>(
          translator, region);
  writer2->PutNext({0x0008U, 0x0008U});
  EXPECT_EQ(0xFE, bytes[0x08]);  // 00030008: B   00030008 ; T11
  EXPECT_EQ(0xE7, bytes[0x09]);
  writer2->PutNext({0x0008U, 0x0010U});
  EXPECT_EQ(0x02, bytes[0x08]);  // 00030008: B   00030010 ; T11
  EXPECT_EQ(0xE0, bytes[0x09]);

  auto writer3 =
      std::make_unique<Rel32WriterArm<AArch32Rel32Translator::AddrTraits_T20>>(
          translator, region);
  writer3->PutNext({0x000CU, 0x000AU});
  EXPECT_EQ(0xBF, bytes[0x0C]);  // 0003000C: B   0003000A ; T20
  EXPECT_EQ(0xF7, bytes[0x0D]);
  EXPECT_EQ(0xFD, bytes[0x0E]);
  EXPECT_EQ(0xAF, bytes[0x0F]);
  writer3->PutNext({0x000CU, 0x0010U});
  EXPECT_EQ(0x80, bytes[0x0C]);  // 0003000C: B   00030010 ; T20
  EXPECT_EQ(0xF3, bytes[0x0D]);
  EXPECT_EQ(0x00, bytes[0x0E]);
  EXPECT_EQ(0x80, bytes[0x0F]);
}

TEST(Rel32UtilsTest, Rel32WriterArm_AArch32_Hard) {
  constexpr offset_t kTestImageSize = 0x10000000U;
  constexpr rva_t kRvaBegin = 0x0C030000U;
  TestAddressTranslator translator(kTestImageSize, kRvaBegin);

  std::vector<uint8_t> bytes = {
      0xFF, 0xFF,              // 0C030000: (Filler)
      0x00, 0xF0, 0x00, 0xB8,  // 0C030002: B   0C030006 ; T24
      0xFF, 0xFF, 0xFF, 0xFF,  // 0C030006: (Filler)
      0x00, 0xF0, 0x7A, 0xE8,  // 0C03000A: BLX 0C030100 ; T24
      0xFF, 0xFF,              // 0C03000E: (Filler)
      0x00, 0xF0, 0x7A, 0xE8,  // 0C030010: BLX 0C030108 ; T24
  };
  MutableBufferView region(&bytes[0], bytes.size());

  auto writer =
      std::make_unique<Rel32WriterArm<AArch32Rel32Translator::AddrTraits_T24>>(
          translator, region);
  writer->PutNext({0x0002U, 0x0000U});
  EXPECT_EQ(0xFF, bytes[0x02]);  // 0C030002: B   0C030000 ; T24
  EXPECT_EQ(0xF7, bytes[0x03]);
  EXPECT_EQ(0xFD, bytes[0x04]);
  EXPECT_EQ(0xBF, bytes[0x05]);
  writer->PutNext({0x0002U, 0x0008U});
  EXPECT_EQ(0x00, bytes[0x02]);  // 0C030002: B   0C030008 ; T24
  EXPECT_EQ(0xF0, bytes[0x03]);
  EXPECT_EQ(0x01, bytes[0x04]);
  EXPECT_EQ(0xB8, bytes[0x05]);

  // BLX complication, with location that's not 4-byte aligned.
  writer->PutNext({0x000AU, 0x0010U});
  EXPECT_EQ(0x00, bytes[0x0A]);  // 0C03000A: BLX 0C030010 ; T24
  EXPECT_EQ(0xF0, bytes[0x0B]);
  EXPECT_EQ(0x02, bytes[0x0C]);
  EXPECT_EQ(0xE8, bytes[0x0D]);
  writer->PutNext({0x000AU, 0x0100U});
  EXPECT_EQ(0x00, bytes[0x0A]);  // 0C03000A: BLX 0C030100 ; T24
  EXPECT_EQ(0xF0, bytes[0x0B]);
  EXPECT_EQ(0x7A, bytes[0x0C]);
  EXPECT_EQ(0xE8, bytes[0x0D]);
  writer->PutNext({0x000AU, 0x0000U});
  EXPECT_EQ(0xFF, bytes[0x0A]);  // 0C03000A: BLX 0C030000 ; T24
  EXPECT_EQ(0xF7, bytes[0x0B]);
  EXPECT_EQ(0xFA, bytes[0x0C]);
  EXPECT_EQ(0xEF, bytes[0x0D]);

  // BLX complication, with location that's 4-byte aligned.
  writer->PutNext({0x0010U, 0x0010U});
  EXPECT_EQ(0xFF, bytes[0x10]);  // 0C030010: BLX 0C030010 ; T24
  EXPECT_EQ(0xF7, bytes[0x11]);
  EXPECT_EQ(0xFE, bytes[0x12]);
  EXPECT_EQ(0xEF, bytes[0x13]);
  writer->PutNext({0x0010U, 0x0108U});
  EXPECT_EQ(0x00, bytes[0x10]);  // 0C030010: BLX 0C030108 ; T24
  EXPECT_EQ(0xF0, bytes[0x11]);
  EXPECT_EQ(0x7A, bytes[0x12]);
  EXPECT_EQ(0xE8, bytes[0x13]);
}

// Test BLX encoding A2, which is an ARM instruction that switches to THUMB2,
// and therefore should have 2-byte alignment.
TEST(Rel32UtilsTest, AArch32SwitchToThumb2) {
  constexpr offset_t kTestImageSize = 0x10000000U;
  constexpr rva_t kRvaBegin = 0x08030000U;
  TestAddressTranslator translator(kTestImageSize, kRvaBegin);

  std::vector<uint8_t> bytes = {
      0xFF, 0xFF, 0x00, 0x00,  // 08030000: (Filler)
      0x00, 0x00, 0x00, 0xFA,  // 08030004: BLX 0803000C ; A24
  };
  MutableBufferView region(&bytes[0], bytes.size());

  auto writer =
      std::make_unique<Rel32WriterArm<AArch32Rel32Translator::AddrTraits_A24>>(
          translator, region);

  // To location that's 4-byte aligned.
  writer->PutNext({0x0004U, 0x0100U});
  EXPECT_EQ(0x3D, bytes[0x04]);  // 08030004: BLX 08030100 ; A24
  EXPECT_EQ(0x00, bytes[0x05]);
  EXPECT_EQ(0x00, bytes[0x06]);
  EXPECT_EQ(0xFA, bytes[0x07]);

  // To location that's 2-byte aligned but not 4-byte aligned.
  writer->PutNext({0x0004U, 0x0052U});
  EXPECT_EQ(0x11, bytes[0x04]);  // 08030004: BLX 08030052 ; A24
  EXPECT_EQ(0x00, bytes[0x05]);
  EXPECT_EQ(0x00, bytes[0x06]);
  EXPECT_EQ(0xFB, bytes[0x07]);

  // Clean slate code.
  writer->PutNext({0x0004U, 0x000CU});
  EXPECT_EQ(0x00, bytes[0x04]);  // 08030004: BLX 0803000C ; A24
  EXPECT_EQ(0x00, bytes[0x05]);
  EXPECT_EQ(0x00, bytes[0x06]);
  EXPECT_EQ(0xFA, bytes[0x07]);
}

TEST(Rel32UtilsTest, ArmCopyDisp_AArch32) {
  std::vector<uint8_t> expect_fail;

  // Successful A24.
  ArmCopyDispFun copier_A24 =
      ArmCopyDisp<AArch32Rel32Translator::AddrTraits_A24>;
  CheckCopy({0x12, 0x34, 0x56, 0xEB},  // 00000100: BL     0158D150
            {0xA0, 0xC0, 0x0E, 0x2A},  // 00000100: BCS    003B0388
            {0x12, 0x34, 0x56, 0x2A},  // 00000100: BCS    0158D150
            {0xA0, 0xC0, 0x0E, 0xEB},  // 00000100: BL     003B0388
            copier_A24);

  // Successful T8.
  ArmCopyDispFun copier_T8 = ArmCopyDisp<AArch32Rel32Translator::AddrTraits_T8>;
  CheckCopy({0x12, 0xD5},  // 00000100: BPL    00000128
            {0xAB, 0xD8},  // 00000100: BHI    0000005A
            {0x12, 0xD8},  // 00000100: BHI    00000128
            {0xAB, 0xD5},  // 00000100: BPL    0000005A
            copier_T8);

  // Successful T11.
  ArmCopyDispFun copier_T11 =
      ArmCopyDisp<AArch32Rel32Translator::AddrTraits_T11>;
  CheckCopy({0xF5, 0xE0},  // 00000100: B      000002EE
            {0x12, 0xE7},  // 00000100: B      FFFFFF28
            {0xF5, 0xE0},  // 00000100: B      000002EE
            {0x12, 0xE7},  // 00000100: B      FFFFFF28
            copier_T11);

  // Failure if wrong copier is used.
  CheckCopy(expect_fail, expect_fail, {0xF5, 0xE0}, {0x12, 0xE7}, copier_T8);

  // Successful T20.
  ArmCopyDispFun copier_T20 =
      ArmCopyDisp<AArch32Rel32Translator::AddrTraits_T20>;
  CheckCopy({0x41, 0xF2, 0xA5, 0x88},  // 00000100: BLS.W   0008124E
            {0x04, 0xF3, 0x3C, 0xA2},  // 00000100: BGT.W   0004457C
            {0x01, 0xF3, 0xA5, 0x88},  // 00000100: BGT.W   0008124E
            {0x44, 0xF2, 0x3C, 0xA2},  // 00000100: BLS.W   0004457C
            copier_T20);
  CheckCopy({0x7F, 0xF6, 0xFF, 0xAF},  // 00000100: BLS.W   00000102
            {0x00, 0xF3, 0x00, 0x80},  // 00000100: BGT.W   00000104
            {0x3F, 0xF7, 0xFF, 0xAF},  // 00000100: BGT.W   00000102
            {0x40, 0xF2, 0x00, 0x80},  // 00000100: BLS.W   00000104
            copier_T20);

  // Failure if wrong copier is used.
  CheckCopy(expect_fail, expect_fail, {0x41, 0xF2, 0xA5, 0x88},
            {0x84, 0xF3, 0x3C, 0xA2}, copier_A24);

  // T24: Mix B encoding T4 and BL encoding T1.
  ArmCopyDispFun copier_T24 =
      ArmCopyDisp<AArch32Rel32Translator::AddrTraits_T24>;
  CheckCopy({0xFF, 0xF7, 0xFF, 0xFF},  // 00000100: BL      00000102
            {0x00, 0xF0, 0x00, 0x90},  // 00000100: B.W     00C00104
            {0xFF, 0xF7, 0xFF, 0xBF},  // 00000100: B.W     00000102
            {0x00, 0xF0, 0x00, 0xD0},  // 00000100: BL      00C00104
            copier_T24);

  // Mix B encoding T4 and BLX encoding T2. Note that the forward direction
  // fails because B's target is invalid for BLX! It's possible to do "best
  // effort" copying to reduce diff -- but right now we're not doing this.
  CheckCopy(expect_fail, {0x00, 0xF0, 0x00, 0x90},  // 00000100: B.W 00C00104
            {0xFF, 0xF7, 0xFF, 0xBF},  // 00000100: B.W     00000102
            {0x00, 0xF0, 0x00, 0xC0},  // 00000100: BLX     00C00104
            copier_T24);
  // Success if ow B's target is valid for BLX.
  CheckCopy({0xFF, 0xF7, 0xFE, 0xEF},  // 00000100: BLX     00000100
            {0x00, 0xF0, 0x00, 0x90},  // 00000100: B.W     00C00104
            {0xFF, 0xF7, 0xFE, 0xBF},  // 00000100: B.W     00000100
            {0x00, 0xF0, 0x00, 0xC0},  // 00000100: BLX     00C00104
            copier_T24);
}

TEST(Rel32UtilsTest, Rel32ReaderArm_AArch64) {
  constexpr offset_t kTestImageSize = 0x00100000U;
  constexpr rva_t kRvaBegin = 0x00030000U;
  TestAddressTranslator translator(kTestImageSize, kRvaBegin);

  std::vector<uint8_t> bytes = {
      0xFF, 0xFF, 0xFF, 0xFF,  // 00030000: (Filler)
      0xFF, 0xFF, 0xFF, 0xFF,  // 00030004: (Filler)
      0x02, 0x00, 0x00, 0x14,  // 00030008: B    00030010 ; Immd26
      0xFF, 0xFF, 0xFF, 0xFF,  // 0003000C: (Filler)
      0x25, 0x00, 0x00, 0x35,  // 00030010: CBNZ R5,00030014 ; Immd19
      0xFF, 0xFF, 0xFF, 0xFF,  // 00030014: (Filler)
      0xCA, 0xFF, 0xFF, 0x54,  // 00030018: BGE  00030010 ; Immd19
      0x4C, 0xFF, 0x8F, 0x36,  // 0003001C: TBZ  X12,#17,00030004 ; Immd14
  };
  MutableBufferView region(&bytes[0], bytes.size());

  // Generate Immd26. We specify rel32 locations directly.
  std::deque<offset_t> rel32_locations_Immd26 = {0x0008U};
  auto reader1 = std::make_unique<
      Rel32ReaderArm<AArch64Rel32Translator::AddrTraits_Immd26>>(
      translator, region, rel32_locations_Immd26, 0x0000U, 0x0020U);
  CheckReader({{0x0008U, 0x0010U}}, std::move(reader1));

  // Generate Immd19.
  std::deque<offset_t> rel32_locations_Immd19 = {0x0010U, 0x0018U};
  auto reader2 = std::make_unique<
      Rel32ReaderArm<AArch64Rel32Translator::AddrTraits_Immd19>>(
      translator, region, rel32_locations_Immd19, 0x0000U, 0x0020U);
  CheckReader({{0x0010U, 0x0014U}, {0x0018U, 0x0010U}}, std::move(reader2));

  // Generate Immd14.
  std::deque<offset_t> rel32_locations_Immd14 = {0x001CU};
  auto reader3 = std::make_unique<
      Rel32ReaderArm<AArch64Rel32Translator::AddrTraits_Immd14>>(
      translator, region, rel32_locations_Immd14, 0x0000U, 0x0020U);
  CheckReader({{0x001CU, 0x0004U}}, std::move(reader3));
}

TEST(Rel32UtilsTest, Rel32WriterArm_AArch64) {
  constexpr offset_t kTestImageSize = 0x00100000U;
  constexpr rva_t kRvaBegin = 0x00030000U;
  TestAddressTranslator translator(kTestImageSize, kRvaBegin);

  std::vector<uint8_t> bytes = {
      0xFF, 0xFF, 0xFF, 0xFF,  // 00030000: (Filler)
      0xFF, 0xFF, 0xFF, 0xFF,  // 00030004: (Filler)
      0x02, 0x00, 0x00, 0x14,  // 00030008: B    00030010 ; Immd26
      0xFF, 0xFF, 0xFF, 0xFF,  // 0003000C: (Filler)
      0x25, 0x00, 0x00, 0x35,  // 00030010: CBNZ R5,00030014 ; Immd19
      0xFF, 0xFF, 0xFF, 0xFF,  // 00030014: (Filler)
      0xCA, 0xFF, 0xFF, 0x54,  // 00030018: BGE  00030010 ; Immd19
      0x4C, 0xFF, 0x8F, 0x36,  // 0003001C: TBZ  X12,#17,00030004 ; Immd14
  };
  MutableBufferView region(&bytes[0], bytes.size());

  auto writer1 = std::make_unique<
      Rel32WriterArm<AArch64Rel32Translator::AddrTraits_Immd26>>(translator,
                                                                 region);
  writer1->PutNext({0x0008U, 0x0000U});
  EXPECT_EQ(0xFE, bytes[0x08]);  // 00030008: B    00030000 ; Immd26
  EXPECT_EQ(0xFF, bytes[0x09]);
  EXPECT_EQ(0xFF, bytes[0x0A]);
  EXPECT_EQ(0x17, bytes[0x0B]);

  auto writer2 = std::make_unique<
      Rel32WriterArm<AArch64Rel32Translator::AddrTraits_Immd19>>(translator,
                                                                 region);
  writer2->PutNext({0x0010U, 0x0000U});
  EXPECT_EQ(0x85, bytes[0x10]);  // 00030010: CBNZ R5,00030000 ; Immd19
  EXPECT_EQ(0xFF, bytes[0x11]);
  EXPECT_EQ(0xFF, bytes[0x12]);
  EXPECT_EQ(0x35, bytes[0x13]);
  writer2->PutNext({0x0018U, 0x001CU});
  EXPECT_EQ(0x2A, bytes[0x18]);  // 00030018: BGE  0003001C ; Immd19
  EXPECT_EQ(0x00, bytes[0x19]);
  EXPECT_EQ(0x00, bytes[0x1A]);
  EXPECT_EQ(0x54, bytes[0x1B]);

  auto writer3 = std::make_unique<
      Rel32WriterArm<AArch64Rel32Translator::AddrTraits_Immd14>>(translator,
                                                                 region);
  writer3->PutNext({0x001CU, 0x0010U});
  EXPECT_EQ(0xAC, bytes[0x1C]);  // 0003001C: TBZ  X12,#17,00030010 ; Immd14
  EXPECT_EQ(0xFF, bytes[0x1D]);
  EXPECT_EQ(0x8F, bytes[0x1E]);
  EXPECT_EQ(0x36, bytes[0x1F]);
}

TEST(Rel32UtilsTest, ArmCopyDisp_AArch64) {
  std::vector<uint8_t> expect_fail;

  // Successful Imm26.
  ArmCopyDispFun copier_Immd26 =
      ArmCopyDisp<AArch64Rel32Translator::AddrTraits_Immd26>;
  CheckCopy({0x12, 0x34, 0x56, 0x94},  // 00000100: BL     0158D148
            {0xA1, 0xC0, 0x0E, 0x17},  // 00000100: B      FC3B0384
            {0x12, 0x34, 0x56, 0x14},  // 00000100: B      0158D148
            {0xA1, 0xC0, 0x0E, 0x97},  // 00000100: BL     FC3B0384
            copier_Immd26);

  // Successful Imm19.
  ArmCopyDispFun copier_Immd19 =
      ArmCopyDisp<AArch64Rel32Translator::AddrTraits_Immd19>;
  CheckCopy({0x24, 0x12, 0x34, 0x54},  // 00000100: BMI    00068344
            {0xD7, 0xA5, 0xFC, 0xB4},  // 00000100: CBZ    X23,FFFF95B8
            {0x37, 0x12, 0x34, 0xB4},  // 00000100: CBZ    X23,00068344
            {0xC4, 0xA5, 0xFC, 0x54},  // 00000100: BMI    FFFF95B8
            copier_Immd19);

  // Successful Imm14.
  ArmCopyDispFun copier_Immd14 =
      ArmCopyDisp<AArch64Rel32Translator::AddrTraits_Immd14>;
  CheckCopy({0x00, 0x00, 0x00, 0x36},  // 00000100: TBZ    X0,#0,00000100
            {0xFF, 0xFF, 0xFF, 0xB7},  // 00000100: TBNZ   ZR,#63,000000FC
            {0x1F, 0x00, 0xF8, 0xB7},  // 00000100: TBNZ   ZR,#63,00000100
            {0xE0, 0xFF, 0x07, 0x36},  // 00000100: TBZ    X0,#0,000000FC
            copier_Immd14);

  // Failure if wrong copier is used.
  CheckCopy(expect_fail, expect_fail, {0x1F, 0x00, 0xF8, 0xB7},
            {0xE0, 0xFF, 0x07, 0x36}, copier_Immd26);
}

}  // namespace zucchini
