// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/zucchini/abs32_utils.h"

#include <stdint.h>

#include <algorithm>
#include <string>
#include <utility>

#include "base/numerics/safe_conversions.h"
#include "components/zucchini/address_translator.h"
#include "components/zucchini/image_utils.h"
#include "components/zucchini/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace zucchini {

namespace {

// A trivial AddressTranslator that applies constant shift.
class TestAddressTranslator : public AddressTranslator {
 public:
  TestAddressTranslator(size_t image_size, rva_t rva_begin) {
    DCHECK_GE(rva_begin, 0U);
    CHECK_EQ(AddressTranslator::kSuccess,
             Initialize({{0, base::checked_cast<offset_t>(image_size),
                          rva_begin, base::checked_cast<rva_t>(image_size)}}));
  }
};

// Helper to translate address |value| to RVA. May return |kInvalidRva|.
rva_t AddrValueToRva(uint64_t value, AbsoluteAddress* addr) {
  *addr->mutable_value() = value;
  return addr->ToRva();
}

}  // namespace

TEST(Abs32UtilsTest, AbsoluteAddress32) {
  std::vector<uint8_t> data32 = ParseHexString(
      "00 00 32 00  21 43 65 4A  00 00 00 00  FF FF FF FF  FF FF 31 00");
  ConstBufferView image32(data32.data(), data32.size());
  MutableBufferView mutable_image32(data32.data(), data32.size());

  AbsoluteAddress addr32(kBit32, 0x00320000U);
  EXPECT_TRUE(addr32.Read(0x0U, image32));
  EXPECT_EQ(0x00000000U, addr32.ToRva());
  EXPECT_TRUE(addr32.Read(0x4U, image32));
  EXPECT_EQ(0x4A334321U, addr32.ToRva());
  EXPECT_TRUE(addr32.Read(0x8U, image32));
  EXPECT_EQ(kInvalidRva, addr32.ToRva());  // Underflow.
  EXPECT_TRUE(addr32.Read(0xCU, image32));
  EXPECT_EQ(kInvalidRva, addr32.ToRva());  // Translated RVA would be too large.
  EXPECT_TRUE(addr32.Read(0x10U, image32));
  EXPECT_EQ(kInvalidRva, addr32.ToRva());  // Underflow (boundary case).

  EXPECT_FALSE(addr32.Read(0x11U, image32));
  EXPECT_FALSE(addr32.Read(0x14U, image32));
  EXPECT_FALSE(addr32.Read(0x100000U, image32));
  EXPECT_FALSE(addr32.Read(0x80000000U, image32));
  EXPECT_FALSE(addr32.Read(0xFFFFFFFFU, image32));

  EXPECT_TRUE(addr32.FromRva(0x11223344U));
  EXPECT_TRUE(addr32.Write(0x2U, &mutable_image32));
  EXPECT_TRUE(addr32.Write(0x10U, &mutable_image32));
  std::vector<uint8_t> expected_data32 = ParseHexString(
      "00 00  44 33 54 11  65 4A 00 00 00 00 FF FF FF FF  44 33 54 11");
  EXPECT_EQ(expected_data32, data32);
  EXPECT_FALSE(addr32.Write(0x11U, &mutable_image32));
  EXPECT_FALSE(addr32.Write(0xFFFFFFFFU, &mutable_image32));
  EXPECT_EQ(expected_data32, data32);
}

TEST(Abs32UtilsTest, AbsoluteAddress32Overflow) {
  AbsoluteAddress addr32(kBit32, 0xC0000000U);
  EXPECT_TRUE(addr32.FromRva(0x00000000U));
  EXPECT_TRUE(addr32.FromRva(0x11223344U));
  EXPECT_TRUE(addr32.FromRva(0x3FFFFFFFU));
  EXPECT_FALSE(addr32.FromRva(0x40000000U));
  EXPECT_FALSE(addr32.FromRva(0x40000001U));
  EXPECT_FALSE(addr32.FromRva(0x80000000U));
  EXPECT_FALSE(addr32.FromRva(0xFFFFFFFFU));

  EXPECT_EQ(0x00000000U, AddrValueToRva(0xC0000000U, &addr32));
  EXPECT_EQ(kInvalidRva, AddrValueToRva(0xBFFFFFFFU, &addr32));
  EXPECT_EQ(kInvalidRva, AddrValueToRva(0x00000000U, &addr32));
  EXPECT_EQ(0x3FFFFFFFU, AddrValueToRva(0xFFFFFFFFU, &addr32));
}

