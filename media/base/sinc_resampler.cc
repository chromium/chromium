// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Initial input buffer layout, dividing into regions r0_ to r4_ (note: r0_, r3_
// and r4_ will move after the first load):
//
// |----------------|-----------------------------------------|----------------|
//
//                                        request_frames_
//                   <--------------------------------------------------------->
//                                    r0_ (during first load)
//
//  kernel_size_ / 2  kernel_size_ / 2        kernel_size_ / 2  kernel_size_ / 2
// <---------------> <--------------->       <---------------> <--------------->
//        r1_               r2_                     r3_               r4_
//
//                             block_size_ == r4_ - r2_
//                   <--------------------------------------->
//
//                                                  request_frames_
//                                    <------------------ ... ----------------->
//                                               r0_ (during second load)
//
// On the second request r0_ slides to the right by kernel_size_ / 2 and r3_,
// r4_ and block_size_ are reinitialized via step (3) in the algorithm below.
//
// These new regions remain constant until a Flush() occurs.  While complicated,
// this allows us to reduce jitter by always requesting the same amount from the
// provided callback.
//
// The algorithm:
//
// 1) Allocate input_buffer of size: request_frames_ + kernel_size_; this
// ensures
//    there's enough room to read request_frames_ from the callback into region
//    r0_ (which will move between the first and subsequent passes).
//
// 2) Let r1_, r2_ each represent half the kernel centered around r0_:
//
//        r0_ = input_buffer_ + kernel_size_ / 2
//        r1_ = input_buffer_
//        r2_ = r0_
//
//    r0_ is always request_frames_ in size.  r1_, r2_ are kernel_size_ / 2 in
//    size.  r1_ must be zero initialized to avoid convolution with garbage (see
//    step (5) for why).
//
// 3) Let r3_, r4_ each represent half the kernel right aligned with the end of
//    r0_ and choose block_size_ as the distance in frames between r4_ and r2_:
//
//        r3_ = r0_ + request_frames_ - kernel_size_
//        r4_ = r0_ + request_frames_ - kernel_size_ / 2
//        block_size_ = r4_ - r2_ = request_frames_ - kernel_size_ / 2
//
// 4) Consume request_frames_ frames into r0_.
//
// 5) Position kernel centered at start of r2_ and generate output frames until
//    the kernel is centered at the start of r4_ or we've finished generating
//    all the output frames.
//
// 6) Wrap left over data from the r3_ to r1_ and r4_ to r2_.
//
// 7) If we're on the second load, in order to avoid overwriting the frames we
//    just wrapped from r4_ we need to slide r0_ to the right by the size of
//    r4_, which is kernel_size_ / 2:
//
//        r0_ = r0_ + kernel_size_ / 2 = input_buffer_ + kernel_size_
//
//    r3_, r4_, and block_size_ then need to be reinitialized, so goto (3).
//
// 8) Else, if we're not on the second load, goto (4).
//
// Note: we're glossing over how the sub-sample handling works with
// |virtual_source_idx_|, etc.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/base/sinc_resampler.h"

#include <limits>
#include <numbers>

#include "base/check_op.h"
#include "base/cpu.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "cc/base/math_util.h"

#if defined(ARCH_CPU_X86_FAMILY)
#include <immintrin.h>
// Including these headers directly should generally be avoided. Since
// Chrome is compiled with -msse3 (the minimal requirement), we include the
// headers directly to make the intrinsics available.
#include <avx2intrin.h>
#include <avxintrin.h>
#include <fmaintrin.h>
#elif defined(ARCH_CPU_ARM_FAMILY) && defined(USE_NEON)
#include <arm_neon.h>
#endif

