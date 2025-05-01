// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "media/base/vector_math.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>

#include "base/containers/span_reader.h"
#include "base/containers/span_writer.h"
#include "base/cpu.h"
#include "base/memory/aligned_memory.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringize_macros.h"
#include "base/types/zip.h"
#include "build/build_config.h"
#include "media/base/vector_math_testing.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

// Default test values.
static constexpr float kScale = 0.5;
static constexpr float kInputFillValue = 1.0;
static constexpr float kOutputFillValue = 3.0;
static constexpr int kVectorSize = 8192;

// List of unclamped values that are out of bounds and within bounds.
static const float kUnclampedInputValues[] = {
    std::numeric_limits<float>::quiet_NaN(),
    std::numeric_limits<float>::signaling_NaN(),
    -std::numeric_limits<float>::infinity(),
    std::numeric_limits<float>::infinity(),
    -2.0,
    -1.0,
    -0.5,
    0.0,
    0.5,
    1.0,
    2.0,
};

// Expected result of clamping `kUnclampedInputValues`.
static const float kClampedOutputValues[] = {0,    0,   -1.0, 1.0, -1.0, -1.0,
                                             -0.5, 0.0, 0.5,  1.0, 1.0};

static_assert(std::size(kUnclampedInputValues) ==
              std::size(kClampedOutputValues));

class VectorMathTest : public testing::Test {
 public:
  VectorMathTest() {
    // Initialize input and output vectors.
    input_array_ = base::AlignedUninit<float>(kVectorSize,
                                              vector_math::kRequiredAlignment);
    output_array_ = base::AlignedUninit<float>(kVectorSize,
                                               vector_math::kRequiredAlignment);
  }

  VectorMathTest(const VectorMathTest&) = delete;
  VectorMathTest& operator=(const VectorMathTest&) = delete;

  void FillTestVectors(float input, float output) {
    // Setup input and output vectors.
    std::ranges::fill(input_array_, input);
    std::ranges::fill(output_array_, output);
  }

  void FillTestClampingVectors(base::span<const float> input, float output) {
    // Setup input and output vectors.
    FillSpan(input_array_, input);
    std::ranges::fill(output_array_, output);
  }

  void VerifyOutput(float value) {
    EXPECT_TRUE(std::ranges::all_of(
        output_array_, [value](float datum) { return datum == value; }));
  }

  void VerifyClampOutput(base::span<const float> values) {
    auto reader = base::SpanReader(base::span(output_array_));

    while (reader.remaining() > values.size()) {
      auto output_values = *reader.Read(values.size());
      EXPECT_EQ(output_values, values);
    }

    if (reader.remaining()) {
      auto remaining_values = reader.remaining_span();
      EXPECT_EQ(remaining_values, values.first(remaining_values.size()));
    }
  }

 protected:
  base::AlignedHeapArray<float> input_array_;
  base::AlignedHeapArray<float> output_array_;

 private:
  // Fills `dest` with `values`, repeating `values`.
  void FillSpan(base::span<float> dest, base::span<const float> values) {
    auto writer = base::SpanWriter(dest);

    // Fill as much as possible with `values`.
    while (writer.remaining() > values.size()) {
      writer.Write(values);
    }

    // Fill the remaining space with the start of values.
    if (writer.remaining()) {
      writer.Write(values.first((writer.remaining())));
    }
  }
};

