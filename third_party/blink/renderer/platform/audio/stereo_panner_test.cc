// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/audio/stereo_panner.h"

#include <algorithm>
#include <array>
#include <cmath>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/audio/audio_bus.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

TEST(StereoPannerTest, MonoInputPanToTargetValue) {
  constexpr unsigned kFrames = 128;
  auto input_bus = AudioBus::Create(1, kFrames);
  auto output_bus = AudioBus::Create(2, kFrames);

  std::ranges::fill(input_bus->Channel(0)->MutableSpan(), 1.0f);

  StereoPanner panner(48000.0f);

  // Pan hard left (-1.0)
  panner.PanToTargetValue(input_bus.get(), output_bus.get(), -1.0f, kFrames);
  EXPECT_NEAR(output_bus->Channel(0)->Span()[0], 1.0f, 1e-5f);
  EXPECT_NEAR(output_bus->Channel(1)->Span()[0], 0.0f, 1e-5f);

  // Pan center (0.0)
  panner.PanToTargetValue(input_bus.get(), output_bus.get(), 0.0f, kFrames);
  const float center_gain =
      static_cast<float>(std::cos(kPiOverTwoDouble * 0.5));
  EXPECT_NEAR(output_bus->Channel(0)->Span()[0], center_gain, 1e-5f);
  EXPECT_NEAR(output_bus->Channel(1)->Span()[0], center_gain, 1e-5f);

  // Pan hard right (1.0)
  panner.PanToTargetValue(input_bus.get(), output_bus.get(), 1.0f, kFrames);
  EXPECT_NEAR(output_bus->Channel(0)->Span()[0], 0.0f, 1e-5f);
  EXPECT_NEAR(output_bus->Channel(1)->Span()[0], 1.0f, 1e-5f);
}

TEST(StereoPannerTest, StereoInputPanToTargetValue) {
  constexpr unsigned kFrames = 128;
  auto input_bus = AudioBus::Create(2, kFrames);
  auto output_bus = AudioBus::Create(2, kFrames);

  std::ranges::fill(input_bus->Channel(0)->MutableSpan(), 1.0f);
  std::ranges::fill(input_bus->Channel(1)->MutableSpan(), 2.0f);

  StereoPanner panner(48000.0f);

  // Pan hard left (-1.0): left channel = input_l + input_r * cos(0) = 3.0
  panner.PanToTargetValue(input_bus.get(), output_bus.get(), -1.0f, kFrames);
  EXPECT_NEAR(output_bus->Channel(0)->Span()[0], 3.0f, 1e-5f);
  EXPECT_NEAR(output_bus->Channel(1)->Span()[0], 0.0f, 1e-5f);

  // Pan center (0.0): left = 1.0, right = 2.0
  panner.PanToTargetValue(input_bus.get(), output_bus.get(), 0.0f, kFrames);
  EXPECT_NEAR(output_bus->Channel(0)->Span()[0], 1.0f, 1e-5f);
  EXPECT_NEAR(output_bus->Channel(1)->Span()[0], 2.0f, 1e-5f);

  // Pan hard right (1.0): right channel = input_r + input_l * sin(pi/2) = 3.0
  panner.PanToTargetValue(input_bus.get(), output_bus.get(), 1.0f, kFrames);
  EXPECT_NEAR(output_bus->Channel(0)->Span()[0], 0.0f, 1e-5f);
  EXPECT_NEAR(output_bus->Channel(1)->Span()[0], 3.0f, 1e-5f);
}

TEST(StereoPannerTest, PanWithSampleAccurateValues) {
  constexpr std::array<float, 3> kPanValues = {-1.0f, 0.0f, 1.0f};
  constexpr unsigned kFrames = kPanValues.size();
  auto mono_input = AudioBus::Create(1, kFrames);
  auto stereo_input = AudioBus::Create(2, kFrames);
  auto output_bus = AudioBus::Create(2, kFrames);

  std::ranges::fill(mono_input->Channel(0)->MutableSpan(), 1.0f);
  std::ranges::fill(stereo_input->Channel(0)->MutableSpan(), 1.0f);
  std::ranges::fill(stereo_input->Channel(1)->MutableSpan(), 2.0f);

  StereoPanner panner(48000.0f);

  // Mono input: hard left (-1.0), center (0.0), hard right (1.0).
  panner.PanWithSampleAccurateValues(mono_input.get(), output_bus.get(),
                                     kPanValues);
  const float center_gain =
      static_cast<float>(std::cos(kPiOverTwoDouble * 0.5));
  EXPECT_NEAR(output_bus->Channel(0)->Span()[0], 1.0f, 1e-5f);
  EXPECT_NEAR(output_bus->Channel(1)->Span()[0], 0.0f, 1e-5f);
  EXPECT_NEAR(output_bus->Channel(0)->Span()[1], center_gain, 1e-5f);
  EXPECT_NEAR(output_bus->Channel(1)->Span()[1], center_gain, 1e-5f);
  EXPECT_NEAR(output_bus->Channel(0)->Span()[2], 0.0f, 1e-5f);
  EXPECT_NEAR(output_bus->Channel(1)->Span()[2], 1.0f, 1e-5f);

  // Stereo input: hard left (-1.0), center (0.0), hard right (1.0).
  panner.PanWithSampleAccurateValues(stereo_input.get(), output_bus.get(),
                                     kPanValues);
  EXPECT_NEAR(output_bus->Channel(0)->Span()[0], 3.0f, 1e-5f);
  EXPECT_NEAR(output_bus->Channel(1)->Span()[0], 0.0f, 1e-5f);
  EXPECT_NEAR(output_bus->Channel(0)->Span()[1], 1.0f, 1e-5f);
  EXPECT_NEAR(output_bus->Channel(1)->Span()[1], 2.0f, 1e-5f);
  EXPECT_NEAR(output_bus->Channel(0)->Span()[2], 0.0f, 1e-5f);
  EXPECT_NEAR(output_bus->Channel(1)->Span()[2], 3.0f, 1e-5f);
}

TEST(StereoPannerTest, InPlaceProcessing) {
  constexpr unsigned kFrames = 128;
  auto bus = AudioBus::Create(2, kFrames);
  std::ranges::fill(bus->Channel(0)->MutableSpan(), 1.0f);
  std::ranges::fill(bus->Channel(1)->MutableSpan(), 2.0f);

  StereoPanner panner(48000.0f);
  panner.PanToTargetValue(bus.get(), bus.get(), -1.0f, kFrames);

  EXPECT_NEAR(bus->Channel(0)->Span()[0], 3.0f, 1e-5f);
  EXPECT_NEAR(bus->Channel(1)->Span()[0], 0.0f, 1e-5f);

  // Also verify the target_pan > 0 branch for in-place aliasing.
  std::ranges::fill(bus->Channel(0)->MutableSpan(), 1.0f);
  std::ranges::fill(bus->Channel(1)->MutableSpan(), 2.0f);
  panner.PanToTargetValue(bus.get(), bus.get(), 1.0f, kFrames);

  EXPECT_NEAR(bus->Channel(0)->Span()[0], 0.0f, 1e-5f);
  EXPECT_NEAR(bus->Channel(1)->Span()[0], 3.0f, 1e-5f);
}

}  // namespace blink
