// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/base/vector_math.h"

#include <algorithm>
#include <cmath>
#include <memory>

#include "base/cpu.h"
#include "base/memory/aligned_memory.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringize_macros.h"
#include "build/build_config.h"
#include "media/base/vector_math_testing.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

// Default test values.
static constexpr float kScale = 0.5;
static constexpr float kInputFillValue = 1.0;
static constexpr float kOutputFillValue = 3.0;
static constexpr int kVectorSize = 8192;

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

  void VerifyOutput(float value) {
    EXPECT_TRUE(std::ranges::all_of(
        output_array_, [value](float datum) { return datum == value; }));
  }

 protected:
  base::AlignedHeapArray<float> input_array_;
  base::AlignedHeapArray<float> output_array_;
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