// Ensure each optimized vector_math::FMAC() method returns the same value.
TEST_F(VectorMathTest, FMAC) {
  static const float kResult = kInputFillValue * kScale + kOutputFillValue;

  {
    SCOPED_TRACE("FMAC");
    FillTestVectors(kInputFillValue, kOutputFillValue);
    vector_math::FMAC(input_array_, kScale, output_array_);
    VerifyOutput(kResult);
  }

  {
    SCOPED_TRACE("FMAC_C");
    FillTestVectors(kInputFillValue, kOutputFillValue);
    vector_math::FMAC_C(input_array_.data(), kScale, kVectorSize,
                        output_array_.data());
    VerifyOutput(kResult);
  }

#if defined(ARCH_CPU_X86_FAMILY)
  {
    SCOPED_TRACE("FMAC_SSE");
    FillTestVectors(kInputFillValue, kOutputFillValue);
    vector_math::FMAC_SSE(input_array_.data(), kScale, kVectorSize,
                          output_array_.data());
    VerifyOutput(kResult);
  }
  {
    base::CPU cpu;
    if (cpu.has_avx2() && cpu.has_fma3()) {
      SCOPED_TRACE("FMAC_AVX2");
      FillTestVectors(kInputFillValue, kOutputFillValue);
      vector_math::FMAC_AVX2(input_array_.data(), kScale, kVectorSize,
                             output_array_.data());
      VerifyOutput(kResult);
    }
  }
#endif

#if defined(ARCH_CPU_ARM_FAMILY) && defined(USE_NEON)
  {
    SCOPED_TRACE("FMAC_NEON");
    FillTestVectors(kInputFillValue, kOutputFillValue);
    vector_math::FMAC_NEON(input_array_.data(), kScale, kVectorSize,
                           output_array_.data());
    VerifyOutput(kResult);
  }
#endif
}

// Ensure each optimized vector_math::FMUL() method returns the same value.
TEST_F(VectorMathTest, FMUL) {
  static const float kResult = kInputFillValue * kScale;

  {
    SCOPED_TRACE("FMUL");
    FillTestVectors(kInputFillValue, kOutputFillValue);
    vector_math::FMUL(input_array_, kScale, output_array_);
    VerifyOutput(kResult);
  }

  {
    SCOPED_TRACE("FMUL_C");
    FillTestVectors(kInputFillValue, kOutputFillValue);
    vector_math::FMUL_C(input_array_.data(), kScale, kVectorSize,
                        output_array_.data());
    VerifyOutput(kResult);
  }

#if defined(ARCH_CPU_X86_FAMILY)
  {
    SCOPED_TRACE("FMUL_SSE");
    FillTestVectors(kInputFillValue, kOutputFillValue);
    vector_math::FMUL_SSE(input_array_.data(), kScale, kVectorSize,
                          output_array_.data());
    VerifyOutput(kResult);
  }
  {
    base::CPU cpu;
    if (cpu.has_avx2()) {
      SCOPED_TRACE("FMUL_AVX2");
      FillTestVectors(kInputFillValue, kOutputFillValue);
      vector_math::FMUL_AVX2(input_array_.data(), kScale, kVectorSize,
                             output_array_.data());
      VerifyOutput(kResult);
    }
  }
#endif

#if defined(ARCH_CPU_ARM_FAMILY) && defined(USE_NEON)
  {
    SCOPED_TRACE("FMUL_NEON");
    FillTestVectors(kInputFillValue, kOutputFillValue);
    vector_math::FMUL_NEON(input_array_.data(), kScale, kVectorSize,
                           output_array_.data());
    VerifyOutput(kResult);
  }
#endif
}

