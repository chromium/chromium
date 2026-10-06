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
    base::span<const float> source) {
  FrameCounts counts = {0u, 0u, 0u, 0u, 0u};

  const size_t avx_alignment_offset =
      GetAVXAlignmentOffsetInNumberOfFloats(source.data());

  // If the first frame is not AVX aligned, the first several frames (at most
  // seven) must be processed separately for proper alignment.
  const size_t total_for_alignment =
      (avx::kPackedFloatsPerRegister - avx_alignment_offset) &
      ~avx::kFramesToProcessMask;
  const size_t scalar_for_alignment =
      total_for_alignment & ~sse::kFramesToProcessMask;
  const size_t sse_for_alignment =
      total_for_alignment & sse::kFramesToProcessMask;

  size_t frames_to_process = source.size();

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
    DCHECK(sse::IsAligned(source.subspan(counts.scalar_for_alignment).data()));

    if (use_at_least_avx) {
      counts.sse_for_alignment = sse_for_alignment;
      frames_to_process -= counts.sse_for_alignment;
      // The remaining frames are AVX aligned.
      DCHECK(avx::IsAligned(
          source.subspan(counts.scalar_for_alignment + counts.sse_for_alignment)
              .data()));

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
    base::span<const float> filter,
    AudioFloatArray* prepared_filter) {
  if (CPUSupportsAVX()) {
    avx::PrepareFilterForConv(filter, prepared_filter);
  } else {
    sse::PrepareFilterForConv(filter, prepared_filter);
  }
}

ALWAYS_INLINE static void Conv(base::span<const float> source,
                               base::span<const float> filter,
                               base::span<float> dest,
                               base::span<const float> prepared_filter) {
  const size_t frames_to_process = dest.size();
  const size_t filter_size = filter.size();
  size_t offset = 0;
  if (!prepared_filter.empty()) {
    if (CPUSupportsAVX() && (filter_size & ~avx::kFramesToProcessMask) == 0u) {
      const size_t avx_frames = frames_to_process & avx::kFramesToProcessMask;
      if (avx_frames > 0u) {
        avx::Conv(source, prepared_filter, dest.first(avx_frames), filter_size);
        offset = avx_frames;
      }
    } else if ((filter_size & ~sse::kFramesToProcessMask) == 0u) {
      const size_t sse_frames = frames_to_process & sse::kFramesToProcessMask;
      if (sse_frames > 0u) {
        sse::Conv(source, prepared_filter, dest.first(sse_frames), filter_size);
        offset = sse_frames;
      }
    }
  }
  if (offset < frames_to_process) {
    scalar::Conv(source.subspan(offset), filter, dest.subspan(offset), {});
  }
}

ALWAYS_INLINE static void Vadd(base::span<const float> source1,
                               base::span<const float> source2,
                               base::span<float> dest) {
  DCHECK_EQ(source1.size(), dest.size());
  DCHECK_EQ(source2.size(), dest.size());

  const FrameCounts frame_counts = SplitFramesToProcess(source1);

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

  const FrameCounts frame_counts = SplitFramesToProcess(source1);

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

  const FrameCounts frame_counts = SplitFramesToProcess(source);

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

ALWAYS_INLINE static float Vmaxmgv(base::span<const float> source) {
  const FrameCounts frame_counts = SplitFramesToProcess(source);

  float max = 0;
  size_t offset = 0;
  if (frame_counts.scalar_for_alignment > 0u) {
    max = std::max(max, scalar::Vmaxmgv(source.subspan(
                            offset, frame_counts.scalar_for_alignment)));
    offset += frame_counts.scalar_for_alignment;
  }
  if (frame_counts.sse_for_alignment > 0u) {
    max = std::max(max, sse::Vmaxmgv(source.subspan(
                            offset, frame_counts.sse_for_alignment)));
    offset += frame_counts.sse_for_alignment;
  }
  if (frame_counts.avx > 0u) {
    max = std::max(max, avx::Vmaxmgv(source.subspan(offset, frame_counts.avx)));
    offset += frame_counts.avx;
  }
  if (frame_counts.sse > 0u) {
    max = std::max(max, sse::Vmaxmgv(source.subspan(offset, frame_counts.sse)));
    offset += frame_counts.sse;
  }
  if (frame_counts.scalar > 0u) {
    max = std::max(
        max, scalar::Vmaxmgv(source.subspan(offset, frame_counts.scalar)));
    offset += frame_counts.scalar;
  }
  DCHECK_EQ(source.size(), offset);
  return max;
}

ALWAYS_INLINE static void Vmul(base::span<const float> source1,
                               base::span<const float> source2,
                               base::span<float> dest) {
  DCHECK_EQ(source1.size(), dest.size());
  DCHECK_EQ(source2.size(), dest.size());

  const FrameCounts frame_counts = SplitFramesToProcess(source1);

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

ALWAYS_INLINE static void Vsma(base::span<const float> source,
                               float scale,
                               base::span<float> dest) {
  DCHECK_EQ(source.size(), dest.size());
  const FrameCounts frame_counts = SplitFramesToProcess(source);

  size_t offset = 0;
  if (frame_counts.scalar_for_alignment > 0u) {
    scalar::Vsma(source.subspan(offset, frame_counts.scalar_for_alignment),
                 scale,
                 dest.subspan(offset, frame_counts.scalar_for_alignment));
    offset += frame_counts.scalar_for_alignment;
  }
  if (frame_counts.sse_for_alignment > 0u) {
    sse::Vsma(source.subspan(offset, frame_counts.sse_for_alignment), scale,
              dest.subspan(offset, frame_counts.sse_for_alignment));
    offset += frame_counts.sse_for_alignment;
  }
  if (frame_counts.avx > 0u) {
    avx::Vsma(source.subspan(offset, frame_counts.avx), scale,
              dest.subspan(offset, frame_counts.avx));
    offset += frame_counts.avx;
  }
  if (frame_counts.sse > 0u) {
    sse::Vsma(source.subspan(offset, frame_counts.sse), scale,
              dest.subspan(offset, frame_counts.sse));
    offset += frame_counts.sse;
  }
  if (frame_counts.scalar > 0u) {
    scalar::Vsma(source.subspan(offset, frame_counts.scalar), scale,
                 dest.subspan(offset, frame_counts.scalar));
    offset += frame_counts.scalar;
  }
  DCHECK_EQ(dest.size(), offset);
}

ALWAYS_INLINE static void Vsmul(base::span<const float> source,
                                float scale,
                                base::span<float> dest) {
  DCHECK_EQ(source.size(), dest.size());
  const FrameCounts frame_counts = SplitFramesToProcess(source);

  size_t offset = 0;
  if (frame_counts.scalar_for_alignment > 0u) {
    scalar::Vsmul(source.subspan(offset, frame_counts.scalar_for_alignment),
                  scale,
                  dest.subspan(offset, frame_counts.scalar_for_alignment));
    offset += frame_counts.scalar_for_alignment;
  }
  if (frame_counts.sse_for_alignment > 0u) {
    sse::Vsmul(source.subspan(offset, frame_counts.sse_for_alignment), scale,
               dest.subspan(offset, frame_counts.sse_for_alignment));
    offset += frame_counts.sse_for_alignment;
  }
  if (frame_counts.avx > 0u) {
    avx::Vsmul(source.subspan(offset, frame_counts.avx), scale,
               dest.subspan(offset, frame_counts.avx));
    offset += frame_counts.avx;
  }
  if (frame_counts.sse > 0u) {
    sse::Vsmul(source.subspan(offset, frame_counts.sse), scale,
               dest.subspan(offset, frame_counts.sse));
    offset += frame_counts.sse;
  }
  if (frame_counts.scalar > 0u) {
    scalar::Vsmul(source.subspan(offset, frame_counts.scalar), scale,
                  dest.subspan(offset, frame_counts.scalar));
    offset += frame_counts.scalar;
  }
  DCHECK_EQ(dest.size(), offset);
}

ALWAYS_INLINE static void Vsadd(base::span<const float> source,
                                float addend,
                                base::span<float> dest) {
  DCHECK_EQ(source.size(), dest.size());
  const FrameCounts frame_counts = SplitFramesToProcess(source);

  size_t offset = 0;
  if (frame_counts.scalar_for_alignment > 0u) {
    scalar::Vsadd(source.subspan(offset, frame_counts.scalar_for_alignment),
                  addend,
                  dest.subspan(offset, frame_counts.scalar_for_alignment));
    offset += frame_counts.scalar_for_alignment;
  }
  if (frame_counts.sse_for_alignment > 0u) {
    sse::Vsadd(source.subspan(offset, frame_counts.sse_for_alignment), addend,
               dest.subspan(offset, frame_counts.sse_for_alignment));
    offset += frame_counts.sse_for_alignment;
  }
  if (frame_counts.avx > 0u) {
    avx::Vsadd(source.subspan(offset, frame_counts.avx), addend,
               dest.subspan(offset, frame_counts.avx));
    offset += frame_counts.avx;
  }
  if (frame_counts.sse > 0u) {
    sse::Vsadd(source.subspan(offset, frame_counts.sse), addend,
               dest.subspan(offset, frame_counts.sse));
    offset += frame_counts.sse;
  }
  if (frame_counts.scalar > 0u) {
    scalar::Vsadd(source.subspan(offset, frame_counts.scalar), addend,
                  dest.subspan(offset, frame_counts.scalar));
    offset += frame_counts.scalar;
  }
  DCHECK_EQ(dest.size(), offset);
}

ALWAYS_INLINE static float Vsvesq(base::span<const float> source) {
  const FrameCounts frame_counts = SplitFramesToProcess(source);

  float sum = 0;
  size_t offset = 0;
  if (frame_counts.scalar_for_alignment > 0u) {
    sum += scalar::Vsvesq(
        source.subspan(offset, frame_counts.scalar_for_alignment));
    offset += frame_counts.scalar_for_alignment;
  }
  if (frame_counts.sse_for_alignment > 0u) {
    sum += sse::Vsvesq(source.subspan(offset, frame_counts.sse_for_alignment));
    offset += frame_counts.sse_for_alignment;
  }
  if (frame_counts.avx > 0u) {
    sum += avx::Vsvesq(source.subspan(offset, frame_counts.avx));
    offset += frame_counts.avx;
  }
  if (frame_counts.sse > 0u) {
    sum += sse::Vsvesq(source.subspan(offset, frame_counts.sse));
    offset += frame_counts.sse;
  }
  if (frame_counts.scalar > 0u) {
    sum += scalar::Vsvesq(source.subspan(offset, frame_counts.scalar));
    offset += frame_counts.scalar;
  }
  DCHECK_EQ(source.size(), offset);
  return sum;
}

ALWAYS_INLINE static void Zvmul(base::span<const float> real1,
                                base::span<const float> imag1,
                                base::span<const float> real2,
                                base::span<const float> imag2,
                                base::span<float> real_dest,
                                base::span<float> imag_dest) {
  DCHECK_EQ(real1.size(), real_dest.size());
  DCHECK_EQ(imag1.size(), real_dest.size());
  DCHECK_EQ(real2.size(), real_dest.size());
  DCHECK_EQ(imag2.size(), real_dest.size());
  DCHECK_EQ(imag_dest.size(), real_dest.size());

  const FrameCounts frame_counts = SplitFramesToProcess(real1);

  size_t offset = 0;
  if (frame_counts.scalar_for_alignment > 0u) {
    scalar::Zvmul(real1.subspan(offset, frame_counts.scalar_for_alignment),
                  imag1.subspan(offset, frame_counts.scalar_for_alignment),
                  real2.subspan(offset, frame_counts.scalar_for_alignment),
                  imag2.subspan(offset, frame_counts.scalar_for_alignment),
                  real_dest.subspan(offset, frame_counts.scalar_for_alignment),
                  imag_dest.subspan(offset, frame_counts.scalar_for_alignment));
    offset += frame_counts.scalar_for_alignment;
  }
  if (frame_counts.sse_for_alignment > 0u) {
    sse::Zvmul(real1.subspan(offset, frame_counts.sse_for_alignment),
               imag1.subspan(offset, frame_counts.sse_for_alignment),
               real2.subspan(offset, frame_counts.sse_for_alignment),
               imag2.subspan(offset, frame_counts.sse_for_alignment),
               real_dest.subspan(offset, frame_counts.sse_for_alignment),
               imag_dest.subspan(offset, frame_counts.sse_for_alignment));
    offset += frame_counts.sse_for_alignment;
  }
  if (frame_counts.avx > 0u) {
    avx::Zvmul(real1.subspan(offset, frame_counts.avx),
               imag1.subspan(offset, frame_counts.avx),
               real2.subspan(offset, frame_counts.avx),
               imag2.subspan(offset, frame_counts.avx),
               real_dest.subspan(offset, frame_counts.avx),
               imag_dest.subspan(offset, frame_counts.avx));
    offset += frame_counts.avx;
  }
  if (frame_counts.sse > 0u) {
    sse::Zvmul(real1.subspan(offset, frame_counts.sse),
               imag1.subspan(offset, frame_counts.sse),
               real2.subspan(offset, frame_counts.sse),
               imag2.subspan(offset, frame_counts.sse),
               real_dest.subspan(offset, frame_counts.sse),
               imag_dest.subspan(offset, frame_counts.sse));
    offset += frame_counts.sse;
  }
  if (frame_counts.scalar > 0u) {
    scalar::Zvmul(real1.subspan(offset, frame_counts.scalar),
                  imag1.subspan(offset, frame_counts.scalar),
                  real2.subspan(offset, frame_counts.scalar),
                  imag2.subspan(offset, frame_counts.scalar),
                  real_dest.subspan(offset, frame_counts.scalar),
                  imag_dest.subspan(offset, frame_counts.scalar));
    offset += frame_counts.scalar;
  }
  DCHECK_EQ(real_dest.size(), offset);
}

}  // namespace x86
}  // namespace vector_math
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_AUDIO_CPU_X86_VECTOR_MATH_X86_H_
