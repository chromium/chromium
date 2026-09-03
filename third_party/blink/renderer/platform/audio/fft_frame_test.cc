// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/audio/fft_frame.h"

#include <algorithm>
#include <array>
#include <cmath>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/audio/audio_array.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

namespace {

struct TestParams {
  unsigned size;
  const char* description;
};

enum class SignalType { kImpulse, kSine, kDC, kNoise };

// Tolerance for comparing round-trip (FFT then inverse FFT) results.
constexpr float kIdentityTolerance = 1e-5;

constexpr auto kTestParams = std::to_array<TestParams>({
    // Powers of two
    {2, "PowerOfTwo_2"},
    {4, "PowerOfTwo_4"},
    {8, "PowerOfTwo_8"},
    {16, "PowerOfTwo_16"},
    {32, "PowerOfTwo_32"},
    {64, "PowerOfTwo_64"},
    {128, "PowerOfTwo_128"},
    {256, "PowerOfTwo_256"},
    {512, "PowerOfTwo_512"},
    {1024, "PowerOfTwo_1024"},
    {2048, "PowerOfTwo_2048"},
    {4096, "PowerOfTwo_4096"},
    {8192, "PowerOfTwo_8192"},
    {16384, "PowerOfTwo_16384"},
    {32768, "PowerOfTwo_32768"},

    // Small even sizes
    {6, "SmallEven_6"},
    {10, "SmallEven_10"},
    {12, "SmallEven_12"},
    {14, "SmallEven_14"},
    {18, "SmallEven_18"},
    {20, "SmallEven_20"},
    {22, "SmallEven_22"},
    {24, "SmallEven_24"},
    {26, "SmallEven_26"},
    {28, "SmallEven_28"},

    // Even non-powers-of-two (N/2 is prime)
    {34, "PrimeHalf_17"},
    {38, "PrimeHalf_19"},
    {46, "PrimeHalf_23"},
    {58, "PrimeHalf_29"},
    {62, "PrimeHalf_31"},

    // Even non-powers-of-two (composite)
    {96, "Composite_96"},
    {160, "Composite_160"},
    {192, "Composite_192"},

    // Odd Primes
    {3, "Prime_3"},
    {5, "Prime_5"},
    {7, "Prime_7"},
    {11, "Prime_11"},
    {13, "Prime_13"},
    {17, "Prime_17"},
    {19, "Prime_19"},
    {23, "Prime_23"},
    {29, "Prime_29"},
    {31, "Prime_31"},

    // Odd Composites
    {9, "Composite_9"},
    {15, "Composite_15"},
    {21, "Composite_21"},
    {25, "Composite_25"},
    {27, "Composite_27"},
    {33, "Composite_33"},
});

void GenerateSignal(base::span<float> data, SignalType type) {
  const size_t size = data.size();
  if (type == SignalType::kImpulse) {
    std::fill(data.begin(), data.end(), 0.0f);
    data[0] = 1.0f;
  } else if (type == SignalType::kSine) {
    for (size_t i = 0; i < size; ++i) {
      // 3 cycles
      data[i] = std::sin(2.0f * kPiFloat * 3.0f * i / size);
    }
  } else if (type == SignalType::kDC) {
    std::fill(data.begin(), data.end(), 1.0f);
  } else if (type == SignalType::kNoise) {
    // Simple deterministic pseudo-random noise (LCG).
    uint32_t seed = 1;
    for (size_t i = 0; i < size; ++i) {
      seed = 1103515245u * seed + 12345u;
      float rand_val = static_cast<float>((seed >> 16) & 0x7FFF) / 32768.0f;
      data[i] = rand_val * 2.0f - 1.0f;
    }
  }
}

void RunIdentityTransformTest(unsigned fft_size) {
  FFTFrame frame(fft_size);
  AudioFloatArray input(fft_size);
  AudioFloatArray output(fft_size);

  for (SignalType signal : {SignalType::kImpulse, SignalType::kSine,
                            SignalType::kDC, SignalType::kNoise}) {
    GenerateSignal(input.as_span(), signal);
    frame.DoFFT(input.as_span());
    frame.DoInverseFFT(output.as_span());

    ASSERT_EQ(input.size(), fft_size);
    ASSERT_EQ(output.size(), fft_size);
    EXPECT_THAT(input.as_span(),
                ::testing::Pointwise(::testing::FloatNear(kIdentityTolerance),
                                     output.as_span()))
        << "for size " << fft_size << " and signal type "
        << static_cast<int>(signal);
  }
}

class FFTFrameTest : public testing::TestWithParam<TestParams> {};

TEST_P(FFTFrameTest, IdentityTransform) {
  RunIdentityTransformTest(GetParam().size);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    FFTFrameTest,
    testing::ValuesIn(kTestParams),
    [](const testing::TestParamInfo<FFTFrameTest::ParamType>& info) {
      return info.param.description;
    });

TEST(FFTFrameSimpleTest, ExactValuesSize2) {
  FFTFrame frame(2);
  AudioFloatArray input(2);
  input[0] = 1.0f;
  input[1] = 2.0f;

  frame.DoFFT(input.as_span());

  EXPECT_EQ(frame.RealData()[0], 3.0f);
  EXPECT_EQ(frame.ImagData()[0], -1.0f);

  AudioFloatArray output(2);
  frame.DoInverseFFT(output.as_span());

  EXPECT_NEAR(output[0], 1.0f, kIdentityTolerance);
  EXPECT_NEAR(output[1], 2.0f, kIdentityTolerance);
}

}  // namespace

}  // namespace blink
