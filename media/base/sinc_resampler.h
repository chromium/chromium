// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_SINC_RESAMPLER_H_
#define MEDIA_BASE_SINC_RESAMPLER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/aligned_memory.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "media/base/media_export.h"

namespace media {

// SincResampler is a high-quality single-channel sample-rate converter.
class MEDIA_EXPORT SincResampler {
 public:
  // The kernel size can be adjusted for quality (higher is better) at the
  // expense of performance.  Must be a multiple of 32. We aim for 64 for
  // perceptible audio quality (see crbug.com/1407622), but fallback to 32 in
  // cases where `request_frames_` is too small (e.g. 10ms of 8kHz audio).
  // Use SincResampler::KernelSize() to check which size is being used.
  static constexpr int kMaxKernelSize = 64;
  static constexpr int kMinKernelSize = 32;

  // Default request size.  Affects how often and for how much SincResampler
  // calls back for input.  Must be greater than 1.5 * `kernel_size_`.
  static constexpr int kDefaultRequestSize = 512;

  // A smaller request size, which still allows higher quality resampling, by
  // guaranteeing we will use kMaxKernelSize.
  static constexpr int kSmallRequestSize = kMaxKernelSize * 2;

  // The kernel offset count is used for interpolation and is the number of
  // sub-sample kernel shifts.  Can be adjusted for quality (higher is better)
  // at the expense of allocating more memory.
  static constexpr int kKernelOffsetCount = 32;

  // Callback type for providing more data into the resampler.  Expects |frames|
  // of data to be rendered into |destination|; zero padded if not enough frames
  // are available to satisfy the request.
  using ReadCB = base::RepeatingCallback<void(int frames, float* destination)>;

  // Returns the kernel size which will be used for a given `request_frames`.
  static int KernelSizeFromRequestFrames(int request_frames);

  // Constructs a SincResampler with the specified |read_cb|, which is used to
  // acquire audio data for resampling.  |io_sample_rate_ratio| is the ratio
  // of input / output sample rates.  |request_frames| controls the size in
  // frames of the buffer requested by each |read_cb| call.  The value must be
  // greater than 1.5*`kernel_size_`.  Specify kDefaultRequestSize if there are
  // no request size constraints.
  SincResampler(double io_sample_rate_ratio,
                int request_frames,
                const ReadCB read_cb);

  SincResampler(const SincResampler&) = delete;
  SincResampler& operator=(const SincResampler&) = delete;

  ~SincResampler();

  // Resample |frames| of data from |read_cb_| into |destination|.
  void Resample(int frames, float* destination);

  // The maximum size in output frames that guarantees Resample() will only make
  // a single call to |read_cb_| for more data.  Note: If PrimeWithSilence() is
  // not called, chunk size will grow after the first two Resample() calls by
  // `kernel_size_` / (2 * io_sample_rate_ratio).  See the .cc file for details.
  int ChunkSize() const { return chunk_size_; }

  // Returns the max number of frames that could be requested (via multiple
  // calls to |read_cb_|) during one Resample(|output_frames_requested|) call.
  int GetMaxInputFramesRequested(int output_frames_requested) const;

  // Guarantees that ChunkSize() will not change between calls by initializing
  // the input buffer with silence.  Note, this will cause the first few samples
  // of output to be biased towards silence. Must be called again after Flush().
  void PrimeWithSilence();

  // Flush all buffered data and reset internal indices.  Not thread safe, do
  // not call while Resample() is in progress.  Note, if PrimeWithSilence() was
  // previously called it must be called again after the Flush().
  void Flush();

  // Update |io_sample_rate_ratio_|.  SetRatio() will cause a reconstruction of
  // the kernels used for resampling.  Not thread safe, do not call while
  // Resample() is in progress.
  void SetRatio(double io_sample_rate_ratio);

  // Return number of input frames consumed by a callback but not yet processed.
  // Since input/output ratio can be fractional, so can this value.
  // Zero before first call to Resample().
  double BufferedFrames() const;

  // Return the actual kernel size used by the resampler. Should be
  // kMaxKernelSize most of the time, but varies based on `request_frames_`;
  int KernelSize() const;