namespace media {

static double SincScaleFactor(double io_ratio, int kernel_size) {
  // |sinc_scale_factor| is basically the normalized cutoff frequency of the
  // low-pass filter.
  double sinc_scale_factor = io_ratio > 1.0 ? 1.0 / io_ratio : 1.0;

  // The sinc function is an idealized brick-wall filter, but since we're
  // windowing it the transition from pass to stop does not happen right away.
  // So we should adjust the low pass filter cutoff slightly downward to avoid
  // some aliasing at the very high-end.
  // Note: these values are derived empirically.
  if (kernel_size == SincResampler::kMaxKernelSize) {
    sinc_scale_factor *= 0.92;
  } else {
    DCHECK_EQ(kernel_size, SincResampler::kMinKernelSize);
    sinc_scale_factor *= 0.90;
  }

  return sinc_scale_factor;
}

// If we know the minimum architecture at compile time, avoid CPU detection.
void SincResampler::InitializeCPUSpecificFeatures() {
#if defined(ARCH_CPU_ARM_FAMILY) && defined(USE_NEON)
  convolve_proc_ = Convolve_NEON;
#elif defined(ARCH_CPU_X86_FAMILY)
  base::CPU cpu;
  // Using AVX2 instead of SSE2 when AVX2/FMA3 supported.
  if (cpu.has_avx2() && cpu.has_fma3()) {
    convolve_proc_ = Convolve_AVX2;
  } else if (cpu.has_sse2()) {
    convolve_proc_ = Convolve_SSE;
  } else {
    convolve_proc_ = Convolve_C;
  }
#else
  // Unknown architecture.
  convolve_proc_ = Convolve_C;
#endif
}

static int CalculateChunkSize(int block_size_, double io_ratio) {
  return block_size_ / io_ratio;
}

// Static
int SincResampler::KernelSizeFromRequestFrames(int request_frames) {
  // We want the kernel size to *more* than 1.5 * `request_frames`.
  constexpr int kSmallKernelLimit = kMaxKernelSize * 3 / 2;
  return request_frames <= kSmallKernelLimit ? kMinKernelSize : kMaxKernelSize;
}

SincResampler::SincResampler(double io_sample_rate_ratio,
                             int request_frames,
                             const ReadCB read_cb)
    : kernel_size_(KernelSizeFromRequestFrames(request_frames)),
      kernel_storage_size_(kernel_size_ * (kKernelOffsetCount + 1)),
      io_sample_rate_ratio_(io_sample_rate_ratio),
      read_cb_(std::move(read_cb)),
      request_frames_(request_frames),
      input_buffer_size_(request_frames_ + kernel_size_),
      // Create input buffers with a 32-byte alignment for SIMD optimizations.
      kernel_storage_(static_cast<float*>(
          base::AlignedAlloc(sizeof(float) * kernel_storage_size_, 32))),
      kernel_pre_sinc_storage_(static_cast<float*>(
          base::AlignedAlloc(sizeof(float) * kernel_storage_size_, 32))),
      kernel_window_storage_(static_cast<float*>(
          base::AlignedAlloc(sizeof(float) * kernel_storage_size_, 32))),
      input_buffer_(static_cast<float*>(
          base::AlignedAlloc(sizeof(float) * input_buffer_size_, 32))),
      r1_(input_buffer_.get()),
      r2_(input_buffer_.get() + kernel_size_ / 2) {
  CHECK_GT(request_frames, kernel_size_ * 3 / 2)
      << "request_frames must be greater than 1.5 kernels to allow sufficient "
         "data for resampling";
  // This means that after the first call to Flush we will have
  // block_size_ > kernel_size_ and r2_ < r3_.

  InitializeCPUSpecificFeatures();
  DCHECK(convolve_proc_);
  CHECK_GT(request_frames_, 0);
  Flush();

  memset(kernel_storage_.get(), 0,
         sizeof(*kernel_storage_.get()) * kernel_storage_size_);
  memset(kernel_pre_sinc_storage_.get(), 0,
         sizeof(*kernel_pre_sinc_storage_.get()) * kernel_storage_size_);
  memset(kernel_window_storage_.get(), 0,
         sizeof(*kernel_window_storage_.get()) * kernel_storage_size_);

  InitializeKernel();
}

SincResampler::~SincResampler() = default;

void SincResampler::UpdateRegions(bool second_load) {
  // Setup various region pointers in the buffer (see diagram above).  If we're
  // on the second load we need to slide r0_ to the right by kernel_size_ / 2.
  r0_ = input_buffer_.get() + (second_load ? kernel_size_ : kernel_size_ / 2);
  r3_ = r0_ + request_frames_ - kernel_size_;
  r4_ = r0_ + request_frames_ - kernel_size_ / 2;
  block_size_ = r4_ - r2_;
  chunk_size_ = CalculateChunkSize(block_size_, io_sample_rate_ratio_);

  // r1_ at the beginning of the buffer.
  CHECK_EQ(r1_, input_buffer_.get());
  // r1_ left of r2_, r4_ left of r3_ and size correct.
  CHECK_EQ(r2_ - r1_, r4_ - r3_);
  // r2_ left of r3.
  CHECK_LT(r2_, r3_);
}

void SincResampler::InitializeKernel() {
  // Blackman window parameters.
  static const double kAlpha = 0.16;
  static const double kA0 = 0.5 * (1.0 - kAlpha);
  static const double kA1 = 0.5;
  static const double kA2 = 0.5 * kAlpha;

  // Generates a set of windowed sinc() kernels.
  // We generate a range of sub-sample offsets from 0.0 to 1.0.
  const double sinc_scale_factor =
      SincScaleFactor(io_sample_rate_ratio_, kernel_size_);
  for (int offset_idx = 0; offset_idx <= kKernelOffsetCount; ++offset_idx) {
    const float subsample_offset =
        static_cast<float>(offset_idx) / kKernelOffsetCount;

    for (int i = 0; i < kernel_size_; ++i) {
      const int idx = i + offset_idx * kernel_size_;
      const float pre_sinc =
          std::numbers::pi_v<float> * (i - kernel_size_ / 2 - subsample_offset);
      kernel_pre_sinc_storage_[idx] = pre_sinc;

      // Compute Blackman window, matching the offset of the sinc().
      const float x = (i - subsample_offset) / kernel_size_;
      const float window =
          static_cast<float>(kA0 - kA1 * cos(2.0 * std::numbers::pi * x) +
                             kA2 * cos(4.0 * std::numbers::pi * x));
      kernel_window_storage_[idx] = window;

      // Compute the sinc with offset, then window the sinc() function and store
      // at the correct offset.
      kernel_storage_[idx] = static_cast<float>(
          window * (pre_sinc ? sin(sinc_scale_factor * pre_sinc) / pre_sinc
                             : sinc_scale_factor));
    }
  }
}

void SincResampler::SetRatio(double io_sample_rate_ratio) {
  if (fabs(io_sample_rate_ratio_ - io_sample_rate_ratio) <
      std::numeric_limits<double>::epsilon()) {
    return;
  }

  io_sample_rate_ratio_ = io_sample_rate_ratio;
  chunk_size_ = CalculateChunkSize(block_size_, io_sample_rate_ratio_);

  // Optimize reinitialization by reusing values which are independent of
  // |sinc_scale_factor|.  Provides a 3x speedup.
  const double sinc_scale_factor =
      SincScaleFactor(io_sample_rate_ratio_, kernel_size_);
  for (int offset_idx = 0; offset_idx <= kKernelOffsetCount; ++offset_idx) {
    for (int i = 0; i < kernel_size_; ++i) {
      const int idx = i + offset_idx * kernel_size_;
      const float window = kernel_window_storage_[idx];
      const float pre_sinc = kernel_pre_sinc_storage_[idx];

      kernel_storage_[idx] = static_cast<float>(
          window * (pre_sinc ? sin(sinc_scale_factor * pre_sinc) / pre_sinc
                             : sinc_scale_factor));
    }
  }
}

void SincResampler::Resample(int frames, float* destination) {
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("audio"), "SincResampler::Resample",
               "io sample rate ratio", io_sample_rate_ratio_);
  int remaining_frames = frames;

