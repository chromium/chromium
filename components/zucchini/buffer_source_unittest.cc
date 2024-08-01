// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/zucchini/buffer_source.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <iterator>
#include <string>
#include <tuple>
#include <vector>

#include "components/zucchini/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace zucchini {

using vec = std::vector<uint8_t>;

class BufferSourceTest : public testing::Test {
 protected:
  std::vector<uint8_t> bytes_ = ParseHexString("10 32 54 76 98 BA DC FE 10 00");

  BufferSource source_ = {bytes_.data(), bytes_.size()};
};

TEST_F(BufferSourceTest, CtorWithOffset) {
  ConstBufferView view(bytes_.data(), bytes_.size());
  for (size_t offset = 0; offset < bytes_.size() * 2; ++offset) {
    BufferSource source(view, offset);
    size_t expected_remaining = bytes_.size() - std::min(bytes_.size(), offset);
    EXPECT_EQ(expected_remaining, source.Remaining());
  }

  BufferSource source1(view, 5);
  EXPECT_TRUE(source1.CheckNextBytes({0xBA, 0xDC, 0xFE, 0x10}));
  BufferSource source2(view, 0);
  EXPECT_TRUE(source2.CheckNextBytes({0x10, 0x32, 0x54, 0x76}));
  BufferSource source3(view, 9);
  EXPECT_TRUE(source3.CheckNextBytes({0x00}));
}

TEST_F(BufferSourceTest, Skip) {
  EXPECT_EQ(bytes_.size(), source_.Remaining());
  EXPECT_TRUE(source_.Skip(2));
  EXPECT_EQ(bytes_.size() - 2, source_.Remaining());
  EXPECT_TRUE(source_.Skip(bytes_.size() - 2));  // Skip to end.
  EXPECT_EQ(size_t(0), source_.Remaining());
  EXPECT_FALSE(source_.Skip(1));
  EXPECT_EQ(size_t(0), source_.Remaining());
  EXPECT_FALSE(source_.Skip(4));
  EXPECT_EQ(size_t(0), source_.Remaining());
}

TEST_F(BufferSourceTest, SkipAcrossEnd) {
  EXPECT_EQ(bytes_.size(), source_.Remaining());
  EXPECT_TRUE(source_.Skip(2));
  EXPECT_EQ(bytes_.size() - 2, source_.Remaining());
  EXPECT_TRUE(source_.Skip(5));
  EXPECT_EQ(bytes_.size() - 7, source_.Remaining());
  EXPECT_FALSE(source_.Skip(10));  // Skip past end.
  EXPECT_EQ(size_t(0), source_.Remaining());
  EXPECT_FALSE(source_.Skip(1));
  EXPECT_EQ(size_t(0), source_.Remaining());
  EXPECT_FALSE(source_.Skip(4));
  EXPECT_EQ(size_t(0), source_.Remaining());
}

TEST_F(BufferSourceTest, CheckNextBytes) {
  EXPECT_TRUE(source_.CheckNextBytes({0x10, 0x32, 0x54, 0x76}));
  EXPECT_TRUE(source_.Skip(4));
  EXPECT_TRUE(source_.CheckNextBytes({0x98, 0xBA, 0xDC, 0xFE}));

  // Cursor has not advanced, so check fails.
  EXPECT_FALSE(source_.CheckNextBytes({0x10, 0x00}));

  EXPECT_TRUE(source_.Skip(4));
  EXPECT_EQ(size_t(2), source_.Remaining());

  // Goes beyond end by 2 bytes.
  EXPECT_FALSE(source_.CheckNextBytes({0x10, 0x00, 0x00, 0x00}));
  EXPECT_EQ(size_t(2), source_.Remaining());
}