TEST(Abs32UtilsTest, AbsoluteAddress64) {
  std::vector<uint8_t> data64 = ParseHexString(
      "00 00 00 00 64 00 00 00  21 43 65 4A 64 00 00 00 "
      "00 00 00 00 00 00 00 00  FF FF FF FF FF FF FF FF "
      "00 00 00 00 64 00 00 80  FF FF FF FF 63 00 00 00");
  ConstBufferView image64(data64.data(), data64.size());
  MutableBufferView mutable_image64(data64.data(), data64.size());

  AbsoluteAddress addr64(kBit64, 0x0000006400000000ULL);
  EXPECT_TRUE(addr64.Read(0x0U, image64));
  EXPECT_EQ(0x00000000U, addr64.ToRva());
  EXPECT_TRUE(addr64.Read(0x8U, image64));
  EXPECT_EQ(0x4A654321U, addr64.ToRva());
  EXPECT_TRUE(addr64.Read(0x10U, image64));  // Succeeds, in spite of value.
  EXPECT_EQ(kInvalidRva, addr64.ToRva());    // Underflow.
  EXPECT_TRUE(addr64.Read(0x18U, image64));
  EXPECT_EQ(kInvalidRva, addr64.ToRva());  // Translated RVA too large.
  EXPECT_TRUE(addr64.Read(0x20U, image64));
  EXPECT_EQ(kInvalidRva, addr64.ToRva());  // Translated RVA toolarge.
  EXPECT_TRUE(addr64.Read(0x28U, image64));
  EXPECT_EQ(kInvalidRva, addr64.ToRva());  // Underflow.

  EXPECT_FALSE(addr64.Read(0x29U, image64));  // Extends outside.
  EXPECT_FALSE(addr64.Read(0x30U, image64));  // Entirely outside (note: hex).
  EXPECT_FALSE(addr64.Read(0x100000U, image64));
  EXPECT_FALSE(addr64.Read(0x80000000U, image64));
  EXPECT_FALSE(addr64.Read(0xFFFFFFFFU, image64));

  EXPECT_TRUE(addr64.FromRva(0x11223344U));
  EXPECT_TRUE(addr64.Write(0x13U, &mutable_image64));
  EXPECT_TRUE(addr64.Write(0x20U, &mutable_image64));
  std::vector<uint8_t> expected_data64 = ParseHexString(
      "00 00 00 00 64 00 00 00  21 43 65 4A 64 00 00 00 "
      "00 00 00 44 33 22 11 64  00 00 00 FF FF FF FF FF "
      "44 33 22 11 64 00 00 00  FF FF FF FF 63 00 00 00");
  EXPECT_EQ(expected_data64, data64);
  EXPECT_FALSE(addr64.Write(0x29U, &mutable_image64));
  EXPECT_FALSE(addr64.Write(0x30U, &mutable_image64));
  EXPECT_FALSE(addr64.Write(0xFFFFFFFFU, &mutable_image64));
  EXPECT_EQ(expected_data64, data64);

  EXPECT_FALSE(addr64.FromRva(0xFFFFFFFFU));
}