  // Step (1) -- Prime the input buffer at the start of the input stream.
  if (!buffer_primed_ && remaining_frames) {
    read_cb_.Run(request_frames_, r0_.get());
    buffer_primed_ = true;
  }

  // Step (2) -- Resample!
  while (remaining_frames) {
    // Silent audio can contain non-zero samples small enough to result in
    // subnormals internally. Disabling subnormals can be significantly faster.
    {
      cc::ScopedSubnormalFloatDisabler disable_subnormals;

      while (virtual_source_idx_ < block_size_) {
        // |virtual_source_idx_| lies in between two kernel offsets so figure
        // out what they are.
        const int source_idx = static_cast<int>(virtual_source_idx_);
        const double virtual_offset_idx =
            (virtual_source_idx_ - source_idx) * kKernelOffsetCount;
        const int offset_idx = static_cast<int>(virtual_offset_idx);

        // We'll compute "convolutions" for the two kernels which straddle
        // |virtual_source_idx_|.
        const float* k1 = kernel_storage_.get() + offset_idx * kernel_size_;
        const float* k2 = k1 + kernel_size_;

        // Ensure |k1|, |k2| are 32-byte aligned for SIMD usage.  Should always
        // be true so long as `kernel_size_` is a multiple of 32.
        DCHECK_EQ(0u, reinterpret_cast<uintptr_t>(k1) & 0x1F);
        DCHECK_EQ(0u, reinterpret_cast<uintptr_t>(k2) & 0x1F);

        // Initialize input pointer based on quantized |virtual_source_idx_|.
        const float* input_ptr = r1_ + source_idx;

        // Figure out how much to weight each kernel's "convolution".
        const double kernel_interpolation_factor =
            virtual_offset_idx - offset_idx;
        *destination++ = convolve_proc_(kernel_size_, input_ptr, k1, k2,
                                        kernel_interpolation_factor);

        // Advance the virtual index.
        virtual_source_idx_ += io_sample_rate_ratio_;
        if (!--remaining_frames) {
          return;
        }
      }
    }

    // Wrap back around to the start.
    DCHECK_GE(virtual_source_idx_, block_size_);
    virtual_source_idx_ -= block_size_;

    // Step (3) -- Copy r3_, r4_ to r1_, r2_.
    // This wraps the last input frames back to the start of the buffer.
    memcpy(r1_, r3_, sizeof(*input_buffer_.get()) * kernel_size_);

    // Step (4) -- Reinitialize regions if necessary.
    if (r0_ == r2_) {
      UpdateRegions(true);
    }

    // Step (5) -- Refresh the buffer with more input.
    read_cb_.Run(request_frames_, r0_.get());
  }
}