  float* get_kernel_for_testing() { return kernel_storage_.get(); }

  int kernel_storage_size_for_testing() { return kernel_storage_size_; }

 private:
  FRIEND_TEST_ALL_PREFIXES(SincResamplerTest, Convolve);
  FRIEND_TEST_ALL_PREFIXES(SincResamplerPerfTest, Convolve_unoptimized_aligned);
  FRIEND_TEST_ALL_PREFIXES(SincResamplerPerfTest, Convolve_optimized_aligned);
  FRIEND_TEST_ALL_PREFIXES(SincResamplerPerfTest, Convolve_optimized_unaligned);

  void InitializeKernel();
  void UpdateRegions(bool second_load);

  // Compute convolution of |k1| and |k2| over |input_ptr|, resultant sums are
  // linearly interpolated using |kernel_interpolation_factor|.  On x86, the
  // underlying implementation is chosen at run time based on SSE support.  On
  // ARM, NEON support is chosen at compile time based on compilation flags.
  static float Convolve_C(const int kernel_size,
                          const float* input_ptr,
                          const float* k1,
                          const float* k2,
                          double kernel_interpolation_factor);
#if defined(ARCH_CPU_X86_FAMILY)
  static float Convolve_SSE(const int kernel_size,
                            const float* input_ptr,
                            const float* k1,
                            const float* k2,
                            double kernel_interpolation_factor);
  static float Convolve_AVX2(const int kernel_size,
                             const float* input_ptr,
                             const float* k1,
                             const float* k2,
                             double kernel_interpolation_factor);
#elif defined(ARCH_CPU_ARM_FAMILY) && defined(USE_NEON)
  static float Convolve_NEON(const int kernel_size,
                             const float* input_ptr,
                             const float* k1,
                             const float* k2,
                             double kernel_interpolation_factor);
#endif

  // Selects runtime specific CPU features like SSE.  Must be called before
  // using SincResampler.
  void InitializeCPUSpecificFeatures();

  const int kernel_size_;
  const int kernel_storage_size_;

  // The ratio of input / output sample rates.
  double io_sample_rate_ratio_;

  // An index on the source input buffer with sub-sample precision.  It must be
  // double precision to avoid drift.
  double virtual_source_idx_;

  // The buffer is primed once at the very beginning of processing.
  bool buffer_primed_;

  // Source of data for resampling.
  const ReadCB read_cb_;

  // The size (in samples) to request from each |read_cb_| execution.
  const int request_frames_;

  // The number of source frames processed per pass.
  int block_size_;

  // Cached value used for ChunkSize().  The maximum size in frames that
  // guarantees Resample() will only ask for input at most once.
  int chunk_size_;

  // The size (in samples) of the internal buffer used by the resampler.
  const int input_buffer_size_;

  // Contains kKernelOffsetCount kernels back-to-back, each of size
  // `kernel_size_`. The kernel offsets are sub-sample shifts of a windowed sinc
  // shifted from 0.0 to 1.0 sample.
  std::unique_ptr<float[], base::AlignedFreeDeleter> kernel_storage_;
  std::unique_ptr<float[], base::AlignedFreeDeleter> kernel_pre_sinc_storage_;
  std::unique_ptr<float[], base::AlignedFreeDeleter> kernel_window_storage_;

  // Data from the source is copied into this buffer for each processing pass.
  std::unique_ptr<float[], base::AlignedFreeDeleter> input_buffer_;

  // Stores the runtime selection of which Convolve function to use.
  using ConvolveProc =
      float (*)(const int, const float*, const float*, const float*, double);
  ConvolveProc convolve_proc_;

  // Pointers to the various regions inside |input_buffer_|.  See the diagram at
  // the top of the .cc file for more information.
  raw_ptr<float, AllowPtrArithmetic> r0_;
  const raw_ptr<float, AllowPtrArithmetic> r1_;
  const raw_ptr<float, AllowPtrArithmetic> r2_;
  raw_ptr<float, AllowPtrArithmetic> r3_;
  raw_ptr<float, AllowPtrArithmetic> r4_;
};

}  // namespace media

#endif  // MEDIA_BASE_SINC_RESAMPLER_H_