TEST(Abs32UtilsTest, AbsoluteAddress64Overflow) {
  {
    // Counterpart to AbsoluteAddress632verflow test.
    AbsoluteAddress addr64(kBit64, 0xFFFFFFFFC0000000ULL);
    EXPECT_TRUE(addr64.FromRva(0x00000000U));
    EXPECT_TRUE(addr64.FromRva(0x11223344U));
    EXPECT_TRUE(addr64.FromRva(0x3FFFFFFFU));
    EXPECT_FALSE(addr64.FromRva(0x40000000U));
    EXPECT_FALSE(addr64.FromRva(0x40000001U));
    EXPECT_FALSE(addr64.FromRva(0x80000000U));
    EXPECT_FALSE(addr64.FromRva(0xFFFFFFFFU));

    EXPECT_EQ(0x00000000U, AddrValueToRva(0xFFFFFFFFC0000000U, &addr64));
    EXPECT_EQ(kInvalidRva, AddrValueToRva(0xFFFFFFFFBFFFFFFFU, &addr64));
    EXPECT_EQ(kInvalidRva, AddrValueToRva(0x0000000000000000U, &addr64));
    EXPECT_EQ(kInvalidRva, AddrValueToRva(0xFFFFFFFF00000000U, &addr64));
    EXPECT_EQ(0x3FFFFFFFU, AddrValueToRva(0xFFFFFFFFFFFFFFFFU, &addr64));
  }
  {
    // Pseudo-counterpart to AbsoluteAddress632verflow test: Some now pass.
    AbsoluteAddress addr64(kBit64, 0xC0000000U);
    EXPECT_TRUE(addr64.FromRva(0x00000000U));
    EXPECT_TRUE(addr64.FromRva(0x11223344U));
    EXPECT_TRUE(addr64.FromRva(0x3FFFFFFFU));
    EXPECT_TRUE(addr64.FromRva(0x40000000U));
    EXPECT_TRUE(addr64.FromRva(0x40000001U));
    EXPECT_FALSE(addr64.FromRva(0x80000000U));
    EXPECT_FALSE(addr64.FromRva(0xFFFFFFFFU));

    // ToRva() still fail though.
    EXPECT_EQ(0x00000000U, AddrValueToRva(0xC0000000U, &addr64));
    EXPECT_EQ(kInvalidRva, AddrValueToRva(0xBFFFFFFFU, &addr64));
    EXPECT_EQ(kInvalidRva, AddrValueToRva(0x00000000U, &addr64));
    EXPECT_EQ(0x3FFFFFFFU, AddrValueToRva(0xFFFFFFFFU, &addr64));
  }
  {
    AbsoluteAddress addr64(kBit64, 0xC000000000000000ULL);
    EXPECT_TRUE(addr64.FromRva(0x00000000ULL));
    EXPECT_TRUE(addr64.FromRva(0x11223344ULL));
    EXPECT_TRUE(addr64.FromRva(0x3FFFFFFFULL));
    EXPECT_TRUE(addr64.FromRva(0x40000000ULL));
    EXPECT_TRUE(addr64.FromRva(0x40000001ULL));
    EXPECT_FALSE(addr64.FromRva(0x80000000ULL));
    EXPECT_FALSE(addr64.FromRva(0xFFFFFFFFULL));

    EXPECT_EQ(0x00000000U, AddrValueToRva(0xC000000000000000ULL, &addr64));
    EXPECT_EQ(kInvalidRva, AddrValueToRva(0xBFFFFFFFFFFFFFFFULL, &addr64));
    EXPECT_EQ(kInvalidRva, AddrValueToRva(0x0000000000000000ULL, &addr64));
    EXPECT_EQ(0x3FFFFFFFU, AddrValueToRva(0xC00000003FFFFFFFULL, &addr64));
    EXPECT_EQ(kInvalidRva, AddrValueToRva(0xFFFFFFFFFFFFFFFFULL, &addr64));
  }
}

