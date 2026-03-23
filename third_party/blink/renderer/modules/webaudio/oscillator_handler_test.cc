// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/oscillator_handler.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/modules/webaudio/audio_param.h"
#include "third_party/blink/renderer/modules/webaudio/offline_audio_context.h"
#include "third_party/blink/renderer/modules/webaudio/oscillator_node.h"
#include "third_party/blink/renderer/modules/webaudio/periodic_wave.h"
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

}  // namespace blink
