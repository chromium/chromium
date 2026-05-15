// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/audio/direct_convolver.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/audio/audio_array.h"

namespace blink {

namespace {

void TestDirectConvolverImpulse(size_t input_block_size, size_t kernel_size) {
  auto kernel = std::make_unique<AudioFloatArray>(kernel_size);
  for (size_t i = 0; i < kernel_size; ++i) {
    (*kernel)[i] = static_cast<float>(i + 1);
  }

  DirectConvolver convolver(input_block_size, std::move(kernel));

  AudioFloatArray source(input_block_size);
  AudioFloatArray dest(input_block_size);

  // Total frames to verify: at least kernel_size + input_block_size
  size_t total_frames_to_process = kernel_size + input_block_size;
  size_t frames_processed = 0;

  bool first_block = true;
  while (frames_processed < total_frames_to_process) {
    source.Zero();
    if (first_block) {
      // Impulse
      source[0] = 1.0f;
      first_block = false;
    }

    convolver.Process(source.as_span(), dest.as_span());

    for (size_t i = 0; i < input_block_size; ++i) {
      size_t frame_index = frames_processed + i;
      float expected = 0.0f;
      if (frame_index < kernel_size) {
        expected = static_cast<float>(frame_index + 1);
      }
      EXPECT_NEAR(dest[i], expected, 1e-6f) << "at frame " << frame_index;
    }

    frames_processed += input_block_size;
  }
}

}  // namespace

TEST(DirectConvolverTest, KernelSmallerThanBlock) {
  TestDirectConvolverImpulse(128, 32);
  TestDirectConvolverImpulse(128, 64);
}

TEST(DirectConvolverTest, KernelEqualToBlock) {
  TestDirectConvolverImpulse(128, 128);
}

TEST(DirectConvolverTest, KernelGreaterThanBlock) {
  TestDirectConvolverImpulse(32, 128);
  TestDirectConvolverImpulse(64, 128);
}

}  // namespace blink