// Ensure each optimized vector_math::FCLAMP() method returns the same value.
TEST_F(VectorMathTest, FCLAMP) {
  {
    SCOPED_TRACE("FCLAMP");
    FillTestClampingVectors(kUnclampedInputValues, kOutputFillValue);
    vector_math::FCLAMP(input_array_, output_array_);
    VerifyClampOutput(kClampedOutputValues);
  }

  {
    SCOPED_TRACE("FCLAMP_C");
    FillTestClampingVectors(kUnclampedInputValues, kOutputFillValue);
    vector_math::FCLAMP_C(input_array_.data(), kVectorSize,
                          output_array_.data());
    VerifyClampOutput(kClampedOutputValues);
  }

#if defined(ARCH_CPU_X86_FAMILY)
  {
    SCOPED_TRACE("FCLAMP_SSE");
    FillTestClampingVectors(kUnclampedInputValues, kOutputFillValue);
    vector_math::FCLAMP_SSE(input_array_.data(), kVectorSize,
                            output_array_.data());
    VerifyClampOutput(kClampedOutputValues);
  }
  {
    base::CPU cpu;
    if (cpu.has_avx()) {
      SCOPED_TRACE("FCLAMP_AVX");
      FillTestClampingVectors(kUnclampedInputValues, kOutputFillValue);
      vector_math::FCLAMP_AVX(input_array_.data(), kVectorSize,
                              output_array_.data());
      VerifyClampOutput(kClampedOutputValues);
    }
  }
#endif

#if defined(ARCH_CPU_ARM_FAMILY) && defined(USE_NEON)
  {
    SCOPED_TRACE("FCLAMP_NEON");
    FillTestClampingVectors(kUnclampedInputValues, kOutputFillValue);
    vector_math::FCLAMP_NEON(input_array_.data(), kVectorSize,
                             output_array_.data());
    VerifyClampOutput(kClampedOutputValues);
  }
#endif
}

// Algorithms handle "leftover" data that is too small to fill an SIMD
// instruction differently. Make sure that this data is also properly sanitized.
TEST_F(VectorMathTest, FCLAMP_remainder_data) {
  // Feed in values 1 at a time to guarantee we don't use SIMD.
  static constexpr int kSmallVectorSize = 1;
  static constexpr float kGuardValue = 123.0f;

  const auto run_per_value_clamp_test =
      [&](void (*fn)(const float[], int, float[])) {
        for (auto [input, output] :
             base::zip(kUnclampedInputValues, kClampedOutputValues)) {
          input_array_[0] = input;
          output_array_[0] = kGuardValue;
          fn(input_array_.data(), kSmallVectorSize, output_array_.data());
          EXPECT_EQ(output_array_[0], output);
        }
      };

  {
    SCOPED_TRACE("FCLAMP_C");
    run_per_value_clamp_test(vector_math::FCLAMP_C);
  }

#if defined(ARCH_CPU_X86_FAMILY)
  {
    SCOPED_TRACE("FCLAMP_SSE");
    run_per_value_clamp_test(vector_math::FCLAMP_SSE);
  }
  {
    base::CPU cpu;
    if (cpu.has_avx()) {
      SCOPED_TRACE("FCLAMP_AVX");
      run_per_value_clamp_test(vector_math::FCLAMP_AVX);
    }
  }
#endif

#if defined(ARCH_CPU_ARM_FAMILY) && defined(USE_NEON)
  {
    SCOPED_TRACE("FCLAMP_NEON");
    run_per_value_clamp_test(vector_math::FCLAMP_NEON);
  }
#endif
}

TEST_F(VectorMathTest, EmptyInputs) {
  {
    SCOPED_TRACE("FMUL");
    FillTestVectors(kInputFillValue, kOutputFillValue);
    vector_math::FMUL(base::span<float>(), kScale, output_array_);
    VerifyOutput(kOutputFillValue);
  }

  {
    SCOPED_TRACE("FMAC");
    FillTestVectors(kInputFillValue, kOutputFillValue);
    vector_math::FMAC(base::span<float>(), kScale, output_array_);
    VerifyOutput(kOutputFillValue);
  }

  {
    SCOPED_TRACE("FCLAMP");
    FillTestVectors(kInputFillValue, kOutputFillValue);
    vector_math::FMAC(base::span<float>(), kScale, output_array_);
    VerifyOutput(kOutputFillValue);
  }
}

class EWMATestScenario {
 public:
  EWMATestScenario(float initial_value,
                   base::span<const float> src,
                   float smoothing_factor)
      : initial_value_(initial_value),
        smoothing_factor_(smoothing_factor),
        expected_final_avg_(initial_value) {
    CopyDataAligned(src);
  }

