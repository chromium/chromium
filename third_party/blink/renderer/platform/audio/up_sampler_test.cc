// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/audio/up_sampler.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/audio/audio_array.h"

namespace blink {

namespace {

void TestUpSampler(unsigned block_size) {
  UpSampler sampler(block_size);
  EXPECT_GT(sampler.LatencyFrames(), 0u);

  AudioFloatArray source(block_size);
  AudioFloatArray dest(block_size * 2);

  // Fill source with DC = 1.0
  for (unsigned i = 0; i < block_size; ++i) {
    source[i] = 1.0f;
  }

  // Process several blocks to let the FIR filter settle.
  unsigned blocks_to_process = std::max(4u, 512u / block_size);
  for (unsigned i = 0; i < blocks_to_process; ++i) {
    sampler.Process(source.as_span(), dest.as_span());
  }

  // Verify output settled to near 1.0 (DC gain of interpolation filter)
  for (unsigned i = 0; i < dest.size(); ++i) {
    EXPECT_NEAR(dest[i], 1.0f, 1e-6f);
  }

  sampler.Reset();
}

}  // namespace

TEST(UpSamplerTest, DirectConvolver) {
  TestUpSampler(32);
  TestUpSampler(64);
  TestUpSampler(128);
}

TEST(UpSamplerTest, SimpleFFTConvolver) {
  TestUpSampler(256);
  TestUpSampler(512);
  TestUpSampler(1024);
  TestUpSampler(2048);
}

}  // namespace blink
