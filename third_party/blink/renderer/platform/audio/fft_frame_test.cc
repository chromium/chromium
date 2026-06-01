// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/audio/fft_frame.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/audio/audio_array.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

namespace {

struct TestParams {
  unsigned size;
  const char* description;
};

class FFTFrameParameterSizeTest : public testing::TestWithParam<TestParams> {
 protected:
  static void SetUpTestSuite() { FFTFrame::Initialize(44100); }
  static void TearDownTestSuite() { FFTFrame::Cleanup(); }

  void SetUp() override { params_ = GetParam(); }

  TestParams params_;
};

enum class SignalType { kImpulse, kSine, kDC, kNoise };

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

TEST_P(FFTFrameParameterSizeTest, IdentityTransform) {
  const unsigned fft_size = params_.size;

  FFTFrame frame(fft_size);
  AudioFloatArray input(fft_size);
  AudioFloatArray output(fft_size);

  for (SignalType signal : {SignalType::kImpulse, SignalType::kSine,
                            SignalType::kDC, SignalType::kNoise}) {
    GenerateSignal(input.as_span(), signal);
    frame.DoFFT(input.as_span());
    frame.DoInverseFFT(output.as_span());

    for (size_t i = 0; i < fft_size; ++i) {
      EXPECT_NEAR(input[i], output[i], 1e-5)
          << "Mismatch at index " << i << " for size " << fft_size
          << " and signal type " << static_cast<int>(signal);
    }
  }
}

std::vector<TestParams> GetEvenTestParams() {
  return {
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
  };
}

INSTANTIATE_TEST_SUITE_P(
    All,
    FFTFrameParameterSizeTest,
    testing::ValuesIn(GetEvenTestParams()),
    [](const testing::TestParamInfo<FFTFrameParameterSizeTest::ParamType>&
           info) { return info.param.description; });

}  // namespace

}  // namespace blink