  // Copy constructor and assignment operator for ::testing::Values(...).
  EWMATestScenario(const EWMATestScenario& other) { *this = other; }
  EWMATestScenario& operator=(const EWMATestScenario& other) {
    this->initial_value_ = other.initial_value_;
    this->smoothing_factor_ = other.smoothing_factor_;
    this->CopyDataAligned(other.data_);
    this->expected_final_avg_ = other.expected_final_avg_;
    this->expected_max_ = other.expected_max_;
    return *this;
  }

  EWMATestScenario ScaledBy(float scale) const {
    EWMATestScenario result(*this);
    std::ranges::for_each(result.data_,
                          [scale](float& datum) { datum *= scale; });
    return result;
  }

  EWMATestScenario WithImpulse(float value, int offset) const {
    EWMATestScenario result(*this);
    result.data_[offset] = value;
    return result;
  }

  EWMATestScenario HasExpectedResult(float final_avg_value,
                                     float max_value) const {
    EWMATestScenario result(*this);
    result.expected_final_avg_ = final_avg_value;
    result.expected_max_ = max_value;
    return result;
  }

  void RunTest() const {
    {
      SCOPED_TRACE("EWMAAndMaxPower");
      const std::pair<float, float>& result = vector_math::EWMAAndMaxPower(
          initial_value_, data_, smoothing_factor_);
      EXPECT_NEAR(expected_final_avg_, result.first, 0.0000001f);
      EXPECT_NEAR(expected_max_, result.second, 0.0000001f);
    }

    {
      SCOPED_TRACE("EWMAAndMaxPower_C");
      const std::pair<float, float>& result = vector_math::EWMAAndMaxPower_C(
          initial_value_, data_.data(), data_.size(), smoothing_factor_);
      EXPECT_NEAR(expected_final_avg_, result.first, 0.0000001f);
      EXPECT_NEAR(expected_max_, result.second, 0.0000001f);
    }

#if defined(ARCH_CPU_X86_FAMILY)
    {
      SCOPED_TRACE("EWMAAndMaxPower_SSE");
      const std::pair<float, float>& result = vector_math::EWMAAndMaxPower_SSE(
          initial_value_, data_.data(), data_.size(), smoothing_factor_);
      EXPECT_NEAR(expected_final_avg_, result.first, 0.0000001f);
      EXPECT_NEAR(expected_max_, result.second, 0.0000001f);
    }
    {
      base::CPU cpu;
      if (cpu.has_avx2() && cpu.has_fma3()) {
        SCOPED_TRACE("EWMAAndMaxPower_AVX2");
        const std::pair<float, float>& result =
            vector_math::EWMAAndMaxPower_AVX2(initial_value_, data_.data(),
                                              data_.size(), smoothing_factor_);
        EXPECT_NEAR(expected_final_avg_, result.first, 0.0000001f);
        EXPECT_NEAR(expected_max_, result.second, 0.0000001f);
      }
    }
#endif

#if defined(ARCH_CPU_ARM_FAMILY) && defined(USE_NEON)
    {
      SCOPED_TRACE("EWMAAndMaxPower_NEON");
      const std::pair<float, float>& result = vector_math::EWMAAndMaxPower_NEON(
          initial_value_, data_.data(), data_.size(), smoothing_factor_);
      EXPECT_NEAR(expected_final_avg_, result.first, 0.0000001f);
      EXPECT_NEAR(expected_max_, result.second, 0.0000001f);
    }
#endif
  }

 private:
  void CopyDataAligned(base::span<const float> src) {
    if (src.empty()) {
      data_ = base::AlignedHeapArray<float>();
      return;
    }

    data_ =
        base::AlignedUninit<float>(src.size(), vector_math::kRequiredAlignment);
    data_.copy_from(src);
  }

  float initial_value_;
  base::AlignedHeapArray<float> data_;
  float smoothing_factor_;
  float expected_final_avg_;
  float expected_max_ = 0.0f;
};

typedef testing::TestWithParam<EWMATestScenario> VectorMathEWMAAndMaxPowerTest;