TEST(Abs32UtilsTest, Win32Read32) {
  constexpr uint32_t kImageBase = 0xA0000000U;
  constexpr uint32_t kRvaBegin = 0x00C00000U;
  struct {
    std::vector<uint8_t> data32;
    std::deque<offset_t> abs32_locations;  // Assumption: Sorted.
    offset_t lo;  // Assumption: In range, does not straddle |abs32_location|.
    offset_t hi;  // Assumption: Also >= |lo|.
    std::vector<Reference> expected_refs;
  } test_cases[] = {
      // Targets at beginning and end.
      {ParseHexString("FF FF FF FF 0F 00 C0 A0 00 00 C0 A0 FF FF FF FF"),
       {0x4U, 0x8U},
       0x0U,
       0x10U,
       {{0x4U, 0xFU}, {0x8U, 0x0U}}},
      // Targets at beginning and end are out of bound: Rejected.
      {ParseHexString("FF FF FF FF 10 00 C0 A0 FF FF BF A0 FF FF FF FF"),
       {0x4U, 0x8U},
       0x0U,
       0x10U,
       std::vector<Reference>()},
      // Same with more extreme target values: Rejected.
      {ParseHexString("FF FF FF FF FF FF FF FF 00 00 00 00 FF FF FF FF"),
       {0x4U, 0x8U},
       0x0U,
       0x10U,
       std::vector<Reference>()},
      // Locations at beginning and end, plus invalid locations.
      {ParseHexString("08 00 C0 A0 FF FF FF FF FF FF FF FF 04 00 C0 A0"),
       {0x0U, 0xCU, 0x10U, 0x1000U, 0x80000000U, 0xFFFFFFFFU},
       0x0U,
       0x10U,
       {{0x0U, 0x8U}, {0xCU, 0x4U}}},
      // Odd size, location, target.
      {ParseHexString("FF FF FF 09 00 C0 A0 FF FF FF FF FF FF FF FF FF "
                      "FF FF FF"),
       {0x3U},
       0x0U,
       0x13U,
       {{0x3U, 0x9U}}},
      // No location given.
      {ParseHexString("FF FF FF FF 0C 00 C0 A0 00 00 C0 A0 FF FF FF FF"),
       std::deque<offset_t>(), 0x0U, 0x10U, std::vector<Reference>()},
      // Simple alternation.
      {ParseHexString("04 00 C0 A0 FF FF FF FF 0C 00 C0 A0 FF FF FF FF "
                      "14 00 C0 A0 FF FF FF FF 1C 00 C0 A0 FF FF FF FF"),
       {0x0U, 0x8U, 0x10U, 0x18U},
       0x0U,
       0x20U,
       {{0x0U, 0x4U}, {0x8U, 0xCU}, {0x10U, 0x14U}, {0x18U, 0x1CU}}},
      // Same, with locations limited by |lo| and |hi|. By assumption these must
      // not cut accross Reference body.
      {ParseHexString("04 00 C0 A0 FF FF FF FF 0C 00 C0 A0 FF FF FF FF "
                      "14 00 C0 A0 FF FF FF FF 1C 00 C0 A0 FF FF FF FF"),
       {0x0U, 0x8U, 0x10U, 0x18U},
       0x04U,
       0x17U,
       {{0x8U, 0xCU}, {0x10U, 0x14U}}},
      // Same, with very limiting |lo| and |hi|.
      {ParseHexString("04 00 C0 A0 FF FF FF FF 0C 00 C0 A0 FF FF FF FF "
                      "14 00 C0 A0 FF FF FF FF 1C 00 C0 A0 FF FF FF FF"),
       {0x0U, 0x8U, 0x10U, 0x18U},
       0x0CU,
       0x10U,
       std::vector<Reference>()},
      // Same, |lo| == |hi|.
      {ParseHexString("04 00 C0 A0 FF FF FF FF 0C 00 C0 A0 FF FF FF FF "
                      "14 00 C0 A0 FF FF FF FF 1C 00 C0 A0 FF FF FF FF"),
       {0x0U, 0x8U, 0x10U, 0x18U},
       0x14U,
       0x14U,
       std::vector<Reference>()},
      // Same, |lo| and |hi| at end.
      {ParseHexString("04 00 C0 A0 FF FF FF FF 0C 00 C0 A0 FF FF FF FF "
                      "14 00 C0 A0 FF FF FF FF 1C 00 C0 A0 FF FF FF FF"),
       {0x0U, 0x8U, 0x10U, 0x18U},
       0x20U,
       0x20U,
       std::vector<Reference>()},
      // Mix. Note that targets can overlap.
      {ParseHexString("FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF "
                      "06 00 C0 A0 2C 00 C0 A0 FF FF C0 A0 2B 00 C0 A0 "
                      "FF 06 00 C0 A0 00 00 C0 A0 FF FF FF FF FF FF FF"),
       {0x10U, 0x14U, 0x18U, 0x1CU, 0x21U, 0x25U, 0xAAAAU},
       0x07U,
       0x25U,
       {{0x10U, 0x6U}, {0x14U, 0x2CU}, {0x1CU, 0x2BU}, {0x21, 0x6U}}},
  };

  for (const auto& test_case : test_cases) {
    ConstBufferView image32(test_case.data32.data(), test_case.data32.size());
    Abs32RvaExtractorWin32 extractor(image32, {kBit32, kImageBase},
                                     test_case.abs32_locations, test_case.lo,
                                     test_case.hi);

    TestAddressTranslator translator(test_case.data32.size(), kRvaBegin);
    Abs32ReaderWin32 reader(std::move(extractor), translator);

    // Loop over |expected_ref| to check element-by-element.
    std::optional<Reference> ref;
    for (const auto& expected_ref : test_case.expected_refs) {
      ref = reader.GetNext();
      EXPECT_TRUE(ref.has_value());
      EXPECT_EQ(expected_ref, ref.value());
    }
    // Check that nothing is left.
    ref = reader.GetNext();
    EXPECT_FALSE(ref.has_value());
  }
}

