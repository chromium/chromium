/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/audio/sinc_resampler.h"

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/platform/audio/audio_bus.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/fdlibm/ieee754.h"

#if defined(ARCH_CPU_X86_FAMILY)
#include <emmintrin.h>
#endif

// Input buffer layout, dividing the total buffer into regions (r0 - r5):
//
// |----------------|-----------------------------------------|----------------|
//
//                                     blockSize + kernelSize / 2
//                   <--------------------------------------------------------->
//                                                r0
//
//   kernelSize / 2   kernelSize / 2          kernelSize / 2     kernelSize / 2
// <---------------> <--------------->       <---------------> <--------------->
//         r1                r2                      r3               r4
//
//                                                     blockSize
//                                    <---------------------------------------->
//                                                         r5

// The Algorithm:
//
// 1) Consume input frames into r0 (r1 is zero-initialized).
// 2) Position kernel centered at start of r0 (r2) and generate output frames
//    until kernel is centered at start of r4, or we've finished generating
//    all the output frames.
// 3) Copy r3 to r1 and r4 to r2.
// 4) Consume input frames into r5 (zero-pad if we run out of input).
// 5) Goto (2) until all of input is consumed.
//
// note: we're glossing over how the sub-sample handling works with
// m_virtualSourceIndex, etc.

namespace blink {

SincResampler::SincResampler(double scale_factor,
                             unsigned kernel_size,
                             unsigned number_of_kernel_offsets)
    : scale_factor_(scale_factor),
      kernel_size_(kernel_size),
      number_of_kernel_offsets_(number_of_kernel_offsets),
      kernel_storage_(kernel_size_ * (number_of_kernel_offsets_ + 1)),
      virtual_source_index_(0),
      block_size_(512),
      // See input buffer layout above.
      input_buffer_(block_size_ + kernel_size_),
      source_(nullptr),
      source_frames_available_(0),
      source_provider_(nullptr),
      is_buffer_primed_(false) {
  InitializeKernel();
}

void SincResampler::InitializeKernel() {
  // Blackman window parameters.
  double alpha = 0.16;
  double a0 = 0.5 * (1.0 - alpha);
  double a1 = 0.5;
  double a2 = 0.5 * alpha;

  // sincScaleFactor is basically the normalized cutoff frequency of the
  // low-pass filter.
  double sinc_scale_factor = scale_factor_ > 1.0 ? 1.0 / scale_factor_ : 1.0;

  // The sinc function is an idealized brick-wall filter, but since we're
  // windowing it the transition from pass to stop does not happen right away.
  // So we should adjust the lowpass filter cutoff slightly downward to avoid
  // some aliasing at the very high-end.
  // FIXME: this value is empirical and to be more exact should vary depending
  // on m_kernelSize.
  sinc_scale_factor *= 0.9;

  int n = kernel_size_;
  int half_size = n / 2;

  // Generates a set of windowed sinc() kernels.
  // We generate a range of sub-sample offsets from 0.0 to 1.0.
  for (unsigned offset_index = 0; offset_index <= number_of_kernel_offsets_;
       ++offset_index) {
    double subsample_offset =
        static_cast<double>(offset_index) / number_of_kernel_offsets_;

    for (int i = 0; i < n; ++i) {
      // Compute the sinc() with offset.
      double s =
          sinc_scale_factor * kPiDouble * (i - half_size - subsample_offset);
      double sinc = !s ? 1.0 : fdlibm::sin(s) / s;
      sinc *= sinc_scale_factor;

      // Compute Blackman window, matching the offset of the sinc().
      double x = (i - subsample_offset) / n;
      double window = a0 - a1 * fdlibm::cos(kTwoPiDouble * x) +
                      a2 * fdlibm::cos(kTwoPiDouble * 2.0 * x);

      // Window the sinc() function and store at the correct offset.
      kernel_storage_[i + offset_index * kernel_size_] = sinc * window;
    }
  }
}

void SincResampler::ConsumeSource(float* buffer,
                                  unsigned number_of_source_frames) {
  DCHECK(source_provider_);

  // Wrap the provided buffer by an AudioBus for use by the source provider.
  scoped_refptr<AudioBus> bus =
      AudioBus::Create(1, number_of_source_frames, false);

  // FIXME: Find a way to make the following const-correct:
  bus->SetChannelMemory(0, buffer, number_of_source_frames);

  source_provider_->ProvideInput(
      bus.get(), base::checked_cast<int>(number_of_source_frames));
}

namespace {

// BufferSourceProvider is an AudioSourceProvider wrapping an in-memory buffer.

class BufferSourceProvider final : public AudioSourceProvider {
 public:
  BufferSourceProvider(const float* source, int number_of_source_frames)
      : source_(source), source_frames_available_(number_of_source_frames) {}