TEST_F(BufferSourceTest, ConsumeBytes) {
  EXPECT_FALSE(source_.ConsumeBytes({0x10, 0x00}));
  EXPECT_EQ(bytes_.size(), source_.Remaining());
  EXPECT_TRUE(source_.ConsumeBytes({0x10, 0x32, 0x54, 0x76}));
  EXPECT_EQ(size_t(6), source_.Remaining());
  EXPECT_TRUE(source_.ConsumeBytes({0x98, 0xBA, 0xDC, 0xFE}));
  EXPECT_EQ(size_t(2), source_.Remaining());

  // Goes beyond end by 2 bytes.
  EXPECT_FALSE(source_.ConsumeBytes({0x10, 0x00, 0x00, 0x00}));
  EXPECT_EQ(size_t(2), source_.Remaining());
}

TEST_F(BufferSourceTest, CheckNextValue) {
  EXPECT_TRUE(source_.CheckNextValue(uint32_t(0x76543210)));
  EXPECT_FALSE(source_.CheckNextValue(uint32_t(0x0)));
  EXPECT_TRUE(source_.CheckNextValue(uint64_t(0xFEDCBA9876543210)));
  EXPECT_FALSE(source_.CheckNextValue(uint64_t(0x0)));

  EXPECT_TRUE(source_.Skip(8));
  EXPECT_EQ(size_t(2), source_.Remaining());

  // Goes beyond end by 2 bytes.
  EXPECT_FALSE(source_.CheckNextValue(uint32_t(0x1000)));
}

// Supported by MSVC, g++, and clang++.
// Ensures unaligned data access and no gaps in packing.
#pragma pack(push, 1)

// Trivial wrapper for uint32_t, to ensure data access is unaligned.
struct UnalignedUint32T {
  uint32_t value;
};

struct ValueType {
  uint32_t a;
  uint16_t b;
};
#pragma pack(pop)

TEST_F(BufferSourceTest, GetValueIntegral) {
  uint32_t value = 0;
  EXPECT_TRUE(source_.GetValue(&value));
  EXPECT_EQ(uint32_t(0x76543210), value);
  EXPECT_EQ(size_t(6), source_.Remaining());

  EXPECT_TRUE(source_.GetValue(&value));
  EXPECT_EQ(uint32_t(0xFEDCBA98), value);
  EXPECT_EQ(size_t(2), source_.Remaining());

  EXPECT_FALSE(source_.GetValue(&value));
  EXPECT_EQ(size_t(2), source_.Remaining());
}

TEST_F(BufferSourceTest, GetValueAggregate) {
  ValueType value = {};
  EXPECT_TRUE(source_.GetValue(&value));
  EXPECT_EQ(uint32_t(0x76543210), value.a);
  EXPECT_EQ(uint32_t(0xBA98), value.b);
  EXPECT_EQ(size_t(4), source_.Remaining());
}

TEST_F(BufferSourceTest, GetValueUnaligned) {
  uint8_t v8 = 0U;
  EXPECT_TRUE(source_.GetValue(&v8));
  EXPECT_EQ(0x10U, v8);
  uint16_t v16 = 0U;
  EXPECT_TRUE(source_.GetValue(&v16));
  EXPECT_EQ(0x5432U, v16);
  uint32_t v32 = 0U;
  EXPECT_TRUE(source_.GetValue(&v32));
  EXPECT_EQ(0xDCBA9876U, v32);
}

TEST_F(BufferSourceTest, GetRegion) {
  ConstBufferView region;
  EXPECT_TRUE(source_.GetRegion(0, &region));
  EXPECT_EQ(bytes_.size(), source_.Remaining());
  EXPECT_TRUE(region.empty());

  EXPECT_TRUE(source_.GetRegion(2, &region));
  EXPECT_EQ(size_t(2), region.size());
  EXPECT_EQ(vec({0x10, 0x32}), vec(region.begin(), region.end()));
  EXPECT_EQ(size_t(8), source_.Remaining());

  EXPECT_FALSE(source_.GetRegion(bytes_.size(), &region));
  EXPECT_EQ(size_t(8), source_.Remaining());
  // |region| is left untouched.
  EXPECT_EQ(vec({0x10, 0x32}), vec(region.begin(), region.end()));
  EXPECT_EQ(size_t(2), region.size());
}