TEST(Abs32UtilsTest, Win32Read64) {
  constexpr uint64_t kImageBase = 0x31415926A0000000U;
  constexpr uint32_t kRvaBegin = 0x00C00000U;
  // For simplicity, just test mixed case.
  std::vector<uint8_t> data64 = ParseHexString(
      "FF FF FF FF FF FF FF FF 00 00 C0 A0 26 59 41 31 "
      "06 00 C0 A0 26 59 41 31 02 00 C0 A0 26 59 41 31 "
      "FF FF FF BF 26 59 41 31 FF FF FF FF FF FF FF FF "
      "02 00 C0 A0 26 59 41 31 07 00 C0 A0 26 59 41 31");
  std::deque<offset_t> abs32_locations = {0x8U,  0x10U, 0x18U, 0x20U,
                                          0x28U, 0x30U, 0x38U, 0x40U};
  offset_t lo = 0x10U;
  offset_t hi = 0x38U;
  std::vector<Reference> expected_refs = {
      {0x10U, 0x06U}, {0x18U, 0x02U}, {0x30U, 0x02U}};

  ConstBufferView image64(data64.data(), data64.size());
  Abs32RvaExtractorWin32 extractor(image64, {kBit64, kImageBase},
                                   abs32_locations, lo, hi);
  TestAddressTranslator translator(data64.size(), kRvaBegin);
  Abs32ReaderWin32 reader(std::move(extractor), translator);

  std::vector<Reference> refs;
  std::optional<Reference> ref;
  for (ref = reader.GetNext(); ref.has_value(); ref = reader.GetNext())
    refs.push_back(ref.value());
  EXPECT_EQ(expected_refs, refs);
}

