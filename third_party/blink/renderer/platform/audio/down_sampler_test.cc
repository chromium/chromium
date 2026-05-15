// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/audio/down_sampler.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/audio/audio_array.h"

namespace blink {

namespace {

void TestDownSampler(unsigned block_size) {
  DownSampler sampler(block_size);
  EXPECT_GT(sampler.LatencyFrames(), 0u);

  AudioFloatArray source(block_size);
  AudioFloatArray dest(block_size / 2);

  // Fill source with DC = 1.0
  for (unsigned i = 0; i < block_size; ++i) {
    source[i] = 1.0f;
  }

  // Process several blocks to let the FIR filter settle.
  unsigned blocks_to_process = std::max(4u, 512u / block_size);
  for (unsigned i = 0; i < blocks_to_process; ++i) {
    sampler.Process(source.as_span(), dest.as_span());
  }

  // Verify output settled to near 1.0 (DC gain of half-band filter)
  for (unsigned i = 0; i < dest.size(); ++i) {
    EXPECT_NEAR(dest[i], 1.0f, 1e-6f);
  }

  sampler.Reset();
}

}  // namespace

TEST(DownSamplerTest, DirectConvolver) {
  TestDownSampler(32);
  TestDownSampler(64);
  TestDownSampler(128);
  TestDownSampler(256);
}

TEST(DownSamplerTest, SimpleFFTConvolver) {
  TestDownSampler(512);
  TestDownSampler(1024);
  TestDownSampler(2048);
}

}  // namespace blink
