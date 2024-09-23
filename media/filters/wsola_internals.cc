// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/filters/wsola_internals.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <memory>
#include <numbers>

#include "base/check_op.h"
#include "build/build_config.h"
#include "media/base/audio_bus.h"

#if defined(ARCH_CPU_X86_FAMILY)
#define USE_SIMD 1
#include <xmmintrin.h>
#elif defined(ARCH_CPU_ARM_FAMILY) && defined(USE_NEON)
#define USE_SIMD 1
#include <arm_neon.h>
#endif

namespace media {

namespace internal {

bool InInterval(int n, Interval q) {
  return n >= q.first && n <= q.second;
}

float MultiChannelSimilarityMeasure(const float* dot_prod_a_b,
                                    const float* energy_a,
                                    const float* energy_b,
                                    int channels) {
  const float kEpsilon = 1e-12f;
  float similarity_measure = 0.0f;
  for (int n = 0; n < channels; ++n) {
    similarity_measure +=
        dot_prod_a_b[n] / std::sqrt(energy_a[n] * energy_b[n] + kEpsilon);
  }
  return similarity_measure;
}

void MultiChannelDotProduct(const AudioBus* a,
                            int frame_offset_a,
                            const AudioBus* b,
                            int frame_offset_b,
                            int num_frames,
                            float* dot_product) {
  DCHECK_EQ(a->channels(), b->channels());
  DCHECK_GE(frame_offset_a, 0);
  DCHECK_GE(frame_offset_b, 0);
  DCHECK_LE(frame_offset_a + num_frames, a->frames());
  DCHECK_LE(frame_offset_b + num_frames, b->frames());

// SIMD optimized variants can provide a massive speedup to this operation.
#if defined(USE_SIMD)
  const int rem = num_frames % 4;
  const int last_index = num_frames - rem;
  const int channels = a->channels();
  for (int ch = 0; ch < channels; ++ch) {
    const float* a_src = a->channel(ch) + frame_offset_a;
    const float* b_src = b->channel(ch) + frame_offset_b;

#if defined(ARCH_CPU_X86_FAMILY)
    // First sum all components.
    __m128 m_sum = _mm_setzero_ps();
    for (int s = 0; s < last_index; s += 4) {
      m_sum = _mm_add_ps(
          m_sum, _mm_mul_ps(_mm_loadu_ps(a_src + s), _mm_loadu_ps(b_src + s)));
    }

    // Reduce to a single float for this channel. Sadly, SSE1,2 doesn't have a
    // horizontal sum function, so we have to condense manually.
    m_sum = _mm_add_ps(_mm_movehl_ps(m_sum, m_sum), m_sum);
    _mm_store_ss(dot_product + ch,
                 _mm_add_ss(m_sum, _mm_shuffle_ps(m_sum, m_sum, 1)));
#elif defined(ARCH_CPU_ARM_FAMILY)
    // First sum all components.
    float32x4_t m_sum = vmovq_n_f32(0);
    for (int s = 0; s < last_index; s += 4)
      m_sum = vmlaq_f32(m_sum, vld1q_f32(a_src + s), vld1q_f32(b_src + s));

    // Reduce to a single float for this channel.
    float32x2_t m_half = vadd_f32(vget_high_f32(m_sum), vget_low_f32(m_sum));
    dot_product[ch] = vget_lane_f32(vpadd_f32(m_half, m_half), 0);
#endif
  }

  if (!rem)
    return;
  num_frames = rem;
  frame_offset_a += last_index;
  frame_offset_b += last_index;
#else
  memset(dot_product, 0, sizeof(*dot_product) * a->channels());
#endif  // defined(USE_SIMD)

  // C version is required to handle remainder of frames (% 4 != 0)
  for (int k = 0; k < a->channels(); ++k) {
    const float* ch_a = a->channel(k) + frame_offset_a;
    const float* ch_b = b->channel(k) + frame_offset_b;
    for (int n = 0; n < num_frames; ++n)
      dot_product[k] += *ch_a++ * *ch_b++;
  }
}

void MultiChannelMovingBlockEnergies(const AudioBus* input,
                                     int frames_per_block,
                                     float* energy) {
  int num_blocks = input->frames() - (frames_per_block - 1);
  int channels = input->channels();

  for (int k = 0; k < input->channels(); ++k) {
    const float* input_channel = input->channel(k);

    energy[k] = 0;

    // First block of channel |k|.
    for (int m = 0; m < frames_per_block; ++m) {
      energy[k] += input_channel[m] * input_channel[m];
    }

    const float* slide_out = input_channel;
    const float* slide_in = input_channel + frames_per_block;
    for (int n = 1; n < num_blocks; ++n, ++slide_in, ++slide_out) {
      energy[k + n * channels] = energy[k + (n - 1) * channels] - *slide_out *
          *slide_out + *slide_in * *slide_in;
    }
  }
}

// Fit the curve f(x) = a * x^2 + b * x + c such that
//   f(-1) = y[0]
//   f(0) = y[1]
//   f(1) = y[2]
// and return the maximum, assuming that y[0] <= y[1] >= y[2].
void QuadraticInterpolation(const float* y_values,
                            float* extremum,
                            float* extremum_value) {
  float a = 0.5f * (y_values[2] + y_values[0]) - y_values[1];
  float b = 0.5f * (y_values[2] - y_values[0]);
  float c = y_values[1];

  if (a == 0.f) {
    // The coordinates are colinear (within floating-point error).
    *extremum = 0;
    *extremum_value = y_values[1];
  } else {
    *extremum = -b / (2.f * a);
    *extremum_value = a * (*extremum) * (*extremum) + b * (*extremum) + c;
  }
}

int DecimatedSearch(int decimation,
                    Interval exclude_interval,
                    const AudioBus* target_block,
                    const AudioBus* search_segment,
                    const float* energy_target_block,
                    const float* energy_candidate_blocks) {
  int channels = search_segment->channels();
  int block_size = target_block->frames();
  int num_candidate_blocks = search_segment->frames() - (block_size - 1);
  auto dot_prod = std::make_unique<float[]>(channels);
  float similarity[3];  // Three elements for cubic interpolation.

  int n = 0;
  MultiChannelDotProduct(target_block, 0, search_segment, n, block_size,
                         dot_prod.get());
  similarity[0] = MultiChannelSimilarityMeasure(
      dot_prod.get(), energy_target_block,
      &energy_candidate_blocks[n * channels], channels);

  // Set the starting point as optimal point.
  float best_similarity = similarity[0];
  int optimal_index = 0;

  n += decimation;
  if (n >= num_candidate_blocks) {
    return 0;
  }

  MultiChannelDotProduct(target_block, 0, search_segment, n, block_size,
                         dot_prod.get());
  similarity[1] = MultiChannelSimilarityMeasure(
      dot_prod.get(), energy_target_block,
      &energy_candidate_blocks[n * channels], channels);

  n += decimation;
  if (n >= num_candidate_blocks) {
    // We cannot do any more sampling. Compare these two values and return the
    // optimal index.
    return similarity[1] > similarity[0] ? decimation : 0;
  }

  for (; n < num_candidate_blocks; n += decimation) {
    MultiChannelDotProduct(target_block, 0, search_segment, n, block_size,
                           dot_prod.get());

    similarity[2] = MultiChannelSimilarityMeasure(
        dot_prod.get(), energy_target_block,
        &energy_candidate_blocks[n * channels], channels);

    if ((similarity[1] > similarity[0] && similarity[1] >= similarity[2]) ||
        (similarity[1] >= similarity[0] && similarity[1] > similarity[2])) {
      // A local maximum is found. Do a cubic interpolation for a better
      // estimate of candidate maximum.
      float normalized_candidate_index;
      float candidate_similarity;
      QuadraticInterpolation(similarity, &normalized_candidate_index,
                             &candidate_similarity);

      int candidate_index = n - decimation + static_cast<int>(
          normalized_candidate_index * decimation +  0.5f);
      if (candidate_similarity > best_similarity &&
          !InInterval(candidate_index, exclude_interval)) {
        optimal_index = candidate_index;
        best_similarity = candidate_similarity;
      }
    } else if (n + decimation >= num_candidate_blocks &&
               similarity[2] > best_similarity &&
               !InInterval(n, exclude_interval)) {
      // If this is the end-point and has a better similarity-measure than
      // optimal, then we accept it as optimal point.
      optimal_index = n;
      best_similarity = similarity[2];
    }
    memmove(similarity, &similarity[1], 2 * sizeof(*similarity));
  }
  return optimal_index;
}

int FullSearch(int low_limit,
               int high_limit,
               Interval exclude_interval,
               const AudioBus* target_block,
               const AudioBus* search_block,
               const float* energy_target_block,
               const float* energy_candidate_blocks) {
  int channels = search_block->channels();
  int block_size = target_block->frames();
  auto dot_prod = std::make_unique<float[]>(channels);

  float best_similarity = std::numeric_limits<float>::min();
  int optimal_index = 0;

  for (int n = low_limit; n <= high_limit; ++n) {
    if (InInterval(n, exclude_interval)) {
      continue;
    }
    MultiChannelDotProduct(target_block, 0, search_block, n, block_size,
                           dot_prod.get());

    float similarity = MultiChannelSimilarityMeasure(
        dot_prod.get(), energy_target_block,
        &energy_candidate_blocks[n * channels], channels);

    if (similarity > best_similarity) {
      best_similarity = similarity;
      optimal_index = n;
    }
  }

  return optimal_index;
}

int OptimalIndex(const AudioBus* search_block,
                 const AudioBus* target_block,
                 Interval exclude_interval) {
  int channels = search_block->channels();
  DCHECK_EQ(channels, target_block->channels());
  int target_size = target_block->frames();
  int num_candidate_blocks = search_block->frames() - (target_size - 1);

  // This is a compromise between complexity reduction and search accuracy. I
  // don't have a proof that down sample of order 5 is optimal. One can compute
  // a decimation factor that minimizes complexity given the size of
  // |search_block| and |target_block|. However, my experiments show the rate of
  // missing the optimal index is significant. This value is chosen
  // heuristically based on experiments.
  const int kSearchDecimation = 5;

  auto energy_target_block = std::make_unique<float[]>(channels);
  auto energy_candidate_blocks =
      std::make_unique<float[]>(channels * num_candidate_blocks);

  // Energy of all candid frames.
  MultiChannelMovingBlockEnergies(search_block, target_size,
                                  energy_candidate_blocks.get());

  // Energy of target frame.
  MultiChannelDotProduct(target_block, 0, target_block, 0,
                         target_size, energy_target_block.get());

  int optimal_index = DecimatedSearch(kSearchDecimation,
                                      exclude_interval, target_block,
                                      search_block, energy_target_block.get(),
                                      energy_candidate_blocks.get());

  int lim_low = std::max(0, optimal_index - kSearchDecimation);
  int lim_high = std::min(num_candidate_blocks - 1,
                          optimal_index + kSearchDecimation);
  return FullSearch(lim_low, lim_high, exclude_interval, target_block,
                    search_block, energy_target_block.get(),
                    energy_candidate_blocks.get());
}

void GetPeriodicHanningWindow(int window_length, float* window) {
  const float scale = 2.0f * std::numbers::pi_v<float> / window_length;
  for (int n = 0; n < window_length; ++n)
    window[n] = 0.5f * (1.0f - std::cos(n * scale));
}

}  // namespace internal

}  // namespace media