TEST(Abs32UtilsTest, Win32ReadFail) {
  // Make |bitness| a state to reduce repetition.
  Bitness bitness = kBit32;

  constexpr uint32_t kImageBase = 0xA0000000U;  // Shared for 32-bit and 64-bit.
  std::vector<uint8_t> data(32U, 0xFFU);
  ConstBufferView image(data.data(), data.size());

  auto try_make = [&](std::deque<offset_t>&& abs32_locations, offset_t lo,
                      offset_t hi) {
    Abs32RvaExtractorWin32 extractor(image, {bitness, kImageBase},
                                     abs32_locations, lo, hi);
    extractor.GetNext();  // Dummy call so |extractor| gets used.
  };

  // 32-bit tests.
  bitness = kBit32;
  try_make({8U, 24U}, 0U, 32U);
#if GTEST_HAS_DEATH_TEST
  EXPECT_DEATH(try_make({4U, 24U}, 32U, 0U), "");  // |lo| > |hi|.
#endif
  try_make({8U, 24U}, 0U, 12U);
  try_make({8U, 24U}, 0U, 28U);
  try_make({8U, 24U}, 8U, 32U);
  try_make({8U, 24U}, 24U, 32U);
#if GTEST_HAS_DEATH_TEST
  EXPECT_DEATH(try_make({8U, 24U}, 0U, 11U), "");   // |hi| straddles.
  EXPECT_DEATH(try_make({8U, 24U}, 26U, 32U), "");  // |lo| straddles.
#endif
  try_make({8U, 24U}, 12U, 24U);

  // 64-bit tests.
  bitness = kBit64;
  try_make({6U, 22U}, 0U, 32U);
#if GTEST_HAS_DEATH_TEST
  // |lo| > |hi|.
  EXPECT_DEATH(try_make(std::deque<offset_t>(), 32U, 31U), "");
#endif
  try_make({6U, 22U}, 0U, 14U);
  try_make({6U, 22U}, 0U, 30U);
  try_make({6U, 22U}, 6U, 32U);
  try_make({6U, 22U}, 22U, 32U);
#if GTEST_HAS_DEATH_TEST
  EXPECT_DEATH(try_make({6U, 22U}, 0U, 29U), "");  // |hi| straddles.
  EXPECT_DEATH(try_make({6U, 22U}, 7U, 32U), "");  // |lo| straddles.
#endif
  try_make({6U, 22U}, 14U, 20U);
  try_make({16U}, 16U, 24U);
#if GTEST_HAS_DEATH_TEST
  EXPECT_DEATH(try_make({16U}, 18U, 18U), "");  // |lo|, |hi| straddle.
#endif
}

TEST(Abs32UtilsTest, Win32Write32) {
  constexpr uint32_t kImageBase = 0xA0000000U;
  constexpr uint32_t kRvaBegin = 0x00C00000U;
  std::vector<uint8_t> data32(0x30, 0xFFU);
  MutableBufferView image32(data32.data(), data32.size());
  AbsoluteAddress addr(kBit32, kImageBase);
  TestAddressTranslator translator(data32.size(), kRvaBegin);
  Abs32WriterWin32 writer(image32, std::move(addr), translator);

  // Successful writes.
  writer.PutNext({0x02U, 0x10U});
  writer.PutNext({0x0BU, 0x21U});
  writer.PutNext({0x16U, 0x10U});
  writer.PutNext({0x2CU, 0x00U});

  // Invalid data: For simplicity, Abs32WriterWin32 simply ignores bad writes.
  // Invalid location.
  writer.PutNext({0x2DU, 0x20U});
  writer.PutNext({0x80000000U, 0x20U});
  writer.PutNext({0xFFFFFFFFU, 0x20U});
  // Invalid target.
  writer.PutNext({0x1CU, 0x00001111U});
  writer.PutNext({0x10U, 0xFFFFFF00U});

  std::vector<uint8_t> expected_data32 = ParseHexString(
      "FF FF 10 00 C0 A0 FF FF FF FF FF 21 00 C0 A0 FF "
      "FF FF FF FF FF FF 10 00 C0 A0 FF FF FF FF FF FF "
      "FF FF FF FF FF FF FF FF FF FF FF FF 00 00 C0 A0");
  EXPECT_EQ(expected_data32, data32);
}

TEST(Abs32UtilsTest, Win32Write64) {
  constexpr uint64_t kImageBase = 0x31415926A0000000U;
  constexpr uint32_t kRvaBegin = 0x00C00000U;
  std::vector<uint8_t> data64(0x30, 0xFFU);
  MutableBufferView image32(data64.data(), data64.size());
  AbsoluteAddress addr(kBit64, kImageBase);
  TestAddressTranslator translator(data64.size(), kRvaBegin);
  Abs32WriterWin32 writer(image32, std::move(addr), translator);

  // Successful writes.
  writer.PutNext({0x02U, 0x10U});
  writer.PutNext({0x0BU, 0x21U});
  writer.PutNext({0x16U, 0x10U});
  writer.PutNext({0x28U, 0x00U});

  // Invalid data: For simplicity, Abs32WriterWin32 simply ignores bad writes.
  // Invalid location.
  writer.PutNext({0x29U, 0x20U});
  writer.PutNext({0x80000000U, 0x20U});
  writer.PutNext({0xFFFFFFFFU, 0x20U});
  // Invalid target.
  writer.PutNext({0x1CU, 0x00001111U});
  writer.PutNext({0x10U, 0xFFFFFF00U});

  std::vector<uint8_t> expected_data64 = ParseHexString(
      "FF FF 10 00 C0 A0 26 59 41 31 FF 21 00 C0 A0 26 "
      "59 41 31 FF FF FF 10 00 C0 A0 26 59 41 31 FF FF "
      "FF FF FF FF FF FF FF FF 00 00 C0 A0 26 59 41 31");
  EXPECT_EQ(expected_data64, data64);
}

