// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/public/common/privacy_budget/identifiable_token_builder.h"

#include <cstdint>
#include <vector>

#include "base/containers/span.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_token.h"

namespace blink {

TEST(IdentifiableTokenBuilderTest, Empty) {
  IdentifiableTokenBuilder sample;
  EXPECT_EQ(IdentifiableToken(INT64_C(0x5ad32e10d3a3c2b5)), sample.GetToken());
}

TEST(IdentifiableTokenBuilderTest, ConstructVsAdd) {
  const char kOneByte[1] = {'a'};
  IdentifiableTokenBuilder add_token;
  add_token.AddBytes(base::as_bytes(base::make_span(kOneByte)));

  IdentifiableTokenBuilder construct_token(
      base::as_bytes(base::make_span(kOneByte)));
  EXPECT_EQ(add_token.GetToken(), construct_token.GetToken());
}

TEST(IdentifiableTokenBuilderTest, OneByte) {
  const char kOneByte[1] = {'a'};
  IdentifiableTokenBuilder sample;
  sample.AddBytes(base::as_bytes(base::make_span(kOneByte)));
  EXPECT_EQ(IdentifiableToken(INT64_C(0x6de50a5cefa7ba0e)), sample.GetToken());
}

TEST(IdentifiableTokenBuilderTest, TwoBytesInTwoTakes) {
  const char kBytes[] = {'a', 'b'};
  auto bytes_span = base::as_bytes(base::span<const char>(kBytes));
  IdentifiableTokenBuilder whole_span_token(bytes_span);
  IdentifiableTokenBuilder two_parts_token;
  two_parts_token.AddBytes(bytes_span.first(1u));
  two_parts_token.AddBytes(bytes_span.last(1u));
  EXPECT_EQ(whole_span_token.GetToken(), two_parts_token.GetToken());
}

TEST(IdentifiableTokenBuilderTest, SixtySixBytesInTwoTakes) {
  constexpr size_t kSize = 66;
  std::vector<char> big_array(kSize, 'a');
  auto bytes_span = base::as_bytes(base::span<const char>(big_array));
  IdentifiableTokenBuilder whole_span_token(bytes_span);
  IdentifiableTokenBuilder two_parts_token;
  two_parts_token.AddBytes(bytes_span.first(kSize / 2));
  two_parts_token.AddBytes(bytes_span.last(kSize / 2));
  EXPECT_EQ(whole_span_token.GetToken(), two_parts_token.GetToken());
}

TEST(IdentifiableTokenBuilderTest, AddValue) {
  const auto kExpectedToken = IdentifiableToken(INT64_C(0xe475af2a732298e2));

  EXPECT_EQ(kExpectedToken,
            IdentifiableTokenBuilder().AddValue(INT8_C(1)).GetToken());
  EXPECT_EQ(kExpectedToken,
            IdentifiableTokenBuilder().AddValue(UINT8_C(1)).GetToken());
  EXPECT_EQ(kExpectedToken,
            IdentifiableTokenBuilder().AddValue(INT16_C(1)).GetToken());
  EXPECT_EQ(kExpectedToken,
            IdentifiableTokenBuilder().AddValue(UINT16_C(1)).GetToken());
  EXPECT_EQ(kExpectedToken,
            IdentifiableTokenBuilder().AddValue(INT32_C(1)).GetToken());
  EXPECT_EQ(kExpectedToken,
            IdentifiableTokenBuilder().AddValue(UINT32_C(1)).GetToken());
  EXPECT_EQ(kExpectedToken,
            IdentifiableTokenBuilder().AddValue(INT64_C(1)).GetToken());
  EXPECT_EQ(kExpectedToken,
            IdentifiableTokenBuilder().AddValue(UINT64_C(1)).GetToken());
}

TEST(IdentifiableTokenBuilderTest, AddAtomic_AlwaysConstant) {
  const uint8_t kS1[] = {1, 2, 3, 4, 5};
  EXPECT_EQ(
      IdentifiableToken(INT64_C(0xfaeb0b8e769729b9)),
      IdentifiableTokenBuilder().AddAtomic(base::make_span(kS1)).GetToken());
}

TEST(IdentifiableTokenBuilderTest, AddAtomic_PadSuffix) {
  const uint8_t kS1[] = {1, 2, 3, 4, 5};
  const uint8_t kS1_padded[] = {5, 0, 0, 0, 0, 0, 0, 0,  // Little endian 5
                                1, 2, 3, 4, 5, 0, 0, 0};
  EXPECT_EQ(
      IdentifiableTokenBuilder().AddAtomic(base::make_span(kS1)).GetToken(),
      IdentifiableTokenBuilder()
          .AddBytes(base::make_span(kS1_padded))
          .GetToken());
}

TEST(IdentifiableTokenBuilderTest, AddAtomic_PadPrefix) {
  const uint8_t kS2_pre[] = {1, 2};
  const uint8_t kS2[] = {3, 4, 5};
  const uint8_t kS2_padded[] = {1, 2, 0, 0, 0, 0, 0, 0,
                                3, 0, 0, 0, 0, 0, 0, 0,  // Little endian 3
                                3, 4, 5, 0, 0, 0, 0, 0};
  EXPECT_EQ(IdentifiableTokenBuilder()
                .AddBytes(base::make_span(kS2_pre))
                .AddAtomic(base::make_span(kS2))
                .GetToken(),
            IdentifiableTokenBuilder()
                .AddBytes(base::make_span(kS2_padded))
                .GetToken());
}

TEST(IdentifiableTokenBuilderTest, AddVsAddAtomic) {
  const uint8_t kA1[] = {'a', 'b', 'c', 'd'};
  const uint8_t kA2[] = {'e', 'f', 'g', 'h'};
  const uint8_t kB1[] = {'a'};
  const uint8_t kB2[] = {'b', 'c', 'd', 'e', 'f', 'g', 'h'};

  // Adding buffers wth AddBytes() doesn't distinguish between the two
  // partitions, and this is intentional.
  IdentifiableTokenBuilder builder_A;
  builder_A.AddBytes(base::make_span(kA1));
  builder_A.AddBytes(base::make_span(kA2));
  auto token_for_A = builder_A.GetToken();

  IdentifiableTokenBuilder builder_B;
  builder_B.AddBytes(base::make_span(kB1));
  builder_B.AddBytes(base::make_span(kB2));
  auto token_for_B = builder_B.GetToken();

  EXPECT_EQ(token_for_A, token_for_B);

  // However AtomicAdd distinguishes between the two.
  IdentifiableTokenBuilder atomic_A;
  atomic_A.AddAtomic(base::make_span(kA1));
  atomic_A.AddAtomic(base::make_span(kA2));
  auto atomic_token_for_A = atomic_A.GetToken();

  IdentifiableTokenBuilder atomic_B;
  atomic_B.AddAtomic(base::make_span(kB1));
  atomic_B.AddAtomic(base::make_span(kB2));
  auto atomic_token_for_B = atomic_B.GetToken();

  EXPECT_NE(atomic_token_for_A, atomic_token_for_B);
}

TEST(IdentifiableTokenBuilderTest, LotsOfRandomPartitions) {
  constexpr size_t kLargeBufferSize = 1000 * 1000 * 10;
  constexpr size_t kCycle = 149;  // A prime
  std::vector<uint8_t> data(kLargeBufferSize);
  for (size_t i = 0; i < kLargeBufferSize; ++i) {
    data[i] = i % kCycle;
  }

  int partition_count = base::RandInt(50, 500);

  // Pick |partition_count| random numbers between 0 and kLargeBufferSize (half
  // open) that will indicate where the partitions are. In reality there will be
  // |partition_count + 1| partitions:
  //
  //   |<--------->:<------------>:<---    ..  ...->:<---------------------->|
  //   |           :              :        ..  ...  :                        |
  //   0      partitions[0]  partitions[1] .. partitions[n-1] kLargeBufferSize
  std::vector<size_t> partitions;
  for (int i = 0; i < partition_count; ++i)
    partitions.push_back(base::RandInt(0, kLargeBufferSize));
  std::sort(partitions.begin(), partitions.end());

  std::string trace;
  base::StringAppendF(&trace, "Partitions[%d]={0", partition_count + 2);
  for (auto p : partitions)
    base::StringAppendF(&trace, ", %zu", p);
  base::StringAppendF(&trace, ", %zu}", kLargeBufferSize);
  SCOPED_TRACE(trace);

  IdentifiableTokenBuilder partitioned_sample;
  size_t low = 0;
  for (auto high : partitions) {
    ASSERT_LE(low, high);
    partitioned_sample.AddBytes(base::make_span(&data[low], high - low));
    low = high;
  }
  partitioned_sample.AddBytes(
      base::make_span(&data[low], kLargeBufferSize - low));

  IdentifiableTokenBuilder full_buffer_sample(data);
  EXPECT_EQ(full_buffer_sample.GetToken(), partitioned_sample.GetToken());
}

}  // namespace blink
