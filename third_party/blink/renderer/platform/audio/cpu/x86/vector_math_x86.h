// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_AUDIO_CPU_X86_VECTOR_MATH_X86_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_AUDIO_CPU_X86_VECTOR_MATH_X86_H_

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/cpu.h"
#include "base/numerics/safe_conversions.h"
#include "third_party/blink/renderer/platform/audio/cpu/x86/vector_math_avx.h"
#include "third_party/blink/renderer/platform/audio/cpu/x86/vector_math_sse.h"
#include "third_party/blink/renderer/platform/audio/vector_math_scalar.h"

namespace blink {
namespace vector_math {
namespace x86 {

struct FrameCounts {
  size_t scalar_for_alignment;
  size_t sse_for_alignment;
  size_t avx;
  size_t sse;
  size_t scalar;
};

static bool CPUSupportsAVX() {
  static const bool supports = ::base::CPU().has_avx();
  return supports;
}

static size_t GetAVXAlignmentOffsetInNumberOfFloats(const float* source_p) {
  constexpr size_t kBytesPerRegister = avx::kBitsPerRegister / 8u;
  constexpr size_t kAlignmentOffsetMask = kBytesPerRegister - 1u;
  uintptr_t offset =
      reinterpret_cast<uintptr_t>(source_p) & kAlignmentOffsetMask;
  DCHECK_EQ(0u, offset % sizeof(*source_p));
  return offset / sizeof(*source_p);
}

ALWAYS_INLINE static FrameCounts SplitFramesToProcess(
    const float* source_p,
    size_t frames_to_process) {
  FrameCounts counts = {0u, 0u, 0u, 0u, 0u};

  const size_t avx_alignment_offset =
      GetAVXAlignmentOffsetInNumberOfFloats(source_p);

  // If the first frame is not AVX aligned, the first several frames (at most
  // seven) must be processed separately for proper alignment.
  const size_t total_for_alignment =
      (avx::kPackedFloatsPerRegister - avx_alignment_offset) &
      ~avx::kFramesToProcessMask;
  const size_t scalar_for_alignment =
      total_for_alignment & ~sse::kFramesToProcessMask;
  const size_t sse_for_alignment =
      total_for_alignment & sse::kFramesToProcessMask;

  // Check which CPU features can be used based on the number of frames to
  // process and based on CPU support.
  const bool use_at_least_avx =
      CPUSupportsAVX() &&
      frames_to_process >= scalar_for_alignment + sse_for_alignment +
                               avx::kPackedFloatsPerRegister;
  const bool use_at_least_sse =
      use_at_least_avx ||
      frames_to_process >= scalar_for_alignment + sse::kPackedFloatsPerRegister;

  if (use_at_least_sse) {
    counts.scalar_for_alignment = scalar_for_alignment;
    frames_to_process -= counts.scalar_for_alignment;
    // The remaining frames are SSE aligned.
    UNSAFE_TODO(DCHECK(sse::IsAligned(source_p + counts.scalar_for_alignment)));

    if (use_at_least_avx) {
      counts.sse_for_alignment = sse_for_alignment;
      frames_to_process -= counts.sse_for_alignment;
      // The remaining frames are AVX aligned.
      UNSAFE_TODO(DCHECK(avx::IsAligned(source_p + counts.scalar_for_alignment +
                                        counts.sse_for_alignment)));

      // Process as many as possible of the remaining frames using AVX.
      counts.avx = frames_to_process & avx::kFramesToProcessMask;
      frames_to_process -= counts.avx;
    }

    // Process as many as possible of the remaining frames using SSE.
    counts.sse = frames_to_process & sse::kFramesToProcessMask;
    frames_to_process -= counts.sse;
  }

  // Process the remaining frames separately.
  counts.scalar = frames_to_process;
  return counts;
}

ALWAYS_INLINE static void PrepareFilterForConv(
    const float* filter_p,
    size_t filter_size,
    AudioFloatArray* prepared_filter) {
  if (CPUSupportsAVX()) {
    avx::PrepareFilterForConv(filter_p, filter_size, prepared_filter);
  } else {
    sse::PrepareFilterForConv(filter_p, filter_size, prepared_filter);
  }
}

ALWAYS_INLINE static void Conv(base::span<const float> source,
                               const float* filter_p,
                               base::span<float> dest,
                               size_t frames_to_process,
                               size_t filter_size,
                               const AudioFloatArray* prepared_filter) {
  const float* prepared_filter_p =
      prepared_filter ? prepared_filter->Data() : nullptr;
  size_t offset = 0;
  if (prepared_filter_p) {
    if (CPUSupportsAVX() && (filter_size & ~avx::kFramesToProcessMask) == 0u) {
      const size_t avx_frames = frames_to_process & avx::kFramesToProcessMask;
      if (avx_frames > 0u) {
        avx::Conv(source.data(), prepared_filter_p, dest.data(), avx_frames,
                  filter_size);
        offset = avx_frames;
      }
    } else if ((filter_size & ~sse::kFramesToProcessMask) == 0u) {
      const size_t sse_frames = frames_to_process & sse::kFramesToProcessMask;
      if (sse_frames > 0u) {
        sse::Conv(source.data(), prepared_filter_p, dest.data(), sse_frames,
                  filter_size);
        offset = sse_frames;
      }
    }
  }
  if (offset < frames_to_process) {
    scalar::Conv(source.subspan(offset), filter_p, dest.subspan(offset),
                 frames_to_process - offset, filter_size, nullptr);
  }
}

ALWAYS_INLINE static void Vadd(base::span<const float> source1,
                               base::span<const float> source2,
                               base::span<float> dest) {
  DCHECK_EQ(source1.size(), dest.size());
  DCHECK_EQ(source2.size(), dest.size());

  const FrameCounts frame_counts =
      SplitFramesToProcess(source1.data(), dest.size());

  size_t offset = 0;
  if (frame_counts.scalar_for_alignment > 0u) {
    scalar::Vadd(source1.subspan(offset, frame_counts.scalar_for_alignment),
                 source2.subspan(offset, frame_counts.scalar_for_alignment),
                 dest.subspan(offset, frame_counts.scalar_for_alignment));
    offset += frame_counts.scalar_for_alignment;
  }
  if (frame_counts.sse_for_alignment > 0u) {
    sse::Vadd(source1.subspan(offset, frame_counts.sse_for_alignment),
              source2.subspan(offset, frame_counts.sse_for_alignment),
              dest.subspan(offset, frame_counts.sse_for_alignment));
    offset += frame_counts.sse_for_alignment;
  }
  if (frame_counts.avx > 0u) {
    avx::Vadd(source1.subspan(offset, frame_counts.avx),
              source2.subspan(offset, frame_counts.avx),
              dest.subspan(offset, frame_counts.avx));
    offset += frame_counts.avx;
  }
  if (frame_counts.sse > 0u) {
    sse::Vadd(source1.subspan(offset, frame_counts.sse),
              source2.subspan(offset, frame_counts.sse),
              dest.subspan(offset, frame_counts.sse));
    offset += frame_counts.sse;
  }
  if (frame_counts.scalar > 0u) {
    scalar::Vadd(source1.subspan(offset, frame_counts.scalar),
                 source2.subspan(offset, frame_counts.scalar),
                 dest.subspan(offset, frame_counts.scalar));
    offset += frame_counts.scalar;
  }
  DCHECK_EQ(dest.size(), offset);
}

ALWAYS_INLINE static void Vsub(base::span<const float> source1,
                               base::span<const float> source2,
                               base::span<float> dest) {
  DCHECK_EQ(source1.size(), dest.size());
  DCHECK_EQ(source2.size(), dest.size());

  const FrameCounts frame_counts =
      SplitFramesToProcess(source1.data(), dest.size());

  size_t offset = 0;
  if (frame_counts.scalar_for_alignment > 0u) {
    scalar::Vsub(source1.subspan(offset, frame_counts.scalar_for_alignment),
                 source2.subspan(offset, frame_counts.scalar_for_alignment),
                 dest.subspan(offset, frame_counts.scalar_for_alignment));
    offset += frame_counts.scalar_for_alignment;
  }
  if (frame_counts.sse_for_alignment > 0u) {
    sse::Vsub(source1.subspan(offset, frame_counts.sse_for_alignment),
              source2.subspan(offset, frame_counts.sse_for_alignment),
              dest.subspan(offset, frame_counts.sse_for_alignment));
    offset += frame_counts.sse_for_alignment;
  }
  if (frame_counts.avx > 0u) {
    avx::Vsub(source1.subspan(offset, frame_counts.avx),
              source2.subspan(offset, frame_counts.avx),
              dest.subspan(offset, frame_counts.avx));
    offset += frame_counts.avx;
  }
  if (frame_counts.sse > 0u) {
    sse::Vsub(source1.subspan(offset, frame_counts.sse),
              source2.subspan(offset, frame_counts.sse),
              dest.subspan(offset, frame_counts.sse));
    offset += frame_counts.sse;
  }
  if (frame_counts.scalar > 0u) {
    scalar::Vsub(source1.subspan(offset, frame_counts.scalar),
                 source2.subspan(offset, frame_counts.scalar),
                 dest.subspan(offset, frame_counts.scalar));
    offset += frame_counts.scalar;
  }
  DCHECK_EQ(dest.size(), offset);
}

ALWAYS_INLINE static void Vclip(base::span<const float> source,
                                float low_threshold,
                                float high_threshold,
                                base::span<float> dest) {
  DCHECK_EQ(source.size(), dest.size());

  const FrameCounts frame_counts =
      SplitFramesToProcess(source.data(), dest.size());

  size_t offset = 0;
  if (frame_counts.scalar_for_alignment > 0u) {
    scalar::Vclip(source.subspan(offset, frame_counts.scalar_for_alignment),
                  low_threshold, high_threshold,
                  dest.subspan(offset, frame_counts.scalar_for_alignment));
    offset += frame_counts.scalar_for_alignment;
  }
  if (frame_counts.sse_for_alignment > 0u) {
    sse::Vclip(source.subspan(offset, frame_counts.sse_for_alignment),
               low_threshold, high_threshold,
               dest.subspan(offset, frame_counts.sse_for_alignment));
    offset += frame_counts.sse_for_alignment;
  }
  if (frame_counts.avx > 0u) {
    avx::Vclip(source.subspan(offset, frame_counts.avx), low_threshold,
               high_threshold, dest.subspan(offset, frame_counts.avx));
    offset += frame_counts.avx;
  }
  if (frame_counts.sse > 0u) {
    sse::Vclip(source.subspan(offset, frame_counts.sse), low_threshold,
               high_threshold, dest.subspan(offset, frame_counts.sse));
    offset += frame_counts.sse;
  }
  if (frame_counts.scalar > 0u) {
    scalar::Vclip(source.subspan(offset, frame_counts.scalar), low_threshold,
                  high_threshold, dest.subspan(offset, frame_counts.scalar));
    offset += frame_counts.scalar;
  }
  DCHECK_EQ(dest.size(), offset);
}

ALWAYS_INLINE static void Vmaxmgv(const float* source_p,
                                  int source_stride,
                                  float* max_p,
                                  size_t frames_to_process) {
  if (source_stride == 1) {
    const FrameCounts frame_counts =
        SplitFramesToProcess(source_p, frames_to_process);

    scalar::Vmaxmgv(source_p, 1, max_p, frame_counts.scalar_for_alignment);
    size_t i = frame_counts.scalar_for_alignment;
    if (frame_counts.sse_for_alignment > 0u) {
      sse::Vmaxmgv(UNSAFE_TODO(source_p + i), max_p,
                   frame_counts.sse_for_alignment);
      i += frame_counts.sse_for_alignment;
    }
    if (frame_counts.avx > 0u) {
      avx::Vmaxmgv(UNSAFE_TODO(source_p + i), max_p, frame_counts.avx);
      i += frame_counts.avx;
    }
    if (frame_counts.sse > 0u) {
      sse::Vmaxmgv(UNSAFE_TODO(source_p + i), max_p, frame_counts.sse);
      i += frame_counts.sse;
    }
    scalar::Vmaxmgv(UNSAFE_TODO(source_p + i), 1, max_p, frame_counts.scalar);
    DCHECK_EQ(frames_to_process, i + frame_counts.scalar);
  } else {
    scalar::Vmaxmgv(source_p, source_stride, max_p, frames_to_process);
  }
}

ALWAYS_INLINE static void Vmul(base::span<const float> source1,
                               base::span<const float> source2,
                               base::span<float> dest) {
  DCHECK_EQ(source1.size(), dest.size());
  DCHECK_EQ(source2.size(), dest.size());

  const FrameCounts frame_counts =
      SplitFramesToProcess(source1.data(), dest.size());

  size_t offset = 0;
  if (frame_counts.scalar_for_alignment > 0u) {
    scalar::Vmul(source1.subspan(offset, frame_counts.scalar_for_alignment),
                 source2.subspan(offset, frame_counts.scalar_for_alignment),
                 dest.subspan(offset, frame_counts.scalar_for_alignment));
    offset += frame_counts.scalar_for_alignment;
  }
  if (frame_counts.sse_for_alignment > 0u) {
    sse::Vmul(source1.subspan(offset, frame_counts.sse_for_alignment),
              source2.subspan(offset, frame_counts.sse_for_alignment),
              dest.subspan(offset, frame_counts.sse_for_alignment));
    offset += frame_counts.sse_for_alignment;
  }
  if (frame_counts.avx > 0u) {
    avx::Vmul(source1.subspan(offset, frame_counts.avx),
              source2.subspan(offset, frame_counts.avx),
              dest.subspan(offset, frame_counts.avx));
    offset += frame_counts.avx;
  }
  if (frame_counts.sse > 0u) {
    sse::Vmul(source1.subspan(offset, frame_counts.sse),
              source2.subspan(offset, frame_counts.sse),
              dest.subspan(offset, frame_counts.sse));
    offset += frame_counts.sse;
  }
  if (frame_counts.scalar > 0u) {
    scalar::Vmul(source1.subspan(offset, frame_counts.scalar),
                 source2.subspan(offset, frame_counts.scalar),
                 dest.subspan(offset, frame_counts.scalar));
    offset += frame_counts.scalar;
  }
  DCHECK_EQ(dest.size(), offset);
}

ALWAYS_INLINE static void Vsma(const float* source_p,
                               int source_stride,
                               const float* scale,
                               float* dest_p,
                               int dest_stride,
                               size_t frames_to_process) {
  if (source_stride == 1 && dest_stride == 1) {
    const FrameCounts frame_counts =
        SplitFramesToProcess(source_p, frames_to_process);

    scalar::Vsma(source_p, 1, scale, dest_p, 1,
                 frame_counts.scalar_for_alignment);
    size_t i = frame_counts.scalar_for_alignment;
    if (frame_counts.sse_for_alignment > 0u) {
      sse::Vsma(UNSAFE_TODO(source_p + i), scale, UNSAFE_TODO(dest_p + i),
                frame_counts.sse_for_alignment);
      i += frame_counts.sse_for_alignment;
    }
    if (frame_counts.avx > 0u) {
      avx::Vsma(UNSAFE_TODO(source_p + i), scale, UNSAFE_TODO(dest_p + i),
                frame_counts.avx);
      i += frame_counts.avx;
    }
    if (frame_counts.sse > 0u) {
      sse::Vsma(UNSAFE_TODO(source_p + i), scale, UNSAFE_TODO(dest_p + i),
                frame_counts.sse);
      i += frame_counts.sse;
    }
    scalar::Vsma(UNSAFE_TODO(source_p + i), 1, scale, UNSAFE_TODO(dest_p + i),
                 1, frame_counts.scalar);
    DCHECK_EQ(frames_to_process, i + frame_counts.scalar);
  } else {
    scalar::Vsma(source_p, source_stride, scale, dest_p, dest_stride,
                 frames_to_process);
  }
}

ALWAYS_INLINE static void Vsmul(const float* source_p,
                                int source_stride,
                                const float* scale,
                                float* dest_p,
                                int dest_stride,
                                size_t frames_to_process) {
  if (source_stride == 1 && dest_stride == 1) {
    const FrameCounts frame_counts =
        SplitFramesToProcess(source_p, frames_to_process);

    scalar::Vsmul(source_p, 1, scale, dest_p, 1,
                  frame_counts.scalar_for_alignment);
    size_t i = frame_counts.scalar_for_alignment;
    if (frame_counts.sse_for_alignment > 0u) {
      sse::Vsmul(UNSAFE_TODO(source_p + i), scale, UNSAFE_TODO(dest_p + i),
                 frame_counts.sse_for_alignment);
      i += frame_counts.sse_for_alignment;
    }
    if (frame_counts.avx > 0u) {
      avx::Vsmul(UNSAFE_TODO(source_p + i), scale, UNSAFE_TODO(dest_p + i),
                 frame_counts.avx);
      i += frame_counts.avx;
    }
    if (frame_counts.sse > 0u) {
      sse::Vsmul(UNSAFE_TODO(source_p + i), scale, UNSAFE_TODO(dest_p + i),
                 frame_counts.sse);
      i += frame_counts.sse;
    }
    scalar::Vsmul(UNSAFE_TODO(source_p + i), 1, scale, UNSAFE_TODO(dest_p + i),
                  1, frame_counts.scalar);
    DCHECK_EQ(frames_to_process, i + frame_counts.scalar);
  } else {
    scalar::Vsmul(source_p, source_stride, scale, dest_p, dest_stride,
                  frames_to_process);
  }
}

ALWAYS_INLINE static void Vsadd(const float* source_p,
                                int source_stride,
                                const float* addend,
                                float* dest_p,
                                int dest_stride,
                                size_t frames_to_process) {
  if (source_stride == 1 && dest_stride == 1) {
    const FrameCounts frame_counts =
        SplitFramesToProcess(source_p, frames_to_process);

    scalar::Vsadd(source_p, 1, addend, dest_p, 1,
                  frame_counts.scalar_for_alignment);
    size_t i = frame_counts.scalar_for_alignment;
    if (frame_counts.sse_for_alignment > 0u) {
      sse::Vsadd(UNSAFE_TODO(source_p + i), addend, UNSAFE_TODO(dest_p + i),
                 frame_counts.sse_for_alignment);
      i += frame_counts.sse_for_alignment;
    }
    if (frame_counts.avx > 0u) {
      avx::Vsadd(UNSAFE_TODO(source_p + i), addend, UNSAFE_TODO(dest_p + i),
                 frame_counts.avx);
      i += frame_counts.avx;
    }
    if (frame_counts.sse > 0u) {
      sse::Vsadd(UNSAFE_TODO(source_p + i), addend, UNSAFE_TODO(dest_p + i),
                 frame_counts.sse);
      i += frame_counts.sse;
    }
    scalar::Vsadd(UNSAFE_TODO(source_p + i), 1, addend, UNSAFE_TODO(dest_p + i),
                  1, frame_counts.scalar);
    DCHECK_EQ(frames_to_process, i + frame_counts.scalar);
  } else {
    scalar::Vsadd(source_p, source_stride, addend, dest_p, dest_stride,
                  frames_to_process);
  }
}

ALWAYS_INLINE static void Vsvesq(const float* source_p,
                                 int source_stride,
                                 float* sum_p,
                                 size_t frames_to_process) {
  if (source_stride == 1) {
    const FrameCounts frame_counts =
        SplitFramesToProcess(source_p, frames_to_process);

    scalar::Vsvesq(source_p, 1, sum_p, frame_counts.scalar_for_alignment);
    size_t i = frame_counts.scalar_for_alignment;
    if (frame_counts.sse_for_alignment > 0u) {
      sse::Vsvesq(UNSAFE_TODO(source_p + i), sum_p,
                  frame_counts.sse_for_alignment);
      i += frame_counts.sse_for_alignment;
    }
    if (frame_counts.avx > 0u) {
      avx::Vsvesq(UNSAFE_TODO(source_p + i), sum_p, frame_counts.avx);
      i += frame_counts.avx;
    }
    if (frame_counts.sse > 0u) {
      sse::Vsvesq(UNSAFE_TODO(source_p + i), sum_p, frame_counts.sse);
      i += frame_counts.sse;
    }
    scalar::Vsvesq(UNSAFE_TODO(source_p + i), 1, sum_p, frame_counts.scalar);
    DCHECK_EQ(frames_to_process, i + frame_counts.scalar);
  } else {
    scalar::Vsvesq(source_p, source_stride, sum_p, frames_to_process);
  }
}

ALWAYS_INLINE static void Zvmul(const float* real1p,
                                const float* imag1p,
                                const float* real2p,
                                const float* imag2p,
                                float* real_dest_p,
                                float* imag_dest_p,
                                size_t frames_to_process) {
  FrameCounts frame_counts = SplitFramesToProcess(real1p, frames_to_process);

  scalar::Zvmul(real1p, imag1p, real2p, imag2p, real_dest_p, imag_dest_p,
                frame_counts.scalar_for_alignment);
  size_t i = frame_counts.scalar_for_alignment;
  if (frame_counts.sse_for_alignment > 0u) {
    sse::Zvmul(UNSAFE_TODO(real1p + i), UNSAFE_TODO(imag1p + i),
               UNSAFE_TODO(real2p + i), UNSAFE_TODO(imag2p + i),
               UNSAFE_TODO(real_dest_p + i), UNSAFE_TODO(imag_dest_p + i),
               frame_counts.sse_for_alignment);
    i += frame_counts.sse_for_alignment;
  }
  if (frame_counts.avx > 0u) {
    avx::Zvmul(UNSAFE_TODO(real1p + i), UNSAFE_TODO(imag1p + i),
               UNSAFE_TODO(real2p + i), UNSAFE_TODO(imag2p + i),
               UNSAFE_TODO(real_dest_p + i), UNSAFE_TODO(imag_dest_p + i),
               frame_counts.avx);
    i += frame_counts.avx;
  }
  if (frame_counts.sse > 0u) {
    sse::Zvmul(UNSAFE_TODO(real1p + i), UNSAFE_TODO(imag1p + i),
               UNSAFE_TODO(real2p + i), UNSAFE_TODO(imag2p + i),
               UNSAFE_TODO(real_dest_p + i), UNSAFE_TODO(imag_dest_p + i),
               frame_counts.sse);
    i += frame_counts.sse;
  }
  scalar::Zvmul(UNSAFE_TODO(real1p + i), UNSAFE_TODO(imag1p + i),
                UNSAFE_TODO(real2p + i), UNSAFE_TODO(imag2p + i),
                UNSAFE_TODO(real_dest_p + i), UNSAFE_TODO(imag_dest_p + i),
                frame_counts.scalar);
  DCHECK_EQ(frames_to_process, i + frame_counts.scalar);
}

}  // namespace x86
}  // namespace vector_math
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_AUDIO_CPU_X86_VECTOR_MATH_X86_H_
