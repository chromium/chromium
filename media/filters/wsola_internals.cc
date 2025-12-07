// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/wsola_internals.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <numbers>
#include <numeric>

#include "base/check_op.h"
#include "base/containers/auto_spanification_helper.h"
#include "base/containers/span.h"
#include "base/cpu.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "build/build_config.h"
#include "media/base/audio_bus.h"

#if defined(ARCH_CPU_X86_FAMILY)
#include <immintrin.h>
#include <xmmintrin.h>
#elif defined(ARCH_CPU_ARM_FAMILY) && defined(USE_NEON)
#include <arm_neon.h>
#endif

namespace media::internal {

namespace {

bool InInterval(size_t n, Interval q) {
  return n >= q.first && n <= q.second;
}

float MultiChannelSimilarityMeasure(base::span<const float> dot_prod_a_b,
                                    base::span<const float> energy_a,
                                    base::span<const float> energy_b) {
  CHECK_EQ(dot_prod_a_b.size(), energy_a.size());
  CHECK_EQ(energy_a.size(), energy_b.size());
  const float kEpsilon = 1e-12f;
  float similarity_measure = 0.0f;
  for (size_t n = 0; n < dot_prod_a_b.size(); ++n) {
    similarity_measure +=
        dot_prod_a_b[n] / std::sqrt(energy_a[n] * energy_b[n] + kEpsilon);
  }
  return similarity_measure;
}

#if defined(ARCH_CPU_X86_FAMILY)
void MultiChannelDotProduct_SSE(const AudioBus* a,
                                size_t frame_offset_a,
                                const AudioBus* b,
                                size_t frame_offset_b,
                                size_t num_frames,
                                base::span<float> dot_product) {
  CHECK_EQ(dot_product.size(), static_cast<size_t>(a->channels()));
  // SIMD optimized variants can provide a massive speedup to this operation.
  const size_t rem = num_frames % 4;
  const size_t last_index = num_frames - rem;
  const int channels = a->channels();
  for (int ch = 0; ch < channels; ++ch) {
    base::span<const float> a_src = a->channel_span(ch).subspan(frame_offset_a);
    base::span<const float> b_src = b->channel_span(ch).subspan(frame_offset_b);

    // First sum all components.
    __m128 m_sum = _mm_setzero_ps();
    for (size_t s = 0; s < last_index; s += 4) {
      m_sum = _mm_add_ps(
          m_sum, _mm_mul_ps(_mm_loadu_ps(&a_src[s]), _mm_loadu_ps(&b_src[s])));
    }

    // Reduce to a single float for this channel. Sadly, SSE1,2 doesn't have a
    // horizontal sum function, so we have to condense manually.
    m_sum = _mm_add_ps(_mm_movehl_ps(m_sum, m_sum), m_sum);
    _mm_store_ss(&dot_product[ch],
                 _mm_add_ss(m_sum, _mm_shuffle_ps(m_sum, m_sum, 1)));
  }

  if (!rem) {
    return;
  }

  frame_offset_a += last_index;
  frame_offset_b += last_index;

  // C version is required to handle remainder of frames (% 4 != 0)
  for (int ch = 0; ch < channels; ++ch) {
    base::span<const float> ch_a =
        a->channel_span(ch).subspan(frame_offset_a, rem);
    base::span<const float> ch_b =
        b->channel_span(ch).subspan(frame_offset_b, rem);
    dot_product[ch] +=
        std::inner_product(ch_a.begin(), ch_a.end(), ch_b.begin(), 0.0f);
  }
}

__attribute__((target("avx2,fma"))) void MultiChannelDotProduct_AVX2(
    const AudioBus* a,
    size_t frame_offset_a,
    const AudioBus* b,
    size_t frame_offset_b,
    size_t num_frames,
    base::span<float> dot_product) {
  CHECK_EQ(dot_product.size(), static_cast<size_t>(a->channels()));
  // SIMD optimized variants can provide a massive speedup to this operation.
  const size_t rem = num_frames % 8;
  const size_t last_index = num_frames - rem;
  const int channels = a->channels();
  for (int ch = 0; ch < channels; ++ch) {
    base::span<const float> a_src = a->channel_span(ch).subspan(frame_offset_a);
    base::span<const float> b_src = b->channel_span(ch).subspan(frame_offset_b);

    // First sum all components using FMA.
    __m256 m_sum = _mm256_setzero_ps();
    for (size_t s = 0; s < last_index; s += 8) {
      __m256 a_avx = _mm256_loadu_ps(&a_src[s]);
      __m256 b_avx = _mm256_loadu_ps(&b_src[s]);
      m_sum = _mm256_fmadd_ps(a_avx, b_avx, m_sum);
    }

    // Horizontal add thrice to reduce 8 floats to 4, then 2, then 1.
    // Note: the following could be reduced to a single _mm256_reduce_add_ps(),
    // on CPUs with AVX512 support.
    m_sum = _mm256_hadd_ps(m_sum, m_sum);
    m_sum = _mm256_hadd_ps(m_sum, m_sum);
    m_sum = _mm256_hadd_ps(m_sum, m_sum);

    dot_product[ch] = _mm256_cvtss_f32(m_sum);
  }

  if (!rem) {
    return;
  }

  frame_offset_a += last_index;
  frame_offset_b += last_index;

  // C version is required to handle remainder of frames (% 8 != 0)
  for (int ch = 0; ch < channels; ++ch) {
    base::span<const float> ch_a =
        a->channel_span(ch).subspan(frame_offset_a, rem);
    base::span<const float> ch_b =
        b->channel_span(ch).subspan(frame_offset_b, rem);
    dot_product[ch] +=
        std::inner_product(ch_a.begin(), ch_a.end(), ch_b.begin(), 0.0f);
  }
}

#elif defined(ARCH_CPU_ARM_FAMILY) && defined(USE_NEON)
void MultiChannelDotProduct_NEON(const AudioBus* a,
                                 size_t frame_offset_a,
                                 const AudioBus* b,
                                 size_t frame_offset_b,
                                 size_t num_frames,
                                 base::span<float> dot_product) {
  CHECK_EQ(dot_product.size(), static_cast<size_t>(a->channels()));
  // SIMD optimized variants can provide a massive speedup to this operation.
  const size_t rem = num_frames % 4;
  const size_t last_index = num_frames - rem;
  const int channels = a->channels();
  for (int ch = 0; ch < channels; ++ch) {
    base::span<const float> a_src = a->channel_span(ch).subspan(frame_offset_a);
    base::span<const float> b_src = b->channel_span(ch).subspan(frame_offset_b);

    // First sum all components.
    float32x4_t m_sum = vmovq_n_f32(0);
    for (size_t s = 0; s < last_index; s += 4) {
      m_sum = vmlaq_f32(m_sum, vld1q_f32(&a_src[s]), vld1q_f32(&b_src[s]));
    }

    // Reduce to a single float for this channel.
    float32x2_t m_half = vadd_f32(vget_high_f32(m_sum), vget_low_f32(m_sum));
    dot_product[ch] = vget_lane_f32(vpadd_f32(m_half, m_half), 0);
  }

  if (!rem) {
    return;
  }

  frame_offset_a += last_index;
  frame_offset_b += last_index;

  // C version is required to handle remainder of frames (% 4 != 0)
  for (int ch = 0; ch < channels; ++ch) {
    base::span<const float> ch_a =
        a->channel_span(ch).subspan(frame_offset_a, rem);
    base::span<const float> ch_b =
        b->channel_span(ch).subspan(frame_offset_b, rem);
    dot_product[ch] +=
        std::inner_product(ch_a.begin(), ch_a.end(), ch_b.begin(), 0.0f);
  }
}
#else

void MultiChannelDotProduct_C(const AudioBus* a,
                              size_t frame_offset_a,
                              const AudioBus* b,
                              size_t frame_offset_b,
                              size_t num_frames,
                              base::span<float> dot_product) {
  CHECK_EQ(dot_product.size(), static_cast<size_t>(a->channels()));
  // Zero out the memory we will accumulate into.
  std::fill(dot_product.begin(), dot_product.end(), 0.0f);

  for (int k = 0; k < a->channels(); ++k) {
    auto ch_a = a->channel_span(k).subspan(frame_offset_a, num_frames);
    auto ch_b = b->channel_span(k).subspan(frame_offset_b, num_frames);
    dot_product[k] =
        std::inner_product(ch_a.begin(), ch_a.end(), ch_b.begin(), 0.0f);
  }
}
#endif

}  // namespace

void MultiChannelDotProduct(const AudioBus* a,
                            size_t frame_offset_a,
                            const AudioBus* b,
                            size_t frame_offset_b,
                            size_t num_frames,
                            base::span<float> dot_product) {
  DCHECK_EQ(a->channels(), b->channels());
  DCHECK_EQ(dot_product.size(), static_cast<size_t>(a->channels()));
  DCHECK_LE(frame_offset_a + num_frames, static_cast<size_t>(a->frames()));
  DCHECK_LE(frame_offset_b + num_frames, static_cast<size_t>(b->frames()));

  static const auto dot_product_func = [] {
#if defined(ARCH_CPU_X86_FAMILY)
    base::CPU cpu;
    if (cpu.has_avx2() && cpu.has_fma3()) {
      return MultiChannelDotProduct_AVX2;
    }
    return MultiChannelDotProduct_SSE;
#elif defined(ARCH_CPU_ARM_FAMILY) && defined(USE_NEON)
    return MultiChannelDotProduct_NEON;
#else
    return MultiChannelDotProduct_C;
#endif
  }();

  return dot_product_func(a, frame_offset_a, b, frame_offset_b, num_frames,
                          dot_product);
}

void MultiChannelMovingBlockEnergies(const AudioBus* input,
                                     size_t frames_per_block,
                                     base::span<float> energy) {
  const size_t num_blocks =
      base::CheckSub(input->frames(), (frames_per_block - 1))
          .Cast<size_t>()
          .ValueOrDie();
  const size_t channels = static_cast<size_t>(input->channels());
  CHECK_EQ(energy.size(), num_blocks * channels);

  for (int k = 0; k < input->channels(); ++k) {
    base::span<const float> input_channel = input->channel_span(k);

    auto first_block = input_channel.first(frames_per_block);
    energy[k] = std::inner_product(first_block.begin(), first_block.end(),
                                   first_block.begin(), 0.0f);

    auto slide_out = input_channel.begin();
    auto slide_in = input_channel.subspan(frames_per_block).begin();
    for (size_t n = 1; n < num_blocks; ++n, ++slide_in, ++slide_out) {
      energy[k + n * channels] = energy[k + (n - 1) * channels] -
                                 (*slide_out) * (*slide_out) +
                                 (*slide_in) * (*slide_in);
    }
  }
}

// Fit the curve f(x) = a * x^2 + b * x + c such that
//   f(-1) = y[0]
//   f(0) = y[1]
//   f(1) = y[2]
// and return the maximum, assuming that y[0] <= y[1] >= y[2].
void QuadraticInterpolation(base::span<const float, 3> y_values,
                            float* extremum,
                            float* extremum_value) {
  const float a = 0.5f * (y_values[2] + y_values[0]) - y_values[1];
  const float b = 0.5f * (y_values[2] - y_values[0]);
  const float c = y_values[1];

  if (a == 0.f) {
    // The coordinates are colinear (within floating-point error).
    *extremum = 0;
    *extremum_value = y_values[1];
  } else {
    const float ext = -b / (2.f * a);
    *extremum = ext;
    *extremum_value = a * ext * ext + b * ext + c;
  }
}

size_t DecimatedSearch(size_t decimation,
                       Interval exclude_interval,
                       const AudioBus* target_block,
                       const AudioBus* search_segment,
                       base::span<const float> energy_target_block,
                       base::span<const float> energy_candidate_blocks) {
  size_t channels = static_cast<size_t>(search_segment->channels());
  size_t block_size = target_block->frames();
  size_t num_candidate_blocks = search_segment->frames() - (block_size - 1);
  std::vector<float> dot_prod(channels);
  float similarity[3];  // Three elements for cubic interpolation.

  size_t n = 0;
  MultiChannelDotProduct(target_block, 0, search_segment, n, block_size,
                         dot_prod);
  similarity[0] = MultiChannelSimilarityMeasure(
      dot_prod, energy_target_block,
      energy_candidate_blocks.subspan(n * channels, channels));

  // Set the starting point as optimal point.
  float best_similarity = similarity[0];
  size_t optimal_index = 0;

  n += decimation;
  if (n >= num_candidate_blocks) {
    return 0;
  }

  MultiChannelDotProduct(target_block, 0, search_segment, n, block_size,
                         dot_prod);
  similarity[1] = MultiChannelSimilarityMeasure(
      dot_prod, energy_target_block,
      energy_candidate_blocks.subspan(n * channels, channels));

  n += decimation;
  if (n >= num_candidate_blocks) {
    // We cannot do any more sampling. Compare these two values and return the
    // optimal index.
    return similarity[1] > similarity[0] ? decimation : 0;
  }

  for (; n < num_candidate_blocks; n += decimation) {
    MultiChannelDotProduct(target_block, 0, search_segment, n, block_size,
                           dot_prod);

    similarity[2] = MultiChannelSimilarityMeasure(
        dot_prod, energy_target_block,
        energy_candidate_blocks.subspan(n * channels, channels));

    if ((similarity[1] > similarity[0] && similarity[1] >= similarity[2]) ||
        (similarity[1] >= similarity[0] && similarity[1] > similarity[2])) {
      // A local maximum is found. Do a cubic interpolation for a better
      // estimate of candidate maximum.
      float normalized_candidate_index;
      float candidate_similarity;
      QuadraticInterpolation(similarity, &normalized_candidate_index,
                             &candidate_similarity);

      int candidate_index =
          n - decimation +
          static_cast<int>(normalized_candidate_index * decimation + 0.5f);
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

    similarity[0] = similarity[1];
    similarity[1] = similarity[2];
  }
  return optimal_index;
}

size_t FullSearch(size_t low_limit,
                  size_t high_limit,
                  Interval exclude_interval,
                  const AudioBus* target_block,
                  const AudioBus* search_block,
                  base::span<const float> energy_target_block,
                  base::span<const float> energy_candidate_blocks) {
  const size_t channels = static_cast<size_t>(search_block->channels());
  const size_t block_size = static_cast<size_t>(target_block->frames());
  std::vector<float> dot_prod(channels);

  float best_similarity = std::numeric_limits<float>::min();
  size_t optimal_index = 0;

  for (size_t n = low_limit; n <= high_limit; ++n) {
    if (InInterval(n, exclude_interval)) {
      continue;
    }
    MultiChannelDotProduct(target_block, 0, search_block, n, block_size,
                           dot_prod);

    float similarity = MultiChannelSimilarityMeasure(
        dot_prod, energy_target_block,
        energy_candidate_blocks.subspan(n * channels, channels));

    if (similarity > best_similarity) {
      best_similarity = similarity;
      optimal_index = n;
    }
  }

  return optimal_index;
}

size_t OptimalIndex(const AudioBus* search_block,
                    const AudioBus* target_block,
                    Interval exclude_interval) {
  DCHECK_EQ(search_block->channels(), target_block->channels());
  const size_t channels = search_block->channels();
  const size_t target_size = target_block->frames();
  const size_t num_candidate_blocks =
      base::CheckSub(search_block->frames(), (target_size - 1))
          .Cast<size_t>()
          .ValueOrDie();

  // This is a compromise between complexity reduction and search accuracy. I
  // don't have a proof that down sample of order 5 is optimal. One can compute
  // a decimation factor that minimizes complexity given the size of
  // |search_block| and |target_block|. However, my experiments show the rate of
  // missing the optimal index is significant. This value is chosen
  // heuristically based on experiments.
  constexpr size_t kSearchDecimation = 5;

  std::vector<float> energy_target_block(channels);
  std::vector<float> energy_candidate_blocks(channels * num_candidate_blocks);

  // Energy of all candid frames.
  MultiChannelMovingBlockEnergies(search_block, target_size,
                                  energy_candidate_blocks);

  // Energy of target frame.
  MultiChannelDotProduct(target_block, 0, target_block, 0, target_size,
                         energy_target_block);

  const size_t optimal_index = DecimatedSearch(
      kSearchDecimation, exclude_interval, target_block, search_block,
      energy_target_block, energy_candidate_blocks);

  const size_t lim_low = optimal_index < kSearchDecimation
                             ? 0u
                             : optimal_index - kSearchDecimation;
  const size_t lim_high =
      std::min(num_candidate_blocks - 1, optimal_index + kSearchDecimation);
  return FullSearch(lim_low, lim_high, exclude_interval, target_block,
                    search_block, energy_target_block, energy_candidate_blocks);
}

void GetPeriodicHanningWindow(base::span<float> window) {
  const float scale = 2.0f * std::numbers::pi_v<float> / window.size();
  for (size_t n = 0; n < window.size(); ++n) {
    window[n] = 0.5f * (1.0f - std::cos(n * scale));
  }
}

}  // namespace media::internal