  // Consumes samples from the in-memory buffer.
  void ProvideInput(AudioBus* bus, int frames_to_process) override {
    DCHECK(source_);
    DCHECK(bus);
    if (!source_ || !bus) {
      return;
    }

    float* buffer = bus->Channel(0)->MutableData();

    // Clamp to number of frames available and zero-pad.
    int frames_to_copy = std::min(source_frames_available_, frames_to_process);
    memcpy(buffer, source_, sizeof(float) * frames_to_copy);

    // Zero-pad if necessary.
    if (frames_to_copy < frames_to_process) {
      memset(buffer + frames_to_copy, 0,
             sizeof(float) * (frames_to_process - frames_to_copy));
    }

    source_frames_available_ -= frames_to_copy;
    source_ += frames_to_copy;
  }

 private:
  raw_ptr<const float, AllowPtrArithmetic> source_;
  int source_frames_available_;
};

}  // namespace

void SincResampler::Process(const float* source,
                            float* destination,
                            int number_of_source_frames) {
  // Resample an in-memory buffer using an AudioSourceProvider.
  BufferSourceProvider source_provider(source, number_of_source_frames);

  unsigned number_of_destination_frames =
      static_cast<unsigned>(number_of_source_frames / scale_factor_);
  unsigned remaining = number_of_destination_frames;

  while (remaining) {
    unsigned frames_this_time = std::min(remaining, block_size_);
    Process(&source_provider, destination, frames_this_time);

    destination += frames_this_time;
    remaining -= frames_this_time;
  }
}

void SincResampler::Process(AudioSourceProvider* source_provider,
                            float* destination,
                            uint32_t frames_to_process) {
  DCHECK(source_provider);
  DCHECK_GT(block_size_, kernel_size_);
  DCHECK_GE(input_buffer_.size(), block_size_ + kernel_size_);
  DCHECK_EQ(kernel_size_ % 2, 0u);

  source_provider_ = source_provider;

  unsigned number_of_destination_frames = frames_to_process;

  // Setup various region pointers in the buffer (see diagram above).
  float* r0 = input_buffer_.Data() + kernel_size_ / 2;
  float* r1 = input_buffer_.Data();
  float* r2 = r0;
  float* r3 = r0 + block_size_ - kernel_size_ / 2;
  float* r4 = r0 + block_size_;
  float* r5 = r0 + kernel_size_ / 2;

  // Step (1)
  // Prime the input buffer at the start of the input stream.
  if (!is_buffer_primed_) {
    ConsumeSource(r0, block_size_ + kernel_size_ / 2);
    is_buffer_primed_ = true;
  }

  // Step (2)

  while (number_of_destination_frames) {
    while (virtual_source_index_ < block_size_) {
      // m_virtualSourceIndex lies in between two kernel offsets so figure out
      // what they are.
      int source_index_i = static_cast<int>(virtual_source_index_);
      double subsample_remainder = virtual_source_index_ - source_index_i;

      double virtual_offset_index =
          subsample_remainder * number_of_kernel_offsets_;
      int offset_index = static_cast<int>(virtual_offset_index);

      float* k1 = kernel_storage_.Data() + offset_index * kernel_size_;
      float* k2 = k1 + kernel_size_;

      // Initialize input pointer based on quantized m_virtualSourceIndex.
      float* input_p = r1 + source_index_i;

      // We'll compute "convolutions" for the two kernels which straddle
      // m_virtualSourceIndex
      float sum1 = 0;
      float sum2 = 0;

      // Figure out how much to weight each kernel's "convolution".
      double kernel_interpolation_factor = virtual_offset_index - offset_index;

      // Generate a single output sample.
      int n = kernel_size_;

#define CONVOLVE_ONE_SAMPLE() \
  do {                        \
    input = *input_p++;       \
    sum1 += input * *k1;      \
    sum2 += input * *k2;      \
    ++k1;                     \
    ++k2;                     \
  } while (0)

      {
        float input;

#if defined(ARCH_CPU_X86_FAMILY)
        // If the sourceP address is not 16-byte aligned, the first several
        // frames (at most three) should be processed seperately.
        while ((reinterpret_cast<uintptr_t>(input_p) & 0x0F) && n) {
          CONVOLVE_ONE_SAMPLE();
          n--;
        }

        // Now the inputP is aligned and start to apply SSE.
        float* end_p = input_p + n - n % 4;
        __m128 m_input;
        __m128 m_k1;
        __m128 m_k2;
        __m128 mul1;
        __m128 mul2;

        __m128 sums1 = _mm_setzero_ps();
        __m128 sums2 = _mm_setzero_ps();
        bool k1_aligned = !(reinterpret_cast<uintptr_t>(k1) & 0x0F);
        bool k2_aligned = !(reinterpret_cast<uintptr_t>(k2) & 0x0F);

#define LOAD_DATA(l1, l2)           \
  do {                              \
    m_input = _mm_load_ps(input_p); \
    m_k1 = _mm_##l1##_ps(k1);       \
    m_k2 = _mm_##l2##_ps(k2);       \
  } while (0)

#define CONVOLVE_4_SAMPLES()          \
  do {                                \
    mul1 = _mm_mul_ps(m_input, m_k1); \
    mul2 = _mm_mul_ps(m_input, m_k2); \
    sums1 = _mm_add_ps(sums1, mul1);  \
    sums2 = _mm_add_ps(sums2, mul2);  \
    input_p += 4;                     \
    k1 += 4;                          \
    k2 += 4;                          \
  } while (0)

        if (k1_aligned && k2_aligned) {  // both aligned
          while (input_p < end_p) {
            LOAD_DATA(load, load);
            CONVOLVE_4_SAMPLES();
          }
        } else if (!k1_aligned && k2_aligned) {  // only k2 aligned
          while (input_p < end_p) {
            LOAD_DATA(loadu, load);
            CONVOLVE_4_SAMPLES();
          }
        } else if (k1_aligned && !k2_aligned) {  // only k1 aligned
          while (input_p < end_p) {
            LOAD_DATA(load, loadu);
            CONVOLVE_4_SAMPLES();
          }
        } else {  // both non-aligned
          while (input_p < end_p) {
            LOAD_DATA(loadu, loadu);
            CONVOLVE_4_SAMPLES();
          }
        }

        // Summarize the SSE results to sum1 and sum2.
        float* group_sum_p = reinterpret_cast<float*>(&sums1);
        sum1 +=
            group_sum_p[0] + group_sum_p[1] + group_sum_p[2] + group_sum_p[3];
        group_sum_p = reinterpret_cast<float*>(&sums2);
        sum2 +=
            group_sum_p[0] + group_sum_p[1] + group_sum_p[2] + group_sum_p[3];

        n %= 4;
        while (n) {
          CONVOLVE_ONE_SAMPLE();
          n--;
        }
#else
        // FIXME: add ARM NEON optimizations for the following. The scalar
        // code-path can probably also be optimized better.

        // Optimize size 32 and size 64 kernels by unrolling the while loop.
        // A 20 - 30% speed improvement was measured in some cases by using this
        // approach.

        if (n == 32) {
          CONVOLVE_ONE_SAMPLE();  // 1
          CONVOLVE_ONE_SAMPLE();  // 2
          CONVOLVE_ONE_SAMPLE();  // 3
          CONVOLVE_ONE_SAMPLE();  // 4
          CONVOLVE_ONE_SAMPLE();  // 5
          CONVOLVE_ONE_SAMPLE();  // 6
          CONVOLVE_ONE_SAMPLE();  // 7
          CONVOLVE_ONE_SAMPLE();  // 8
          CONVOLVE_ONE_SAMPLE();  // 9
          CONVOLVE_ONE_SAMPLE();  // 10
          CONVOLVE_ONE_SAMPLE();  // 11
          CONVOLVE_ONE_SAMPLE();  // 12
          CONVOLVE_ONE_SAMPLE();  // 13
          CONVOLVE_ONE_SAMPLE();  // 14
          CONVOLVE_ONE_SAMPLE();  // 15
          CONVOLVE_ONE_SAMPLE();  // 16
          CONVOLVE_ONE_SAMPLE();  // 17
          CONVOLVE_ONE_SAMPLE();  // 18
          CONVOLVE_ONE_SAMPLE();  // 19
          CONVOLVE_ONE_SAMPLE();  // 20
          CONVOLVE_ONE_SAMPLE();  // 21
          CONVOLVE_ONE_SAMPLE();  // 22
          CONVOLVE_ONE_SAMPLE();  // 23
          CONVOLVE_ONE_SAMPLE();  // 24
          CONVOLVE_ONE_SAMPLE();  // 25
          CONVOLVE_ONE_SAMPLE();  // 26
          CONVOLVE_ONE_SAMPLE();  // 27
          CONVOLVE_ONE_SAMPLE();  // 28
          CONVOLVE_ONE_SAMPLE();  // 29
          CONVOLVE_ONE_SAMPLE();  // 30
          CONVOLVE_ONE_SAMPLE();  // 31
          CONVOLVE_ONE_SAMPLE();  // 32
        } else if (n == 64) {
          CONVOLVE_ONE_SAMPLE();  // 1
          CONVOLVE_ONE_SAMPLE();  // 2
          CONVOLVE_ONE_SAMPLE();  // 3
          CONVOLVE_ONE_SAMPLE();  // 4
          CONVOLVE_ONE_SAMPLE();  // 5
          CONVOLVE_ONE_SAMPLE();  // 6
          CONVOLVE_ONE_SAMPLE();  // 7
          CONVOLVE_ONE_SAMPLE();  // 8
          CONVOLVE_ONE_SAMPLE();  // 9
          CONVOLVE_ONE_SAMPLE();  // 10
          CONVOLVE_ONE_SAMPLE();  // 11
          CONVOLVE_ONE_SAMPLE();  // 12
          CONVOLVE_ONE_SAMPLE();  // 13
          CONVOLVE_ONE_SAMPLE();  // 14
          CONVOLVE_ONE_SAMPLE();  // 15
          CONVOLVE_ONE_SAMPLE();  // 16
          CONVOLVE_ONE_SAMPLE();  // 17
          CONVOLVE_ONE_SAMPLE();  // 18
          CONVOLVE_ONE_SAMPLE();  // 19
          CONVOLVE_ONE_SAMPLE();  // 20
          CONVOLVE_ONE_SAMPLE();  // 21
          CONVOLVE_ONE_SAMPLE();  // 22
          CONVOLVE_ONE_SAMPLE();  // 23
          CONVOLVE_ONE_SAMPLE();  // 24
          CONVOLVE_ONE_SAMPLE();  // 25
          CONVOLVE_ONE_SAMPLE();  // 26
          CONVOLVE_ONE_SAMPLE();  // 27
          CONVOLVE_ONE_SAMPLE();  // 28
          CONVOLVE_ONE_SAMPLE();  // 29
          CONVOLVE_ONE_SAMPLE();  // 30
          CONVOLVE_ONE_SAMPLE();  // 31
          CONVOLVE_ONE_SAMPLE();  // 32
          CONVOLVE_ONE_SAMPLE();  // 33
          CONVOLVE_ONE_SAMPLE();  // 34
          CONVOLVE_ONE_SAMPLE();  // 35
          CONVOLVE_ONE_SAMPLE();  // 36
          CONVOLVE_ONE_SAMPLE();  // 37
          CONVOLVE_ONE_SAMPLE();  // 38
          CONVOLVE_ONE_SAMPLE();  // 39
          CONVOLVE_ONE_SAMPLE();  // 40
          CONVOLVE_ONE_SAMPLE();  // 41
          CONVOLVE_ONE_SAMPLE();  // 42
          CONVOLVE_ONE_SAMPLE();  // 43
          CONVOLVE_ONE_SAMPLE();  // 44
          CONVOLVE_ONE_SAMPLE();  // 45
          CONVOLVE_ONE_SAMPLE();  // 46
          CONVOLVE_ONE_SAMPLE();  // 47
          CONVOLVE_ONE_SAMPLE();  // 48
          CONVOLVE_ONE_SAMPLE();  // 49
          CONVOLVE_ONE_SAMPLE();  // 50
          CONVOLVE_ONE_SAMPLE();  // 51
          CONVOLVE_ONE_SAMPLE();  // 52
          CONVOLVE_ONE_SAMPLE();  // 53
          CONVOLVE_ONE_SAMPLE();  // 54
          CONVOLVE_ONE_SAMPLE();  // 55
          CONVOLVE_ONE_SAMPLE();  // 56
          CONVOLVE_ONE_SAMPLE();  // 57
          CONVOLVE_ONE_SAMPLE();  // 58
          CONVOLVE_ONE_SAMPLE();  // 59
          CONVOLVE_ONE_SAMPLE();  // 60
          CONVOLVE_ONE_SAMPLE();  // 61
          CONVOLVE_ONE_SAMPLE();  // 62
          CONVOLVE_ONE_SAMPLE();  // 63
          CONVOLVE_ONE_SAMPLE();  // 64
        } else {
          while (n--) {
            // Non-optimized using actual while loop.
            CONVOLVE_ONE_SAMPLE();
          }
        }
#endif
      }
#undef CONVOLVE_ONE_SAMPLE

      // Linearly interpolate the two "convolutions".
      double result = (1.0 - kernel_interpolation_factor) * sum1 +
                      kernel_interpolation_factor * sum2;

      *destination++ = result;

      // Advance the virtual index.
      virtual_source_index_ += scale_factor_;

      --number_of_destination_frames;
      if (!number_of_destination_frames) {
        return;
      }
    }

    // Wrap back around to the start.
    virtual_source_index_ -= block_size_;

    // Step (3) Copy r3 to r1 and r4 to r2.
    // This wraps the last input frames back to the start of the buffer.
    memcpy(r1, r3, sizeof(float) * (kernel_size_ / 2));
    memcpy(r2, r4, sizeof(float) * (kernel_size_ / 2));

    // Step (4)
    // Refresh the buffer with more input.
    ConsumeSource(r5, block_size_);
  }
}

}  // namespace blink