TEST_F(BufferSourceTest, GetPointerIntegral) {
  const UnalignedUint32T* ptr = source_.GetPointer<UnalignedUint32T>();
  EXPECT_NE(nullptr, ptr);
  EXPECT_EQ(uint32_t(0x76543210), ptr->value);
  EXPECT_EQ(size_t(6), source_.Remaining());

  ptr = source_.GetPointer<UnalignedUint32T>();
  EXPECT_NE(nullptr, ptr);
  EXPECT_EQ(uint32_t(0xFEDCBA98), ptr->value);
  EXPECT_EQ(size_t(2), source_.Remaining());

  EXPECT_EQ(nullptr, source_.GetPointer<UnalignedUint32T>());
  EXPECT_EQ(size_t(2), source_.Remaining());
}

TEST_F(BufferSourceTest, GetPointerIntegralMisaligned) {
  source_.Skip(1);
  EXPECT_EQ(size_t(9), source_.Remaining());
  const UnalignedUint32T* ptr = source_.GetPointer<UnalignedUint32T>();
  EXPECT_NE(nullptr, ptr);
  EXPECT_EQ(uint32_t(0x98765432), ptr->value);
  EXPECT_EQ(size_t(5), source_.Remaining());

  source_.Skip(1);
  EXPECT_EQ(size_t(4), source_.Remaining());
  ptr = source_.GetPointer<UnalignedUint32T>();
  EXPECT_NE(nullptr, ptr);
  EXPECT_EQ(uint32_t(0x0010FEDC), ptr->value);
  EXPECT_EQ(size_t(0), source_.Remaining());
}

TEST_F(BufferSourceTest, GetPointerAggregate) {
  const ValueType* ptr = source_.GetPointer<ValueType>();
  EXPECT_NE(nullptr, ptr);
  EXPECT_EQ(uint32_t(0x76543210), ptr->a);
  EXPECT_EQ(uint32_t(0xBA98), ptr->b);
  EXPECT_EQ(size_t(4), source_.Remaining());
}

TEST_F(BufferSourceTest, GetArrayIntegral) {
  EXPECT_EQ(nullptr, source_.GetArray<UnalignedUint32T>(3));

  const UnalignedUint32T* ptr = source_.GetArray<UnalignedUint32T>(2);
  EXPECT_NE(nullptr, ptr);
  EXPECT_EQ(uint32_t(0x76543210), ptr[0].value);
  EXPECT_EQ(uint32_t(0xFEDCBA98), ptr[1].value);
  EXPECT_EQ(size_t(2), source_.Remaining());
}

TEST_F(BufferSourceTest, GetArrayIntegralMisaligned) {
  source_.Skip(1);
  EXPECT_EQ(nullptr, source_.GetArray<UnalignedUint32T>(3));

  const UnalignedUint32T* ptr = source_.GetArray<UnalignedUint32T>(2);
  EXPECT_NE(nullptr, ptr);
  EXPECT_EQ(uint32_t(0x98765432), ptr[0].value);
  EXPECT_EQ(uint32_t(0x10FEDCBA), ptr[1].value);
  EXPECT_EQ(size_t(1), source_.Remaining());
}

TEST_F(BufferSourceTest, GetArrayAggregate) {
  const ValueType* ptr = source_.GetArray<ValueType>(2);
  EXPECT_EQ(nullptr, ptr);

  ptr = source_.GetArray<ValueType>(1);

  EXPECT_NE(nullptr, ptr);
  EXPECT_EQ(uint32_t(0x76543210), ptr[0].a);
  EXPECT_EQ(uint32_t(0xBA98), ptr[0].b);
  EXPECT_EQ(size_t(4), source_.Remaining());
}

