// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_AUDIO_CPU_ARM_VECTOR_MATH_NEON_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_AUDIO_CPU_ARM_VECTOR_MATH_NEON_H_

#include <arm_neon.h>

#include <algorithm>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "third_party/blink/renderer/platform/audio/vector_math_scalar.h"

namespace blink {
namespace vector_math {
namespace neon {

constexpr size_t kPackedFloatsPerRegister = 4u;

// TODO: Consider optimizing this.
using scalar::Conv;

ALWAYS_INLINE static void Vadd(base::span<const float> source1,
                               base::span<const float> source2,
                               base::span<float> dest) {
  // CHECK allows the compiler to elide bounds checks (docs/unsafe_buffers.md).
  CHECK_EQ(source1.size(), dest.size());
  CHECK_EQ(source2.size(), dest.size());

  const size_t n = dest.size();
  const size_t tail_frames = n % kPackedFloatsPerRegister;
  const size_t aligned_frames = n - tail_frames;

  for (size_t i = 0; i < aligned_frames; i += kPackedFloatsPerRegister) {
    vst1q_f32(
        dest.subspan(i, kPackedFloatsPerRegister).data(),
        vaddq_f32(
            vld1q_f32(source1.subspan(i, kPackedFloatsPerRegister).data()),
            vld1q_f32(source2.subspan(i, kPackedFloatsPerRegister).data())));
  }

  if (tail_frames > 0u) {
    scalar::Vadd(source1.subspan(aligned_frames, tail_frames),
                 source2.subspan(aligned_frames, tail_frames),
                 dest.subspan(aligned_frames, tail_frames));
  }
}

ALWAYS_INLINE static void Vsub(base::span<const float> source1,
                               base::span<const float> source2,
                               base::span<float> dest) {
  // CHECK allows the compiler to elide bounds checks (docs/unsafe_buffers.md).
  CHECK_EQ(source1.size(), dest.size());
  CHECK_EQ(source2.size(), dest.size());

  const size_t n = dest.size();
  const size_t tail_frames = n % kPackedFloatsPerRegister;
  const size_t aligned_frames = n - tail_frames;

  for (size_t i = 0; i < aligned_frames; i += kPackedFloatsPerRegister) {
    vst1q_f32(
        dest.subspan(i, kPackedFloatsPerRegister).data(),
        vsubq_f32(
            vld1q_f32(source1.subspan(i, kPackedFloatsPerRegister).data()),
            vld1q_f32(source2.subspan(i, kPackedFloatsPerRegister).data())));
  }

  if (tail_frames > 0u) {
    scalar::Vsub(source1.subspan(aligned_frames, tail_frames),
                 source2.subspan(aligned_frames, tail_frames),
                 dest.subspan(aligned_frames, tail_frames));
  }
}

ALWAYS_INLINE static void Vclip(base::span<const float> source,
                                float low_threshold,
                                float high_threshold,
                                base::span<float> dest) {
  // CHECK allows the compiler to elide bounds checks (docs/unsafe_buffers.md).
  CHECK_EQ(source.size(), dest.size());

  const size_t n = dest.size();
  const size_t tail_frames = n % kPackedFloatsPerRegister;
  const size_t aligned_frames = n - tail_frames;

  float32x4_t low = vdupq_n_f32(low_threshold);
  float32x4_t high = vdupq_n_f32(high_threshold);
  for (size_t i = 0; i < aligned_frames; i += kPackedFloatsPerRegister) {
    vst1q_f32(
        dest.subspan(i, kPackedFloatsPerRegister).data(),
        vmaxq_f32(
            vminq_f32(
                vld1q_f32(source.subspan(i, kPackedFloatsPerRegister).data()),
                high),
            low));
  }

  if (tail_frames > 0u) {
    scalar::Vclip(source.subspan(aligned_frames, tail_frames), low_threshold,
                  high_threshold, dest.subspan(aligned_frames, tail_frames));
  }
}

ALWAYS_INLINE static void Vmaxmgv(const float* source_p,
                                  int source_stride,
                                  float* max_p,
                                  size_t frames_to_process) {
  size_t n = frames_to_process;

  if (source_stride == 1) {
    size_t tail_frames = n % kPackedFloatsPerRegister;
    const float* end_p = UNSAFE_TODO(source_p + n - tail_frames);

    float32x4_t four_max = vdupq_n_f32(*max_p);
    while (source_p < end_p) {
      float32x4_t source = vld1q_f32(source_p);
      four_max = vmaxq_f32(four_max, vabsq_f32(source));
      UNSAFE_TODO(source_p += kPackedFloatsPerRegister);
    }
    float32x2_t two_max =
        vmax_f32(vget_low_f32(four_max), vget_high_f32(four_max));

    float group_max[2];
    vst1_f32(group_max, two_max);
    *max_p = std::max(group_max[0], group_max[1]);

    n = tail_frames;
  }

  scalar::Vmaxmgv(source_p, source_stride, max_p, n);
}

ALWAYS_INLINE static void Vmul(base::span<const float> source1,
                               base::span<const float> source2,
                               base::span<float> dest) {
  // CHECK allows the compiler to elide bounds checks (docs/unsafe_buffers.md).
  CHECK_EQ(source1.size(), dest.size());
  CHECK_EQ(source2.size(), dest.size());

  const size_t n = dest.size();
  const size_t tail_frames = n % kPackedFloatsPerRegister;
  const size_t aligned_frames = n - tail_frames;

  for (size_t i = 0; i < aligned_frames; i += kPackedFloatsPerRegister) {
    vst1q_f32(
        dest.subspan(i, kPackedFloatsPerRegister).data(),
        vmulq_f32(
            vld1q_f32(source1.subspan(i, kPackedFloatsPerRegister).data()),
            vld1q_f32(source2.subspan(i, kPackedFloatsPerRegister).data())));
  }

  if (tail_frames > 0u) {
    scalar::Vmul(source1.subspan(aligned_frames, tail_frames),
                 source2.subspan(aligned_frames, tail_frames),
                 dest.subspan(aligned_frames, tail_frames));
  }
}

ALWAYS_INLINE static void Vsma(base::span<const float> source,
                               float scale,
                               base::span<float> dest) {
  // CHECK allows the compiler to elide bounds checks (docs/unsafe_buffers.md).
  CHECK_EQ(source.size(), dest.size());
  size_t n = dest.size();
  size_t tail_frames = n % kPackedFloatsPerRegister;
  size_t aligned_frames = n - tail_frames;

  float32x4_t k = vdupq_n_f32(scale);
  for (size_t i = 0; i < aligned_frames; i += kPackedFloatsPerRegister) {
    auto dest_subspan = dest.subspan(i, kPackedFloatsPerRegister);
    float32x4_t source_vec =
        vld1q_f32(source.subspan(i, kPackedFloatsPerRegister).data());
    float32x4_t dest_vec = vld1q_f32(dest_subspan.data());

    dest_vec = vmlaq_f32(dest_vec, source_vec, k);
    vst1q_f32(dest_subspan.data(), dest_vec);
  }

  if (tail_frames > 0u) {
    scalar::Vsma(source.subspan(aligned_frames, tail_frames), scale,
                 dest.subspan(aligned_frames, tail_frames));
  }
}

ALWAYS_INLINE static void Vsmul(base::span<const float> source,
                                float scale,
                                base::span<float> dest) {
  // CHECK allows the compiler to elide bounds checks (docs/unsafe_buffers.md).
  CHECK_EQ(source.size(), dest.size());
  size_t n = dest.size();
  size_t tail_frames = n % kPackedFloatsPerRegister;
  size_t aligned_frames = n - tail_frames;

  for (size_t i = 0; i < aligned_frames; i += kPackedFloatsPerRegister) {
    float32x4_t source_vec =
        vld1q_f32(source.subspan(i, kPackedFloatsPerRegister).data());
    vst1q_f32(dest.subspan(i, kPackedFloatsPerRegister).data(),
              vmulq_n_f32(source_vec, scale));
  }

  if (tail_frames > 0u) {
    scalar::Vsmul(source.subspan(aligned_frames, tail_frames), scale,
                  dest.subspan(aligned_frames, tail_frames));
  }
}

ALWAYS_INLINE static void Vsadd(base::span<const float> source,
                                float addend,
                                base::span<float> dest) {
  // CHECK allows the compiler to elide bounds checks (docs/unsafe_buffers.md).
  CHECK_EQ(source.size(), dest.size());
  size_t n = dest.size();
  size_t tail_frames = n % kPackedFloatsPerRegister;
  size_t aligned_frames = n - tail_frames;

  float32x4_t k = vdupq_n_f32(addend);
  for (size_t i = 0; i < aligned_frames; i += kPackedFloatsPerRegister) {
    float32x4_t source_vec =
        vld1q_f32(source.subspan(i, kPackedFloatsPerRegister).data());
    vst1q_f32(dest.subspan(i, kPackedFloatsPerRegister).data(),
              vaddq_f32(source_vec, k));
  }

  if (tail_frames > 0u) {
    scalar::Vsadd(source.subspan(aligned_frames, tail_frames), addend,
                  dest.subspan(aligned_frames, tail_frames));
  }
}

ALWAYS_INLINE static void Vsvesq(const float* source_p,
                                 int source_stride,
                                 float* sum_p,
                                 size_t frames_to_process) {
  size_t n = frames_to_process;

  if (source_stride == 1) {
    size_t tail_frames = n % kPackedFloatsPerRegister;
    const float* end_p = UNSAFE_TODO(source_p + n - tail_frames);

    float32x4_t four_sum = vdupq_n_f32(0);
    while (source_p < end_p) {
      float32x4_t source = vld1q_f32(source_p);
      four_sum = vmlaq_f32(four_sum, source, source);
      UNSAFE_TODO(source_p += kPackedFloatsPerRegister);
    }
    float32x2_t two_sum =
        vadd_f32(vget_low_f32(four_sum), vget_high_f32(four_sum));

    float group_sum[2];
    vst1_f32(group_sum, two_sum);
    *sum_p += group_sum[0] + group_sum[1];

    n = tail_frames;
  }

  scalar::Vsvesq(source_p, source_stride, sum_p, n);
}

ALWAYS_INLINE static void Zvmul(const float* real1p,
                                const float* imag1p,
                                const float* real2p,
                                const float* imag2p,
                                float* real_dest_p,
                                float* imag_dest_p,
                                size_t frames_to_process) {
  size_t i = 0;

  size_t end_size =
      frames_to_process - frames_to_process % kPackedFloatsPerRegister;
  while (i < end_size) {
    float32x4_t real1 = UNSAFE_TODO(vld1q_f32(real1p + i));
    float32x4_t real2 = UNSAFE_TODO(vld1q_f32(real2p + i));
    float32x4_t imag1 = UNSAFE_TODO(vld1q_f32(imag1p + i));
    float32x4_t imag2 = UNSAFE_TODO(vld1q_f32(imag2p + i));

    float32x4_t real_result = vmlsq_f32(vmulq_f32(real1, real2), imag1, imag2);
    float32x4_t imag_result = vmlaq_f32(vmulq_f32(real1, imag2), imag1, real2);

    UNSAFE_TODO(vst1q_f32(real_dest_p + i, real_result));
    UNSAFE_TODO(vst1q_f32(imag_dest_p + i, imag_result));

    i += kPackedFloatsPerRegister;
  }

  scalar::Zvmul(UNSAFE_TODO(real1p + i), UNSAFE_TODO(imag1p + i),
                UNSAFE_TODO(real2p + i), UNSAFE_TODO(imag2p + i),
                UNSAFE_TODO(real_dest_p + i), UNSAFE_TODO(imag_dest_p + i),
                frames_to_process - i);
}

}  // namespace neon
}  // namespace vector_math
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_AUDIO_CPU_ARM_VECTOR_MATH_NEON_H_