TEST_P(VectorMathEWMAAndMaxPowerTest, Correctness) {
  GetParam().RunTest();
}

static const float kZeros[] = {  // 32 zeros
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static const float kOnes[] = {  // 32 ones
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
};

static const float kCheckerboard[] = {  // 32 alternating 0, 1
  0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1,
  0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1
};

static const float kInverseCheckerboard[] = {  // 32 alternating 1, 0
  1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0,
  1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0
};

INSTANTIATE_TEST_SUITE_P(
    Scenarios,
    VectorMathEWMAAndMaxPowerTest,
    ::testing::Values(
        // Zero-length input: Result should equal initial value.
        EWMATestScenario(0.0f, base::span<float>(), 0.0f)
            .HasExpectedResult(0.0f, 0.0f),
        EWMATestScenario(1.0f, base::span<float>(), 0.0f)
            .HasExpectedResult(1.0f, 0.0f),

        // Smoothing factor of zero: Samples have no effect on result.
        EWMATestScenario(0.0f, kOnes, 0.0f).HasExpectedResult(0.0f, 1.0f),
        EWMATestScenario(1.0f, kZeros, 0.0f).HasExpectedResult(1.0f, 0.0f),

        // Smothing factor of one: Result = last sample squared.
        EWMATestScenario(0.0f, kCheckerboard, 1.0f)
            .ScaledBy(2.0f)
            .HasExpectedResult(4.0f, 4.0f),
        EWMATestScenario(1.0f, kInverseCheckerboard, 1.0f)
            .ScaledBy(2.0f)
            .HasExpectedResult(0.0f, 4.0f),

        // Smoothing factor of 1/4, muted signal.
        EWMATestScenario(1.0f, base::span(kZeros).first(1u), 0.25f)
            .HasExpectedResult(std::pow(0.75f, 1.0f), 0.0f),
        EWMATestScenario(1.0f, base::span(kZeros).first(2u), 0.25f)
            .HasExpectedResult(std::pow(0.75f, 2.0f), 0.0f),
        EWMATestScenario(1.0f, base::span(kZeros).first(3u), 0.25f)
            .HasExpectedResult(std::pow(0.75f, 3.0f), 0.0f),
        EWMATestScenario(1.0f, base::span(kZeros).first(12u), 0.25f)
            .HasExpectedResult(std::pow(0.75f, 12.0f), 0.0f),
        EWMATestScenario(1.0f, base::span(kZeros).first(13u), 0.25f)
            .HasExpectedResult(std::pow(0.75f, 13.0f), 0.0f),
        EWMATestScenario(1.0f, base::span(kZeros).first(14u), 0.25f)
            .HasExpectedResult(std::pow(0.75f, 14.0f), 0.0f),
        EWMATestScenario(1.0f, base::span(kZeros).first(15u), 0.25f)
            .HasExpectedResult(std::pow(0.75f, 15.0f), 0.0f),

        // Smoothing factor of 1/4, constant full-amplitude signal.
        EWMATestScenario(0.0f, base::span(kOnes).first(1u), 0.25f)
            .HasExpectedResult(0.25f, 1.0f),
        EWMATestScenario(0.0f, base::span(kOnes).first(2u), 0.25f)
            .HasExpectedResult(0.4375f, 1.0f),
        EWMATestScenario(0.0f, base::span(kOnes).first(3u), 0.25f)
            .HasExpectedResult(0.578125f, 1.0f),
        EWMATestScenario(0.0f, base::span(kOnes).first(12u), 0.25f)
            .HasExpectedResult(0.96832365f, 1.0f),
        EWMATestScenario(0.0f, base::span(kOnes).first(13u), 0.25f)
            .HasExpectedResult(0.97624274f, 1.0f),
        EWMATestScenario(0.0f, base::span(kOnes).first(14u), 0.25f)
            .HasExpectedResult(0.98218205f, 1.0f),
        EWMATestScenario(0.0f, base::span(kOnes).first(15u), 0.25f)
            .HasExpectedResult(0.98663654f, 1.0f),

        // Smoothing factor of 1/4, checkerboard signal.
        EWMATestScenario(0.0f, base::span(kCheckerboard).first(1u), 0.25f)
            .HasExpectedResult(0.0f, 0.0f),
        EWMATestScenario(0.0f, base::span(kCheckerboard).first(2u), 0.25f)
            .HasExpectedResult(0.25f, 1.0f),
        EWMATestScenario(0.0f, base::span(kCheckerboard).first(3u), 0.25f)
            .HasExpectedResult(0.1875f, 1.0f),
        EWMATestScenario(0.0f, base::span(kCheckerboard).first(12u), 0.25f)
            .HasExpectedResult(0.55332780f, 1.0f),
        EWMATestScenario(0.0f, base::span(kCheckerboard).first(13u), 0.25f)
            .HasExpectedResult(0.41499585f, 1.0f),
        EWMATestScenario(0.0f, base::span(kCheckerboard).first(14u), 0.25f)
            .HasExpectedResult(0.56124689f, 1.0f),
        EWMATestScenario(0.0f, base::span(kCheckerboard).first(15u), 0.25f)
            .HasExpectedResult(0.42093517f, 1.0f),

        // Smoothing factor of 1/4, inverse checkerboard signal.
        EWMATestScenario(0.0f,
                         base::span(kInverseCheckerboard).first(1u),
                         0.25f)
            .HasExpectedResult(0.25f, 1.0f),
        EWMATestScenario(0.0f,
                         base::span(kInverseCheckerboard).first(2u),
                         0.25f)
            .HasExpectedResult(0.1875f, 1.0f),
        EWMATestScenario(0.0f,
                         base::span(kInverseCheckerboard).first(3u),
                         0.25f)
            .HasExpectedResult(0.390625f, 1.0f),
        EWMATestScenario(0.0f,
                         base::span(kInverseCheckerboard).first(12u),
                         0.25f)
            .HasExpectedResult(0.41499585f, 1.0f),
        EWMATestScenario(0.0f,
                         base::span(kInverseCheckerboard).first(13u),
                         0.25f)
            .HasExpectedResult(0.56124689f, 1.0f),
        EWMATestScenario(0.0f,
                         base::span(kInverseCheckerboard).first(14u),
                         0.25f)
            .HasExpectedResult(0.42093517f, 1.0f),
        EWMATestScenario(0.0f,
                         base::span(kInverseCheckerboard).first(15u),
                         0.25f)
            .HasExpectedResult(0.56570137f, 1.0f),

        // Smoothing factor of 1/4, impluse signal.
        EWMATestScenario(0.0f, base::span(kZeros).first(3u), 0.25f)
            .WithImpulse(2.0f, 0)
            .HasExpectedResult(0.562500f, 4.0f),
        EWMATestScenario(0.0f, base::span(kZeros).first(3u), 0.25f)
            .WithImpulse(2.0f, 1)
            .HasExpectedResult(0.75f, 4.0f),
        EWMATestScenario(0.0f, base::span(kZeros).first(3u), 0.25f)
            .WithImpulse(2.0f, 2)
            .HasExpectedResult(1.0f, 4.0f),
        EWMATestScenario(0.0f, kZeros, 0.25f)
            .WithImpulse(2.0f, 0)
            .HasExpectedResult(0.00013394f, 4.0f),
        EWMATestScenario(0.0f, kZeros, 0.25f)
            .WithImpulse(2.0f, 1)
            .HasExpectedResult(0.00017858f, 4.0f),
        EWMATestScenario(0.0f, kZeros, 0.25f)
            .WithImpulse(2.0f, 2)
            .HasExpectedResult(0.00023811f, 4.0f),
        EWMATestScenario(0.0f, kZeros, 0.25f)
            .WithImpulse(2.0f, 3)
            .HasExpectedResult(0.00031748f, 4.0f)));

}  // namespace media