void SincResampler::PrimeWithSilence() {
  // By enforcing the buffer hasn't been primed, we ensure the input buffer has
  // already been zeroed during construction or by a previous Flush() call.
  DCHECK(!buffer_primed_);
  DCHECK_EQ(input_buffer_[0], 0.0f);
  UpdateRegions(true);
}

void SincResampler::Flush() {
  virtual_source_idx_ = 0;
  buffer_primed_ = false;
  memset(input_buffer_.get(), 0,
         sizeof(*input_buffer_.get()) * input_buffer_size_);
  UpdateRegions(false);
}

int SincResampler::GetMaxInputFramesRequested(
    int output_frames_requested) const {
  const int num_chunks = static_cast<int>(
      std::ceil(static_cast<float>(output_frames_requested) / chunk_size_));

  return num_chunks * request_frames_;
}

double SincResampler::BufferedFrames() const {
  return buffer_primed_ ? request_frames_ - virtual_source_idx_ : 0;
}

int SincResampler::KernelSize() const {
  return kernel_size_;
}

float SincResampler::Convolve_C(const int kernel_size,
                                const float* input_ptr,
                                const float* k1,
                                const float* k2,
                                double kernel_interpolation_factor) {
  float sum1 = 0;
  float sum2 = 0;

  // Generate a single output sample.  Unrolling this loop hurt performance in
  // local testing.
  int n = kernel_size;
  while (n--) {
    sum1 += *input_ptr * *k1++;
    sum2 += *input_ptr++ * *k2++;
  }

  // Linearly interpolate the two "convolutions".
  return static_cast<float>((1.0 - kernel_interpolation_factor) * sum1 +
                            kernel_interpolation_factor * sum2);
}

#if defined(ARCH_CPU_X86_FAMILY)
float SincResampler::Convolve_SSE(const int kernel_size,
                                  const float* input_ptr,
                                  const float* k1,
                                  const float* k2,
                                  double kernel_interpolation_factor) {
  __m128 m_input;
  __m128 m_sums1 = _mm_setzero_ps();
  __m128 m_sums2 = _mm_setzero_ps();

  // Based on |input_ptr| alignment, we need to use loadu or load.  Unrolling
  // these loops hurt performance in local testing.
  if (reinterpret_cast<uintptr_t>(input_ptr) & 0x0F) {
    for (int i = 0; i < kernel_size; i += 4) {
      m_input = _mm_loadu_ps(input_ptr + i);
      m_sums1 = _mm_add_ps(m_sums1, _mm_mul_ps(m_input, _mm_load_ps(k1 + i)));
      m_sums2 = _mm_add_ps(m_sums2, _mm_mul_ps(m_input, _mm_load_ps(k2 + i)));
    }
  } else {
    for (int i = 0; i < kernel_size; i += 4) {
      m_input = _mm_load_ps(input_ptr + i);
      m_sums1 = _mm_add_ps(m_sums1, _mm_mul_ps(m_input, _mm_load_ps(k1 + i)));
      m_sums2 = _mm_add_ps(m_sums2, _mm_mul_ps(m_input, _mm_load_ps(k2 + i)));
    }
  }

  // Linearly interpolate the two "convolutions".
  m_sums1 = _mm_mul_ps(
      m_sums1,
      _mm_set_ps1(static_cast<float>(1.0 - kernel_interpolation_factor)));
  m_sums2 = _mm_mul_ps(
      m_sums2, _mm_set_ps1(static_cast<float>(kernel_interpolation_factor)));
  m_sums1 = _mm_add_ps(m_sums1, m_sums2);

  // Sum components together.
  float result;
  m_sums2 = _mm_add_ps(_mm_movehl_ps(m_sums1, m_sums1), m_sums1);
  _mm_store_ss(&result,
               _mm_add_ss(m_sums2, _mm_shuffle_ps(m_sums2, m_sums2, 1)));

  return result;
}