TEST_F(BufferSourceTest, GetUleb128) {
  using size_type = BufferSource::size_type;
  // Result = {success, value, bytes_consumed}.
  using Result = std::tuple<bool, uint32_t, size_type>;

  constexpr uint32_t kUnInit = 0xCCCCCCCC;  // Arbitrary value.
  constexpr Result kBad{false, kUnInit, 0U};

  auto run = [](const std::string hex_string) -> Result {
    std::vector<uint8_t> bytes = ParseHexString(hex_string);
    BufferSource source(ConstBufferView{bytes.data(), bytes.size()});
    BufferSource::iterator base = source.begin();
    // Initialize |value| to |kUnInit| to ensure no write on failure.
    uint32_t value = kUnInit;
    bool success = source.GetUleb128(&value);
    return {success, value, source.begin() - base};
  };

  auto good = [](uint32_t value, size_type bytes_consumed) -> Result {
    return Result{true, value, bytes_consumed};
  };

  EXPECT_EQ(good(0x0U, 1U), run("00"));
  EXPECT_EQ(good(0x20U, 1U), run("20"));
  EXPECT_EQ(good(0x42U, 1U), run("42"));
  EXPECT_EQ(good(0x7FU, 1U), run("7F"));
  EXPECT_EQ(kBad, run("80"));               // Out of data.
  EXPECT_EQ(good(0x0U, 2U), run("80 00"));  // Redundant code.
  EXPECT_EQ(good(0x80U, 2U), run("80 01"));
  EXPECT_EQ(good(0x7FU, 2U), run("FF 00"));  // Redundant (unsigned).
  EXPECT_EQ(good(0x3FFFU, 2U), run("FF 7F"));
  EXPECT_EQ(good(0x0U, 1U), run("00 80"));     // Only reads byte 0.
  EXPECT_EQ(kBad, run("80 80"));               // Out of data.
  EXPECT_EQ(kBad, run("F1 88"));               // Out of data.
  EXPECT_EQ(good(0x0U, 3U), run("80 80 00"));  // Redundant code.
  EXPECT_EQ(good(0x4000U, 3U), run("80 80 01"));
  EXPECT_EQ(good(0x00100000U, 3U), run("80 80 40"));
  EXPECT_EQ(good(0x001FFFFFU, 3U), run("FF FF 7F"));
  EXPECT_EQ(good(0x0U, 1U), run("00 00 80"));     // Only reads byte 0.
  EXPECT_EQ(kBad, run("80 80 80"));               // Out of data.
  EXPECT_EQ(kBad, run("AB CD EF"));               // Out of data.
  EXPECT_EQ(good(0x0U, 4U), run("80 80 80 00"));  // Redundant code.
  EXPECT_EQ(good(0x00100000U, 4U), run("80 80 C0 00"));
  EXPECT_EQ(good(0x00200000U, 4U), run("80 80 80 01"));
  EXPECT_EQ(good(0x08000000U, 4U), run("80 80 80 40"));
  EXPECT_EQ(good(0x001FC07FU, 4U), run("FF 80 FF 00"));
  EXPECT_EQ(good(0x0U, 5U), run("80 80 80 80 00"));  // Redundant code.
  EXPECT_EQ(good(0x10000000U, 5U), run("80 80 80 80 01"));
  EXPECT_EQ(good(0x10204081U, 5U), run("81 81 81 81 01"));
  EXPECT_EQ(good(0x7FFFFFFFU, 5U), run("FF FF FF FF 07"));
  EXPECT_EQ(good(0x80000000U, 5U), run("80 80 80 80 08"));
  EXPECT_EQ(good(0xFFFFFFFFU, 5U), run("FF FF FF FF 0F"));
  EXPECT_EQ(kBad, run("FF FF FF FF 80"));  // Too long / out of data.
  EXPECT_EQ(good(0x0FFFFFFFU, 5U), run("FF FF FF FF 10"));  // "1" discarded.
  EXPECT_EQ(good(0x00000000U, 5U), run("80 80 80 80 20"));  // "2" discarded.
  EXPECT_EQ(good(0xA54A952AU, 5U), run("AA AA AA AA 7A"));  // "7" discarded.
  EXPECT_EQ(kBad, run("FF FF FF FF FF 00"));                // Too long.
}

