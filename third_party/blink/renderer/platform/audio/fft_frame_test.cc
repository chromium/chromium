// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/audio/fft_frame.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <vector>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/audio/audio_array.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

namespace {

struct TestParams {
  unsigned size;
  const char* description;
};

enum class SignalType { kImpulse, kSine, kDC, kNoise };

// Tolerance for comparing frequency domain results across backends.  This is a
// relatively high value because different backends have different rounding and
// error accumulation properties.
constexpr float kComparisonTolerance = 1.5e-3;

// Tolerance for comparing round-trip (FFT then inverse FFT) results.
constexpr float kIdentityTolerance = 1e-5;

constexpr auto kPlatformEvenTestParams = std::to_array<TestParams>({
#if BUILDFLAG(IS_MAC)
    // Powers of two supported by Mac vDSP (< 32)
    {4, "PowerOfTwo_4"},
    {8, "PowerOfTwo_8"},
    {16, "PowerOfTwo_16"},
#endif
    // Powers of two supported by both Mac vDSP and PFFFT
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
#if !BUILDFLAG(IS_MAC)
    // Even non-powers-of-two supported by PFFFT
    {96, "Composite_96"},
    {160, "Composite_160"},
    {192, "Composite_192"},
#endif
});

constexpr auto kRustTestParams = std::to_array<TestParams>({
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

    // Even non-powers-of-two (composite PFFFT supported)
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

class FFTFramePlatformTest : public testing::TestWithParam<TestParams> {
 protected:
  static void SetUpTestSuite() { FFTFrame::Initialize(44100); }
  static void TearDownTestSuite() { FFTFrame::Cleanup(); }

  void SetUp() override {
    feature_override_ = std::make_unique<ScopedWebAudioRustFftForTest>(false);
  }

  std::unique_ptr<ScopedWebAudioRustFftForTest> feature_override_;
};

TEST_P(FFTFramePlatformTest, IdentityTransform) {
  RunIdentityTransformTest(GetParam().size);
}

INSTANTIATE_TEST_SUITE_P(
    Platform,
    FFTFramePlatformTest,
    testing::ValuesIn(kPlatformEvenTestParams),
    [](const testing::TestParamInfo<FFTFramePlatformTest::ParamType>& info) {
      return info.param.description;
    });

class FFTFrameComparisonTest : public testing::TestWithParam<TestParams> {
 protected:
  static void SetUpTestSuite() { FFTFrame::Initialize(44100); }
  static void TearDownTestSuite() { FFTFrame::Cleanup(); }
};

TEST_P(FFTFrameComparisonTest, CompareWithPlatform) {
  const unsigned fft_size = GetParam().size;

  AudioFloatArray input(fft_size);
  AudioFloatArray rust_output(fft_size);
  AudioFloatArray platform_output(fft_size);
  AudioFloatArray cross_output_rust(fft_size);
  AudioFloatArray cross_output_platform(fft_size);

  FFTFrame rust_frame(fft_size);
  std::unique_ptr<FFTFrame> platform_frame;
  FFTFrame cross_frame_rust(fft_size);
  std::unique_ptr<FFTFrame> cross_frame_platform;

  {
    ScopedWebAudioRustFftForTest rust_fft_enabled(false);
    platform_frame = std::make_unique<FFTFrame>(fft_size);
    cross_frame_platform = std::make_unique<FFTFrame>(fft_size);
  }

  for (SignalType signal : {SignalType::kImpulse, SignalType::kSine,
                            SignalType::kDC, SignalType::kNoise}) {
    GenerateSignal(input.as_span(), signal);

    // Run Rust FFT
    rust_frame.DoFFT(input.as_span());

    // Run Platform FFT
    platform_frame->DoFFT(input.as_span());

    // Compare frequency domain
    const size_t half_size = (fft_size + 1) / 2;
    ASSERT_EQ(rust_frame.RealData().size(), half_size);
    ASSERT_EQ(platform_frame->RealData().size(), half_size);
    ASSERT_EQ(rust_frame.ImagData().size(), half_size);
    ASSERT_EQ(platform_frame->ImagData().size(), half_size);
    EXPECT_THAT(rust_frame.RealData().as_span(),
                ::testing::Pointwise(::testing::FloatNear(kComparisonTolerance),
                                     platform_frame->RealData().as_span()))
        << "Real mismatch for size " << fft_size << " and signal type "
        << static_cast<int>(signal);
    EXPECT_THAT(rust_frame.ImagData().as_span(),
                ::testing::Pointwise(::testing::FloatNear(kComparisonTolerance),
                                     platform_frame->ImagData().as_span()))
        << "Imag mismatch for size " << fft_size << " and signal type "
        << static_cast<int>(signal);

    // Copy frequency domain data for cross-compatibility checks BEFORE running
    // inverse transforms, as some platform implementations (like Mac vDSP)
    // modify the frequency data in-place during DoInverseFFT.
    cross_frame_rust.RealData().as_span().copy_from(
        platform_frame->RealData().as_span());
    cross_frame_rust.ImagData().as_span().copy_from(
        platform_frame->ImagData().as_span());

    cross_frame_platform->RealData().as_span().copy_from(
        rust_frame.RealData().as_span());
    cross_frame_platform->ImagData().as_span().copy_from(
        rust_frame.ImagData().as_span());

    // Compare inverse transform
    rust_frame.DoInverseFFT(rust_output.as_span());
    platform_frame->DoInverseFFT(platform_output.as_span());

    ASSERT_EQ(rust_output.size(), fft_size);
    ASSERT_EQ(platform_output.size(), fft_size);
    EXPECT_THAT(rust_output.as_span(),
                ::testing::Pointwise(::testing::FloatNear(kIdentityTolerance),
                                     platform_output.as_span()))
        << "Inverse mismatch for size " << fft_size << " and signal type "
        << static_cast<int>(signal);

    // Cross-compatibility check: Rust inverse on Platform data
    cross_frame_rust.DoInverseFFT(cross_output_rust.as_span());

    ASSERT_EQ(cross_output_rust.size(), fft_size);
    EXPECT_THAT(cross_output_rust.as_span(),
                ::testing::Pointwise(::testing::FloatNear(kIdentityTolerance),
                                     input.as_span()))
        << "Cross Rust inverse mismatch for size " << fft_size
        << " and signal type " << static_cast<int>(signal);

    // Cross-compatibility check: Platform inverse on Rust data
    cross_frame_platform->DoInverseFFT(cross_output_platform.as_span());

    ASSERT_EQ(cross_output_platform.size(), fft_size);
    EXPECT_THAT(cross_output_platform.as_span(),
                ::testing::Pointwise(::testing::FloatNear(kIdentityTolerance),
                                     input.as_span()))
        << "Cross Platform inverse mismatch for size " << fft_size
        << " and signal type " << static_cast<int>(signal);
  }
}

INSTANTIATE_TEST_SUITE_P(
    Comparison,
    FFTFrameComparisonTest,
    testing::ValuesIn(kPlatformEvenTestParams),
    [](const testing::TestParamInfo<FFTFrameComparisonTest::ParamType>& info) {
      return info.param.description;
    });

class FFTFrameRustTest : public testing::TestWithParam<TestParams> {
 protected:
  static void SetUpTestSuite() { FFTFrame::Initialize(44100); }
  static void TearDownTestSuite() { FFTFrame::Cleanup(); }

  void SetUp() override {
    feature_override_ = std::make_unique<ScopedWebAudioRustFftForTest>(true);
  }

  std::unique_ptr<ScopedWebAudioRustFftForTest> feature_override_;
};

TEST_P(FFTFrameRustTest, IdentityTransform) {
  RunIdentityTransformTest(GetParam().size);
}

INSTANTIATE_TEST_SUITE_P(
    Rust,
    FFTFrameRustTest,
    testing::ValuesIn(kRustTestParams),
    [](const testing::TestParamInfo<FFTFrameRustTest::ParamType>& info) {
      return info.param.description;
    });

TEST(FFTFrameRustSimpleTest, ExactValuesSize2) {
  ScopedWebAudioRustFftForTest rust_fft_enabled(true);
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
