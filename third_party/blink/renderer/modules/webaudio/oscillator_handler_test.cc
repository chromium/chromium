// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/oscillator_handler.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_periodic_wave_options.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/modules/webaudio/audio_param.h"
#include "third_party/blink/renderer/modules/webaudio/offline_audio_context.h"
#include "third_party/blink/renderer/modules/webaudio/oscillator_node.h"
#include "third_party/blink/renderer/modules/webaudio/periodic_wave.h"
#include "third_party/blink/renderer/modules/webaudio/periodic_wave_handler.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

TEST(OscillatorHandlerTest, RoundingBug) {
  test::TaskEnvironment task_environment;
  auto page = std::make_unique<DummyPageHolder>();
  OfflineAudioContext* context = OfflineAudioContext::Create(
      page->GetFrame().DomWindow(), 1, 128, 44100, ASSERT_NO_EXCEPTION);

  OscillatorNode* node =
      OscillatorNode::Create(*context, "sine", nullptr, ASSERT_NO_EXCEPTION);
  OscillatorHandler& handler = node->GetOscillatorHandler();

  unsigned periodic_wave_size = handler.periodic_wave_->PeriodicWaveSize();

  // Problematic index: just below periodic_wave_size. 4096 - 1e-11 rounds to
  // 4096.0f as a float.  We use a value that is known to round up to the next
  // power of 2.
  double virtual_read_index = periodic_wave_size - 1e-11;

  constexpr size_t kSize = 4;
  std::array<float, kSize> destination;
  std::array<float, kSize> phase_increments = {1.0f, 1.0f, 1.0f, 1.0f};

  handler.ProcessARate(kSize, destination, virtual_read_index,
                       phase_increments);

  for (int i = 0; i < kSize; ++i) {
    EXPECT_TRUE(std::isfinite(destination[i]));
    EXPECT_LE(std::abs(destination[i]), 1.1f)
        << "Sample " << i << " is too large: " << destination[i]
        << " (possible rounding bug)";
  }
}

TEST(OscillatorHandlerTest, NegativeIncrementRoundingBug) {
  test::TaskEnvironment task_environment;
  auto page = std::make_unique<DummyPageHolder>();
  OfflineAudioContext* context = OfflineAudioContext::Create(
      page->GetFrame().DomWindow(), 1, 128, 44100, ASSERT_NO_EXCEPTION);

  OscillatorNode* node =
      OscillatorNode::Create(*context, "sine", nullptr, ASSERT_NO_EXCEPTION);
  OscillatorHandler& handler = node->GetOscillatorHandler();

  // After adding phase_increments[0] = -1.0f, the second sample's unwrapped
  // virtual_read_index is -1e-13, which wraps via
  // x - floor(x / 4096.0) * 4096.0 to 4096.0 in double precision (testing
  // ProcessARateVectorKernel on sample 1). With phase_increments[3] = 2.0f,
  // incr_sum[3] is also -1.0, so ProcessARateVectorKernel returns 4096.0 to
  // ProcessARateScalar (testing DoInterpolation on sample 4).
  double virtual_read_index = 1.0 - 1e-13;

  constexpr size_t kSize = 5;
  std::array<float, kSize> destination;
  std::array<float, kSize> phase_increments = {-1.0f, -1.0f, -1.0f, 2.0f,
                                               -1.0f};

  handler.ProcessARate(kSize, destination, virtual_read_index,
                       phase_increments);

  for (size_t i = 0; i < kSize; ++i) {
    EXPECT_TRUE(std::isfinite(destination[i]));
    EXPECT_LE(std::abs(destination[i]), 1.1f)
        << "Sample " << i << " is too large: " << destination[i]
        << " (possible rounding bug)";
  }
}

TEST(OscillatorHandlerTest, SetInitialType) {
  test::TaskEnvironment task_environment;
  auto page = std::make_unique<DummyPageHolder>();
  OfflineAudioContext* context = OfflineAudioContext::Create(
      page->GetFrame().DomWindow(), 1, 128, 44100, ASSERT_NO_EXCEPTION);

  struct TypeTestCase {
    const char* type_string;
    V8OscillatorType::Enum expected_enum;
  };
  constexpr std::array<TypeTestCase, 4> kTestCases = {{
      {"sine", V8OscillatorType::Enum::kSine},
      {"square", V8OscillatorType::Enum::kSquare},
      {"sawtooth", V8OscillatorType::Enum::kSawtooth},
      {"triangle", V8OscillatorType::Enum::kTriangle},
  }};

  for (const auto& test_case : kTestCases) {
    OscillatorNode* node = OscillatorNode::Create(
        *context, test_case.type_string, nullptr, ASSERT_NO_EXCEPTION);
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->type().AsEnum(), test_case.expected_enum);
  }
}

TEST(OscillatorHandlerTest, PeriodicWaveOptions) {
  test::TaskEnvironment task_environment;
  auto page = std::make_unique<DummyPageHolder>();
  OfflineAudioContext* context = OfflineAudioContext::Create(
      page->GetFrame().DomWindow(), 1, 128, 44100, ASSERT_NO_EXCEPTION);

  // Neither `real` nor `imag` provided.
  auto* default_options = PeriodicWaveOptions::Create();
  PeriodicWave* default_wave =
      PeriodicWave::Create(context, default_options, ASSERT_NO_EXCEPTION);
  ASSERT_NE(default_wave, nullptr);
  ASSERT_NE(default_wave->handler(), nullptr);

  // Only `real` provided.
  auto* real_only_options = PeriodicWaveOptions::Create();
  real_only_options->setReal({0.0f, 1.0f});
  PeriodicWave* real_only_wave =
      PeriodicWave::Create(context, real_only_options, ASSERT_NO_EXCEPTION);
  ASSERT_NE(real_only_wave, nullptr);
  ASSERT_NE(real_only_wave->handler(), nullptr);

  // Only `imag` provided.
  auto* imag_only_options = PeriodicWaveOptions::Create();
  imag_only_options->setImag({0.0f, 1.0f});
  PeriodicWave* imag_only_wave =
      PeriodicWave::Create(context, imag_only_options, ASSERT_NO_EXCEPTION);
  ASSERT_NE(imag_only_wave, nullptr);
  ASSERT_NE(imag_only_wave->handler(), nullptr);

  // Both `real` and `imag` provided.
  auto* both_options = PeriodicWaveOptions::Create();
  both_options->setReal({0.0f, 0.5f});
  both_options->setImag({0.0f, 0.5f});
  PeriodicWave* both_wave =
      PeriodicWave::Create(context, both_options, ASSERT_NO_EXCEPTION);
  ASSERT_NE(both_wave, nullptr);
  ASSERT_NE(both_wave->handler(), nullptr);

  OscillatorNode* node = OscillatorNode::Create(
      *context, "custom", imag_only_wave, ASSERT_NO_EXCEPTION);
  ASSERT_NE(node, nullptr);
  EXPECT_EQ(node->type().AsEnum(), V8OscillatorType::Enum::kCustom);
}
}  // namespace blink