TEST_F(BufferSourceTest, GetSleb128) {
  using size_type = BufferSource::size_type;
  // Result = {success, value, bytes_consumed}.
  using Result = std::tuple<bool, int32_t, size_type>;

  constexpr int32_t kUnInit = 0xCCCCCCCC;  // Arbitrary value.
  constexpr Result kBad{false, kUnInit, 0U};

  auto run = [](const std::string hex_string) -> Result {
    std::vector<uint8_t> bytes = ParseHexString(hex_string);
    BufferSource source(ConstBufferView{bytes.data(), bytes.size()});
    BufferSource::iterator base = source.begin();
    // Initialize |value| to |kUnInit| to ensure no write on failure.
    int32_t value = kUnInit;
    bool success = source.GetSleb128(&value);
    return {success, value, source.begin() - base};
  };

  auto good = [](int32_t value, size_type bytes_consumed) -> Result {
    return Result{true, value, bytes_consumed};
  };

  EXPECT_EQ(good(0x0, 1U), run("00"));
  EXPECT_EQ(good(0x20U, 1U), run("20"));
  EXPECT_EQ(good(-0x3E, 1U), run("42"));
  EXPECT_EQ(good(-0x1, 1U), run("7F"));
  EXPECT_EQ(kBad, run("80"));              // Out of data.
  EXPECT_EQ(good(0x0, 2U), run("80 00"));  // Redundant code.
  EXPECT_EQ(good(0x80, 2U), run("80 01"));
  EXPECT_EQ(good(0x7F, 2U), run("FF 00"));    // Not redudnant.
  EXPECT_EQ(good(-0x1, 2U), run("FF 7F"));    // Redundant code.
  EXPECT_EQ(good(0x0, 1U), run("00 80"));     // Only reads byte 0.
  EXPECT_EQ(kBad, run("80 80"));              // Out of data.
  EXPECT_EQ(kBad, run("F1 88"));              // Out of data.
  EXPECT_EQ(good(0x0, 3U), run("80 80 00"));  // Redundant code.
  EXPECT_EQ(good(0x4000, 3U), run("80 80 01"));
  EXPECT_EQ(good(-0x100000, 3U), run("80 80 40"));
  EXPECT_EQ(good(-0x1, 3U), run("FF FF 7F"));    // Redundant code.
  EXPECT_EQ(good(0x0, 1U), run("00 00 80"));     // Only reads byte 0.
  EXPECT_EQ(kBad, run("80 80 80"));              // Out of data.
  EXPECT_EQ(kBad, run("AB CD EF"));              // Out of data.
  EXPECT_EQ(good(0x0, 4U), run("80 80 80 00"));  // Redundant code.
  EXPECT_EQ(good(0x00100000, 4U), run("80 80 C0 00"));
  EXPECT_EQ(good(0x00200000, 4U), run("80 80 80 01"));
  EXPECT_EQ(good(-static_cast<int32_t>(0x08000000), 4U), run("80 80 80 40"));
  EXPECT_EQ(good(0x001FC07F, 4U), run("FF 80 FF 00"));
  EXPECT_EQ(good(0x0, 5U), run("80 80 80 80 00"));  // Redundant code.
  EXPECT_EQ(good(0x10000000, 5U), run("80 80 80 80 01"));
  EXPECT_EQ(good(0x10204081, 5U), run("81 81 81 81 01"));
  EXPECT_EQ(good(0x7FFFFFFF, 5U), run("FF FF FF FF 07"));
  // Signed 0x80000000 is already negative.
  EXPECT_EQ(good(static_cast<int32_t>(0x80000000), 5U), run("80 80 80 80 08"));
  EXPECT_EQ(good(-0x1, 5U), run("FF FF FF FF 0F"));  // Redundant code.
  EXPECT_EQ(kBad, run("FF FF FF FF 80"));            // Too long / out of data.
  EXPECT_EQ(good(0x0FFFFFFF, 5U), run("FF FF FF FF 10"));   // "1" discarded.
  EXPECT_EQ(good(0x00000000, 5U), run("80 80 80 80 20"));   // "2" discarded.
  EXPECT_EQ(good(-0x5AB56AD6, 5U), run("AA AA AA AA 7A"));  // "7" discarded.
  EXPECT_EQ(kBad, run("FF FF FF FF FF 00"));                // Too long.
}