TEST(Abs32UtilsTest, RemoveUntranslatableAbs32) {
  Bitness kBitness = kBit32;
  uint64_t kImageBase = 0x2BCD0000;

  // Valid RVAs: [0x00001A00, 0x00001A28) and [0x00003A00, 0x00004000).
  // Valid AVAs: [0x2BCD1A00, 0x2BCD1A28) and [0x2BCD3A00, 0x2BCD4000).
  // Notice that the second section has has dangling RVA.
  AddressTranslator translator;
  ASSERT_EQ(AddressTranslator::kSuccess,
            translator.Initialize(
                {{0x04, +0x28, 0x1A00, +0x28}, {0x30, +0x30, 0x3A00, +0x600}}));

  std::vector<uint8_t> data = ParseHexString(
      "FF FF FF FF  0B 3A CD 2B  00 00 00  04 3A CD 2B  00 "
      "FC 3F CD 2B  14 1A CD 2B  44 00 00 00  CC 00 00 00 "
      "00 00 55 00  00 00  1E 1A CD 2B  00 99  FF FF FF FF "
      "10 3A CD 2B  22 00 00 00  00 00 00 11  00 00 00 00 "
      "66 00 00 00  28 1A CD 2B  00 00 CD 2B  27 1A CD 2B "
      "FF 39 CD 2B  00 00 00 00  18 1A CD 2B  00 00 00 00 "
      "FF FF FF FF  FF FF FF FF");
  MutableBufferView image(data.data(), data.size());

  const offset_t kAbs1 = 0x04;  // a:2BCD3A0B = r:3A0B = o:3B
  const offset_t kAbs2 = 0x0B;  // a:2BCD3A04 = r:3A04 = o:34
  const offset_t kAbs3 = 0x10;  // a:2BCD3FFF = r:3FFF (dangling)
  const offset_t kAbs4 = 0x14;  // a:2BCD1A14 = r:1A14 = o:18
  const offset_t kAbs5 = 0x26;  // a:2BCD1A1E = r:1A1E = o:22
  const offset_t kAbs6 = 0x30;  // a:2BCD3A10 = r:3A10 = 0x40
  const offset_t kAbs7 = 0x44;  // a:2BCD1A28 = r:1A28 (bad: sentinel)
  const offset_t kAbs8 = 0x48;  // a:2BCD0000 = r:0000 (bad: not covered)
  const offset_t kAbs9 = 0x4C;  // a:2BCD1A27 = r:1A27 = 0x2B
  const offset_t kAbsA = 0x50;  // a:2BCD39FF (bad: not covered)
  const offset_t kAbsB = 0x54;  // a:00000000 (bad: underflow)
  const offset_t kAbsC = 0x58;  // a:2BCD1A18 = r:1A18 = 0x1C

  std::deque<offset_t> locations = {kAbs1, kAbs2, kAbs3, kAbs4, kAbs5, kAbs6,
                                    kAbs7, kAbs8, kAbs9, kAbsA, kAbsB, kAbsC};
  std::deque<offset_t> exp_locations = {kAbs1, kAbs2, kAbs3, kAbs4,
                                        kAbs5, kAbs6, kAbs9, kAbsC};
  size_t exp_num_removed = locations.size() - exp_locations.size();
  size_t num_removed = RemoveUntranslatableAbs32(image, {kBitness, kImageBase},
                                                 translator, &locations);
  EXPECT_EQ(exp_num_removed, num_removed);
  EXPECT_EQ(exp_locations, locations);
}

