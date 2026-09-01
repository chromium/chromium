// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/audio/reverb_convolver.h"

#include <cmath>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/audio/audio_bus.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"

namespace blink {

TEST(ReverbConvolverTest, CustomRenderSliceSize) {
  base::test::TaskEnvironment task_environment;
  ScopedTestingPlatformSupport<TestingPlatformSupport> platform;

  const size_t impulse_response_length = 1024;
  const unsigned max_fft_size = 512;
  const float scale = 1.0f;

  scoped_refptr<AudioBus> impulse_response =
      AudioBus::Create(1, impulse_response_length);
  impulse_response->Zero();

  for (size_t convolver_render_phase : {0u, 64u, 128u}) {
    for (unsigned render_slice_size :
         {16u, 32u, 64u, 100u, 128u, 256u, 512u, 1025u}) {
      std::unique_ptr<ReverbConvolver> convolver =
          ReverbConvolver::TryCreate(impulse_response->Channel(0),
                                     render_slice_size, max_fft_size,
                                     convolver_render_phase, scale);
      ASSERT_NE(convolver, nullptr);

      scoped_refptr<AudioBus> source_bus =
          AudioBus::Create(1, render_slice_size);
      scoped_refptr<AudioBus> destination_bus =
          AudioBus::Create(1, render_slice_size);
      source_bus->Zero();
      destination_bus->Zero();

      for (int i = 0; i < 2; ++i) {
        convolver->Process(source_bus->Channel(0), destination_bus->Channel(0),
                           render_slice_size);
      }
    }
  }
}

TEST(ReverbConvolverTest, DiracImpulseResponse) {
  base::test::TaskEnvironment task_environment;
  ScopedTestingPlatformSupport<TestingPlatformSupport> platform;

  const size_t impulse_response_length = 1024;
  const unsigned max_fft_size = 512;
  const float scale = 1.0f;

  scoped_refptr<AudioBus> impulse_response =
      AudioBus::Create(1, impulse_response_length);
  impulse_response->Zero();
  // Dirac delta impulse at t = 0: h[0] = 1.0.
  impulse_response->Channel(0)->MutableSpan()[0] = 1.0f;

  for (size_t convolver_render_phase : {0u, 128u, 256u}) {
    for (unsigned render_slice_size : {64u, 128u, 256u, 512u}) {
      std::unique_ptr<ReverbConvolver> convolver =
          ReverbConvolver::TryCreate(impulse_response->Channel(0),
                                     render_slice_size, max_fft_size,
                                     convolver_render_phase, scale);
      ASSERT_NE(convolver, nullptr);

      scoped_refptr<AudioBus> source_bus =
          AudioBus::Create(1, render_slice_size);
      scoped_refptr<AudioBus> destination_bus =
          AudioBus::Create(1, render_slice_size);

      size_t sample_index = 0;
      for (int chunk = 0; chunk < 4; ++chunk) {
        auto source_span = source_bus->Channel(0)->MutableSpan();
        for (unsigned i = 0; i < render_slice_size; ++i) {
          source_span[i] = std::sin(static_cast<float>(sample_index++) * 0.05f);
        }

        destination_bus->Zero();
        convolver->Process(source_bus->Channel(0), destination_bus->Channel(0),
                           render_slice_size);

        auto dest_span = destination_bus->Channel(0)->Span();
        for (unsigned i = 0; i < render_slice_size; ++i) {
          // Output must match input identically with zero latency.
          EXPECT_NEAR(dest_span[i], source_span[i], 1e-4f);
        }
      }
    }
  }
}

TEST(ReverbConvolverTest, DelayedImpulseResponse) {
  base::test::TaskEnvironment task_environment;
  ScopedTestingPlatformSupport<TestingPlatformSupport> platform;

  const size_t impulse_response_length = 1024;
  const unsigned max_fft_size = 512;
  const float scale = 1.0f;
  const size_t delay_frames = 256;

  scoped_refptr<AudioBus> impulse_response =
      AudioBus::Create(1, impulse_response_length);
  impulse_response->Zero();
  // Delayed delta impulse: h[delay_frames] = 1.0.
  impulse_response->Channel(0)->MutableSpan()[delay_frames] = 1.0f;

  for (unsigned render_slice_size : {64u, 128u, 256u}) {
    std::unique_ptr<ReverbConvolver> convolver = ReverbConvolver::TryCreate(
        impulse_response->Channel(0), render_slice_size, max_fft_size, 0,
        scale);
    ASSERT_NE(convolver, nullptr);

    scoped_refptr<AudioBus> source_bus = AudioBus::Create(1, render_slice_size);
    scoped_refptr<AudioBus> destination_bus =
        AudioBus::Create(1, render_slice_size);

    constexpr int kChunks = 6;
    const size_t total_frames = kChunks * render_slice_size;
    std::vector<float> all_inputs;
    std::vector<float> all_outputs;
    all_inputs.reserve(total_frames);
    all_outputs.reserve(total_frames);

    size_t sample_index = 0;
    for (int chunk = 0; chunk < kChunks; ++chunk) {
      auto source_span = source_bus->Channel(0)->MutableSpan();
      for (unsigned i = 0; i < render_slice_size; ++i) {
        float val = std::sin(static_cast<float>(sample_index++) * 0.05f);
        source_span[i] = val;
        all_inputs.push_back(val);
      }

      destination_bus->Zero();
      convolver->Process(source_bus->Channel(0), destination_bus->Channel(0),
                         render_slice_size);

      auto dest_span = destination_bus->Channel(0)->Span();
      for (unsigned i = 0; i < render_slice_size; ++i) {
        all_outputs.push_back(dest_span[i]);
      }
    }

    // Check that output is delayed by exactly `delay_frames`.
    for (size_t i = 0; i < delay_frames; ++i) {
      EXPECT_NEAR(all_outputs[i], 0.0f, 1e-4f);
    }
    for (size_t i = delay_frames; i < all_outputs.size(); ++i) {
      EXPECT_NEAR(all_outputs[i], all_inputs[i - delay_frames], 1e-4f);
    }
  }
}

}  // namespace blink