TEST_F(BufferSourceTest, SkipLeb128) {
  using size_type = BufferSource::size_type;
  // Result = {success, value, bytes_consumed}.
  using Result = std::tuple<bool, size_type>;

  constexpr Result kBad{false, 0U};

  auto run = [](const std::string hex_string) -> Result {
    std::vector<uint8_t> bytes = ParseHexString(hex_string);
    BufferSource source(ConstBufferView{bytes.data(), bytes.size()});
    BufferSource::iterator base = source.begin();
    bool success = source.SkipLeb128();
    return {success, source.begin() - base};
  };

  auto good = [](size_type bytes_consumed) -> Result {
    return Result{true, bytes_consumed};
  };

  EXPECT_EQ(good(1U), run("00"));
  EXPECT_EQ(good(1U), run("20"));
  EXPECT_EQ(good(1U), run("42"));
  EXPECT_EQ(good(1U), run("7F"));
  EXPECT_EQ(kBad, run("80"));         // Out of data.
  EXPECT_EQ(good(2U), run("80 00"));  // Redundant code.
  EXPECT_EQ(good(2U), run("80 01"));
  EXPECT_EQ(good(2U), run("FF 00"));  // Redundant (unsigned).
  EXPECT_EQ(good(2U), run("FF 7F"));
  EXPECT_EQ(good(1U), run("00 80"));     // Only reads byte 0.
  EXPECT_EQ(kBad, run("80 80"));         // Out of data.
  EXPECT_EQ(kBad, run("F1 88"));         // Out of data.
  EXPECT_EQ(good(3U), run("80 80 00"));  // Redundant code.
  EXPECT_EQ(good(3U), run("80 80 01"));
  EXPECT_EQ(good(3U), run("80 80 40"));
  EXPECT_EQ(good(3U), run("FF FF 7F"));
  EXPECT_EQ(good(1U), run("00 00 80"));     // Only reads byte 0.
  EXPECT_EQ(kBad, run("80 80 80"));         // Out of data.
  EXPECT_EQ(kBad, run("AB CD EF"));         // Out of data.
  EXPECT_EQ(good(4U), run("80 80 80 00"));  // Redundant code.
  EXPECT_EQ(good(4U), run("80 80 C0 00"));
  EXPECT_EQ(good(4U), run("80 80 80 01"));
  EXPECT_EQ(good(4U), run("80 80 80 40"));
  EXPECT_EQ(good(4U), run("FF 80 FF 00"));
  EXPECT_EQ(good(5U), run("80 80 80 80 00"));  // Redundant code.
  EXPECT_EQ(good(5U), run("80 80 80 80 01"));
  EXPECT_EQ(good(5U), run("81 81 81 81 01"));
  EXPECT_EQ(good(5U), run("FF FF FF FF 07"));
  EXPECT_EQ(good(5U), run("80 80 80 80 08"));
  EXPECT_EQ(good(5U), run("FF FF FF FF 0F"));
  EXPECT_EQ(kBad, run("FF FF FF FF 80"));      // Too long / out of data.
  EXPECT_EQ(good(5U), run("FF FF FF FF 10"));  // "1" discarded.
  EXPECT_EQ(good(5U), run("80 80 80 80 20"));  // "2" discarded.
  EXPECT_EQ(good(5U), run("AA AA AA AA 7A"));  // "7" discarded.
  EXPECT_EQ(kBad, run("FF FF FF FF FF 00"));   // Too long.
}

}  // namespace zucchini
