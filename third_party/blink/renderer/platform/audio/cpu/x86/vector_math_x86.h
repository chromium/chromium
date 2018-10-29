// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_AUDIO_CPU_X86_VECTOR_MATH_X86_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_AUDIO_CPU_X86_VECTOR_MATH_X86_H_

#include "base/cpu.h"
#include "third_party/blink/renderer/platform/audio/cpu/x86/vector_math_avx.h"
#include "third_party/blink/renderer/platform/audio/cpu/x86/vector_math_sse.h"
#include "third_party/blink/renderer/platform/audio/vector_math_scalar.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {
namespace vector_math {
namespace X86 {

struct FrameCounts {
  size_t scalar_for_alignment;
  size_t sse_for_alignment;
  size_t avx;
  size_t sse;
  size_t scalar;
};

static bool CPUSupportsAVX() {
  static bool supports = ::base::CPU().has_avx();
  return supports;
}

static size_t GetAVXAlignmentOffsetInNumberOfFloats(const float* source_p) {
  constexpr size_t kBytesPerRegister = AVX::kBitsPerRegister / 8u;
  constexpr size_t kAlignmentOffsetMask = kBytesPerRegister - 1u;
  size_t offset = reinterpret_cast<size_t>(source_p) & kAlignmentOffsetMask;
  DCHECK_EQ(0u, offset % sizeof(*source_p));
  return offset / sizeof(*source_p);
}

static ALWAYS_INLINE FrameCounts
SplitFramesToProcess(const float* source_p, size_t frames_to_process) {
  FrameCounts counts = {0u, 0u, 0u, 0u, 0u};

  const size_t avx_alignment_offset =
      GetAVXAlignmentOffsetInNumberOfFloats(source_p);

  // If the first frame is not AVX aligned, the first several frames (at most
  // seven) must be processed separately for proper alignment.
  const size_t total_for_alignment =
      (AVX::kPackedFloatsPerRegister - avx_alignment_offset) &
      ~AVX::kFramesToProcessMask;
  const size_t scalar_for_alignment =
      total_for_alignment & ~SSE::kFramesToProcessMask;
  const size_t sse_for_alignment =
      total_for_alignment & SSE::kFramesToProcessMask;

  // Check which CPU features can be used based on the number of frames to
  // process and based on CPU support.
  const bool use_at_least_avx =
      CPUSupportsAVX() &&
      frames_to_process >= scalar_for_alignment + sse_for_alignment +
                               AVX::kPackedFloatsPerRegister;
  const bool use_at_least_sse =
      use_at_least_avx ||
      frames_to_process >= scalar_for_alignment + SSE::kPackedFloatsPerRegister;

  if (use_at_least_sse) {
    counts.scalar_for_alignment = scalar_for_alignment;
    frames_to_process -= counts.scalar_for_alignment;
    // The remaining frames are SSE aligned.
    DCHECK(SSE::IsAligned(source_p + counts.scalar_for_alignment));

    if (use_at_least_avx) {
      counts.sse_for_alignment = sse_for_alignment;
      frames_to_process -= counts.sse_for_alignment;
      // The remaining frames are AVX aligned.
      DCHECK(AVX::IsAligned(source_p + counts.scalar_for_alignment +
                            counts.sse_for_alignment));

      // Process as many as possible of the remaining frames using AVX.
      counts.avx = frames_to_process & AVX::kFramesToProcessMask;
      frames_to_process -= counts.avx;
    }

    // Process as many as possible of the remaining frames using SSE.
    counts.sse = frames_to_process & SSE::kFramesToProcessMask;
    frames_to_process -= counts.sse;
  }

  // Process the remaining frames separately.
  counts.scalar = frames_to_process;
  return counts;
}

static ALWAYS_INLINE void PrepareFilterForConv(
    const float* filter_p,
    int filter_stride,
    size_t filter_size,
    AudioFloatArray* prepared_filter) {
  if (CPUSupportsAVX()) {
    AVX::PrepareFilterForConv(filter_p, filter_stride, filter_size,
                              prepared_filter);
  } else {
    SSE::PrepareFilterForConv(filter_p, filter_stride, filter_size,
                              prepared_filter);
  }
}

static ALWAYS_INLINE void Conv(const float* source_p,
                               int source_stride,
                               const float* filter_p,
                               int filter_stride,
                               float* dest_p,
                               int dest_stride,
                               size_t frames_to_process,
                               size_t filter_size,
                               const AudioFloatArray* prepared_filter) {
  const float* prepared_filter_p =
      prepared_filter ? prepared_filter->Data() : nullptr;
  if (source_stride == 1 && dest_stride == 1 && prepared_filter_p) {
    if (CPUSupportsAVX() && (filter_size & ~AVX::kFramesToProcessMask) == 0u) {
      // |frames_to_process| is always a multiply of render quantum and
      // therefore the frames can always be processed using AVX.
      CHECK_EQ(frames_to_process & ~AVX::kFramesToProcessMask, 0u);
      AVX::Conv(source_p, prepared_filter_p, dest_p, frames_to_process,
                filter_size);
      return;
    }
    if ((filter_size & ~SSE::kFramesToProcessMask) == 0u) {
      // |frames_to_process| is always a multiply of render quantum and
      // therefore the frames can always be processed using SSE.
      CHECK_EQ(frames_to_process & ~SSE::kFramesToProcessMask, 0u);
      SSE::Conv(source_p, prepared_filter_p, dest_p, frames_to_process,
                filter_size);
      return;
    }
  }
  Scalar::Conv(source_p, source_stride, filter_p, filter_stride, dest_p,
               dest_stride, frames_to_process, filter_size, nullptr);
}

static ALWAYS_INLINE void Vadd(const float* source1p,
                               int source_stride1,
                               const float* source2p,
                               int source_stride2,
                               float* dest_p,
                               int dest_stride,
                               size_t frames_to_process) {
  if (source_stride1 == 1 && source_stride2 == 1 && dest_stride == 1) {
    const FrameCounts frame_counts =
        SplitFramesToProcess(source1p, frames_to_process);

    Scalar::Vadd(source1p, 1, source2p, 1, dest_p, 1,
                 frame_counts.scalar_for_alignment);
    size_t i = frame_counts.scalar_for_alignment;
    if (frame_counts.sse_for_alignment > 0u) {
      SSE::Vadd(source1p + i, source2p + i, dest_p + i,
                frame_counts.sse_for_alignment);
      i += frame_counts.sse_for_alignment;
    }
    if (frame_counts.avx > 0u) {
      AVX::Vadd(source1p + i, source2p + i, dest_p + i, frame_counts.avx);
      i += frame_counts.avx;
    }
    if (frame_counts.sse > 0u) {
      SSE::Vadd(source1p + i, source2p + i, dest_p + i, frame_counts.sse);
      i += frame_counts.sse;
    }
    Scalar::Vadd(source1p + i, 1, source2p + i, 1, dest_p + i, 1,
                 frame_counts.scalar);
    DCHECK_EQ(frames_to_process, i + frame_counts.scalar);
  } else {
    Scalar::Vadd(source1p, source_stride1, source2p, source_stride2, dest_p,
                 dest_stride, frames_to_process);
  }
}

static ALWAYS_INLINE void Vclip(const float* source_p,
                                int source_stride,
                                const float* low_threshold_p,
                                const float* high_threshold_p,
                                float* dest_p,
                                int dest_stride,
                                size_t frames_to_process) {
  if (source_stride == 1 && dest_stride == 1) {
    const FrameCounts frame_counts =
        SplitFramesToProcess(source_p, frames_to_process);

    Scalar::Vclip(source_p, 1, low_threshold_p, high_threshold_p, dest_p, 1,
                  frame_counts.scalar_for_alignment);
    size_t i = frame_counts.scalar_for_alignment;
    if (frame_counts.sse_for_alignment > 0u) {
      SSE::Vclip(source_p + i, low_threshold_p, high_threshold_p, dest_p + i,
                 frame_counts.sse_for_alignment);
      i += frame_counts.sse_for_alignment;
    }
    if (frame_counts.avx > 0u) {
      AVX::Vclip(source_p + i, low_threshold_p, high_threshold_p, dest_p + i,
                 frame_counts.avx);
      i += frame_counts.avx;
    }
    if (frame_counts.sse > 0u) {
      SSE::Vclip(source_p + i, low_threshold_p, high_threshold_p, dest_p + i,
                 frame_counts.sse);
      i += frame_counts.sse;
    }
    Scalar::Vclip(source_p + i, 1, low_threshold_p, high_threshold_p,
                  dest_p + i, 1, frame_counts.scalar);
    DCHECK_EQ(frames_to_process, i + frame_counts.scalar);
  } else {
    Scalar::Vclip(source_p, source_stride, low_threshold_p, high_threshold_p,
                  dest_p, dest_stride, frames_to_process);
  }
}

static ALWAYS_INLINE void Vmaxmgv(const float* source_p,
                                  int source_stride,
                                  float* max_p,
                                  size_t frames_to_process) {
  if (source_stride == 1) {
    const FrameCounts frame_counts =
        SplitFramesToProcess(source_p, frames_to_process);

    Scalar::Vmaxmgv(source_p, 1, max_p, frame_counts.scalar_for_alignment);
    size_t i = frame_counts.scalar_for_alignment;
    if (frame_counts.sse_for_alignment > 0u) {
      SSE::Vmaxmgv(source_p + i, max_p, frame_counts.sse_for_alignment);
      i += frame_counts.sse_for_alignment;
    }
    if (frame_counts.avx > 0u) {
      AVX::Vmaxmgv(source_p + i, max_p, frame_counts.avx);
      i += frame_counts.avx;
    }
    if (frame_counts.sse > 0u) {
      SSE::Vmaxmgv(source_p + i, max_p, frame_counts.sse);
      i += frame_counts.sse;
    }
    Scalar::Vmaxmgv(source_p + i, 1, max_p, frame_counts.scalar);
    DCHECK_EQ(frames_to_process, i + frame_counts.scalar);
  } else {
    Scalar::Vmaxmgv(source_p, source_stride, max_p, frames_to_process);
  }
}

static ALWAYS_INLINE void Vmul(const float* source1p,
                               int source_stride1,
                               const float* source2p,
                               int source_stride2,
                               float* dest_p,
                               int dest_stride,
                               size_t frames_to_process) {
  if (source_stride1 == 1 && source_stride2 == 1 && dest_stride == 1) {
    const FrameCounts frame_counts =
        SplitFramesToProcess(source1p, frames_to_process);

    Scalar::Vmul(source1p, 1, source2p, 1, dest_p, 1,
                 frame_counts.scalar_for_alignment);
    size_t i = frame_counts.scalar_for_alignment;
    if (frame_counts.sse_for_alignment > 0u) {
      SSE::Vmul(source1p + i, source2p + i, dest_p + i,
                frame_counts.sse_for_alignment);
      i += frame_counts.sse_for_alignment;
    }
    if (frame_counts.avx > 0u) {
      AVX::Vmul(source1p + i, source2p + i, dest_p + i, frame_counts.avx);
      i += frame_counts.avx;
    }
    if (frame_counts.sse > 0u) {
      SSE::Vmul(source1p + i, source2p + i, dest_p + i, frame_counts.sse);
      i += frame_counts.sse;
    }
    Scalar::Vmul(source1p + i, 1, source2p + i, 1, dest_p + i, 1,
                 frame_counts.scalar);
    DCHECK_EQ(frames_to_process, i + frame_counts.scalar);
  } else {
    Scalar::Vmul(source1p, source_stride1, source2p, source_stride2, dest_p,
                 dest_stride, frames_to_process);
  }
}

static ALWAYS_INLINE void Vsma(const float* source_p,
                               int source_stride,
                               const float* scale,
                               float* dest_p,
                               int dest_stride,
                               size_t frames_to_process) {
  if (source_stride == 1 && dest_stride == 1) {
    const FrameCounts frame_counts =
        SplitFramesToProcess(source_p, frames_to_process);

    Scalar::Vsma(source_p, 1, scale, dest_p, 1,
                 frame_counts.scalar_for_alignment);
    size_t i = frame_counts.scalar_for_alignment;
    if (frame_counts.sse_for_alignment > 0u) {
      SSE::Vsma(source_p + i, scale, dest_p + i,
                frame_counts.sse_for_alignment);
      i += frame_counts.sse_for_alignment;
    }
    if (frame_counts.avx > 0u) {
      AVX::Vsma(source_p + i, scale, dest_p + i, frame_counts.avx);
      i += frame_counts.avx;
    }
    if (frame_counts.sse > 0u) {
      SSE::Vsma(source_p + i, scale, dest_p + i, frame_counts.sse);
      i += frame_counts.sse;
    }
    Scalar::Vsma(source_p + i, 1, scale, dest_p + i, 1, frame_counts.scalar);
    DCHECK_EQ(frames_to_process, i + frame_counts.scalar);
  } else {
    Scalar::Vsma(source_p, source_stride, scale, dest_p, dest_stride,
                 frames_to_process);
  }
}

static ALWAYS_INLINE void Vsmul(const float* source_p,
                                int source_stride,
                                const float* scale,
                                float* dest_p,
                                int dest_stride,
                                size_t frames_to_process) {
  if (source_stride == 1 && dest_stride == 1) {
    const FrameCounts frame_counts =
        SplitFramesToProcess(source_p, frames_to_process);

    Scalar::Vsmul(source_p, 1, scale, dest_p, 1,
                  frame_counts.scalar_for_alignment);
    size_t i = frame_counts.scalar_for_alignment;
    if (frame_counts.sse_for_alignment > 0u) {
      SSE::Vsmul(source_p + i, scale, dest_p + i,
                 frame_counts.sse_for_alignment);
      i += frame_counts.sse_for_alignment;
    }
    if (frame_counts.avx > 0u) {
      AVX::Vsmul(source_p + i, scale, dest_p + i, frame_counts.avx);
      i += frame_counts.avx;
    }
    if (frame_counts.sse > 0u) {
      SSE::Vsmul(source_p + i, scale, dest_p + i, frame_counts.sse);
      i += frame_counts.sse;
    }
    Scalar::Vsmul(source_p + i, 1, scale, dest_p + i, 1, frame_counts.scalar);
    DCHECK_EQ(frames_to_process, i + frame_counts.scalar);
  } else {
    Scalar::Vsmul(source_p, source_stride, scale, dest_p, dest_stride,
                  frames_to_process);
  }
}

static ALWAYS_INLINE void Vsvesq(const float* source_p,
                                 int source_stride,
                                 float* sum_p,
                                 size_t frames_to_process) {
  if (source_stride == 1) {
    const FrameCounts frame_counts =
        SplitFramesToProcess(source_p, frames_to_process);

    Scalar::Vsvesq(source_p, 1, sum_p, frame_counts.scalar_for_alignment);
    size_t i = frame_counts.scalar_for_alignment;
    if (frame_counts.sse_for_alignment > 0u) {
      SSE::Vsvesq(source_p + i, sum_p, frame_counts.sse_for_alignment);
      i += frame_counts.sse_for_alignment;
    }
    if (frame_counts.avx > 0u) {
      AVX::Vsvesq(source_p + i, sum_p, frame_counts.avx);
      i += frame_counts.avx;
    }
    if (frame_counts.sse > 0u) {
      SSE::Vsvesq(source_p + i, sum_p, frame_counts.sse);
      i += frame_counts.sse;
    }
    Scalar::Vsvesq(source_p + i, 1, sum_p, frame_counts.scalar);
    DCHECK_EQ(frames_to_process, i + frame_counts.scalar);
  } else {
    Scalar::Vsvesq(source_p, source_stride, sum_p, frames_to_process);
  }
}

static ALWAYS_INLINE void Zvmul(const float* real1p,
                                const float* imag1p,
                                const float* real2p,
                                const float* imag2p,
                                float* real_dest_p,
                                float* imag_dest_p,
                                size_t frames_to_process) {
  FrameCounts frame_counts = SplitFramesToProcess(real1p, frames_to_process);

  Scalar::Zvmul(real1p, imag1p, real2p, imag2p, real_dest_p, imag_dest_p,
                frame_counts.scalar_for_alignment);
  size_t i = frame_counts.scalar_for_alignment;
  if (frame_counts.sse_for_alignment > 0u) {
    SSE::Zvmul(real1p + i, imag1p + i, real2p + i, imag2p + i, real_dest_p + i,
               imag_dest_p + i, frame_counts.sse_for_alignment);
    i += frame_counts.sse_for_alignment;
  }
  if (frame_counts.avx > 0u) {
    AVX::Zvmul(real1p + i, imag1p + i, real2p + i, imag2p + i, real_dest_p + i,
               imag_dest_p + i, frame_counts.avx);
    i += frame_counts.avx;
  }
  if (frame_counts.sse > 0u) {
    SSE::Zvmul(real1p + i, imag1p + i, real2p + i, imag2p + i, real_dest_p + i,
               imag_dest_p + i, frame_counts.sse);
    i += frame_counts.sse;
  }
  Scalar::Zvmul(real1p + i, imag1p + i, real2p + i, imag2p + i, real_dest_p + i,
                imag_dest_p + i, frame_counts.scalar);
  DCHECK_EQ(frames_to_process, i + frame_counts.scalar);
}

}  // namespace X86
}  // namespace vector_math
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_AUDIO_CPU_X86_VECTOR_MATH_X86_H_