__attribute__((target("avx2,fma"))) float SincResampler::Convolve_AVX2(
    const int kernel_size,
    const float* input_ptr,
    const float* k1,
    const float* k2,
    double kernel_interpolation_factor) {
  __m256 m_input;
  __m256 m_sums1 = _mm256_setzero_ps();
  __m256 m_sums2 = _mm256_setzero_ps();

  // Based on |input_ptr| alignment, we need to use loadu or load.  Unrolling
  // these loops has not been tested or benchmarked.
  bool aligned_input = (reinterpret_cast<uintptr_t>(input_ptr) & 0x1F) == 0;
  if (!aligned_input) {
    for (size_t i = 0; i < static_cast<size_t>(kernel_size); i += 8) {
      m_input = _mm256_loadu_ps(input_ptr + i);
      m_sums1 = _mm256_fmadd_ps(m_input, _mm256_load_ps(k1 + i), m_sums1);
      m_sums2 = _mm256_fmadd_ps(m_input, _mm256_load_ps(k2 + i), m_sums2);
    }
  } else {
    for (size_t i = 0; i < static_cast<size_t>(kernel_size); i += 8) {
      m_input = _mm256_load_ps(input_ptr + i);
      m_sums1 = _mm256_fmadd_ps(m_input, _mm256_load_ps(k1 + i), m_sums1);
      m_sums2 = _mm256_fmadd_ps(m_input, _mm256_load_ps(k2 + i), m_sums2);
    }
  }

  // Linearly interpolate the two "convolutions".
  __m128 m128_sums1 = _mm_add_ps(_mm256_extractf128_ps(m_sums1, 0),
                                 _mm256_extractf128_ps(m_sums1, 1));
  __m128 m128_sums2 = _mm_add_ps(_mm256_extractf128_ps(m_sums2, 0),
                                 _mm256_extractf128_ps(m_sums2, 1));
  m128_sums1 = _mm_mul_ps(
      m128_sums1,
      _mm_set_ps1(static_cast<float>(1.0 - kernel_interpolation_factor)));
  m128_sums2 = _mm_mul_ps(
      m128_sums2, _mm_set_ps1(static_cast<float>(kernel_interpolation_factor)));
  m128_sums1 = _mm_add_ps(m128_sums1, m128_sums2);

  // Sum components together.
  float result;
  m128_sums2 = _mm_add_ps(_mm_movehl_ps(m128_sums1, m128_sums1), m128_sums1);
  _mm_store_ss(&result, _mm_add_ss(m128_sums2,
                                   _mm_shuffle_ps(m128_sums2, m128_sums2, 1)));

  return result;
}
#elif defined(ARCH_CPU_ARM_FAMILY) && defined(USE_NEON)
float SincResampler::Convolve_NEON(const int kernel_size,
                                   const float* input_ptr,
                                   const float* k1,
                                   const float* k2,
                                   double kernel_interpolation_factor) {
  float32x4_t m_input;
  float32x4_t m_sums1 = vmovq_n_f32(0);
  float32x4_t m_sums2 = vmovq_n_f32(0);

  const float* upper = input_ptr + kernel_size;
  for (; input_ptr < upper;) {
    m_input = vld1q_f32(input_ptr);
    input_ptr += 4;
    m_sums1 = vmlaq_f32(m_sums1, m_input, vld1q_f32(k1));
    k1 += 4;
    m_sums2 = vmlaq_f32(m_sums2, m_input, vld1q_f32(k2));
    k2 += 4;
  }

  // Linearly interpolate the two "convolutions".
  m_sums1 = vmlaq_f32(
      vmulq_f32(m_sums1, vmovq_n_f32(1.0 - kernel_interpolation_factor)),
      m_sums2, vmovq_n_f32(kernel_interpolation_factor));

  // Sum components together.
  float32x2_t m_half = vadd_f32(vget_high_f32(m_sums1), vget_low_f32(m_sums1));
  return vget_lane_f32(vpadd_f32(m_half, m_half), 0);
}
#endif

}  // namespace media