TEST(Abs32UtilsTest, RemoveOverlappingAbs32Locations) {
  // Make |width| a state to reduce repetition.
  uint32_t width = WidthOf(kBit32);

  auto run_test = [&width](const std::deque<offset_t>& expected_locations,
                           std::deque<offset_t>&& locations) {
    ASSERT_TRUE(std::is_sorted(locations.begin(), locations.end()));
    size_t expected_removals = locations.size() - expected_locations.size();
    size_t removals = RemoveOverlappingAbs32Locations(width, &locations);
    EXPECT_EQ(expected_removals, removals);
    EXPECT_EQ(expected_locations, locations);
  };

  // 32-bit tests.
  width = WidthOf(kBit32);
  run_test(std::deque<offset_t>(), std::deque<offset_t>());
  run_test({4U}, {4U});
  run_test({4U, 10U}, {4U, 10U});
  run_test({4U, 8U}, {4U, 8U});
  run_test({4U}, {4U, 7U});
  run_test({4U}, {4U, 4U});
  run_test({4U, 8U}, {4U, 7U, 8U});
  run_test({4U, 10U}, {4U, 7U, 10U});
  run_test({4U, 9U}, {4U, 9U, 10U});
  run_test({3U}, {3U, 5U, 6U});
  run_test({3U, 7U}, {3U, 4U, 5U, 6U, 7U, 8U, 9U, 10U});
  run_test({3U, 7U, 11U}, {3U, 4U, 5U, 6U, 7U, 8U, 9U, 10U, 11U, 12U});
  run_test({4U, 8U, 12U}, {4U, 6U, 8U, 10U, 12U});
  run_test({4U, 8U, 12U, 16U}, {4U, 8U, 12U, 16U});
  run_test({4U, 8U, 12U}, {4U, 8U, 9U, 12U});
  run_test({4U}, {4U, 4U, 4U, 4U, 4U, 4U});
  run_test({3U}, {3U, 4U, 4U, 4U, 5U, 5U});
  run_test({3U, 7U}, {3U, 4U, 4U, 4U, 7U, 7U, 8U});
  run_test({10U, 20U, 30U, 40U}, {10U, 20U, 22U, 22U, 30U, 40U});
  run_test({1000000U, 1000004U}, {1000000U, 1000004U});
  run_test({1000000U}, {1000000U, 1000002U});

  // 64-bit tests.
  width = WidthOf(kBit64);
  run_test(std::deque<offset_t>(), std::deque<offset_t>());
  run_test({4U}, {4U});
  run_test({4U, 20U}, {4U, 20U});
  run_test({4U, 12U}, {4U, 12U});
  run_test({4U}, {4U, 11U});
  run_test({4U}, {4U, 5U});
  run_test({4U}, {4U, 4U});
  run_test({4U, 12U, 20U}, {4U, 12U, 20U});
  run_test({1U, 9U, 17U}, {1U, 9U, 17U});
  run_test({1U, 17U}, {1U, 8U, 17U});
  run_test({1U, 10U}, {1U, 10U, 17U});
  run_test({3U, 11U}, {3U, 4U, 5U, 6U, 7U, 8U, 9U, 10U, 11U, 12U});
  run_test({4U, 12U}, {4U, 6U, 8U, 10U, 12U});
  run_test({4U, 12U}, {4U, 12U, 16U});
  run_test({4U, 12U, 20U, 28U}, {4U, 12U, 20U, 28U});
  run_test({4U}, {4U, 4U, 4U, 4U, 5U, 5U});
  run_test({3U, 11U}, {3U, 4U, 4U, 4U, 11U, 11U, 12U});
  run_test({10U, 20U, 30U, 40U}, {10U, 20U, 22U, 22U, 30U, 40U});
  run_test({1000000U, 1000008U}, {1000000U, 1000008U});
  run_test({1000000U}, {1000000U, 1000004U});
}

}  // namespace zucchini
