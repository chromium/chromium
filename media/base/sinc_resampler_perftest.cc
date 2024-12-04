// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "base/cpu.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "media/base/sinc_resampler.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"

namespace media {

static const int kBenchmarkIterations = 50000000;

static const double kSampleRateRatio = 192000.0 / 44100.0;
static const double kKernelInterpolationFactor = 0.5;

static void RunConvolveBenchmark(float (*convolve_fn)(const int,
                                                      const float*,
                                                      const float*,
                                                      const float*,
                                                      double),
                                 bool aligned,
                                 const std::string& trace_name) {
  SincResampler resampler(kSampleRateRatio, SincResampler::kDefaultRequestSize,
                          base::DoNothing());

  base::TimeTicks start = base::TimeTicks::Now();
  for (int i = 0; i < kBenchmarkIterations; ++i) {
    convolve_fn(resampler.KernelSize(),
                resampler.get_kernel_for_testing() + (aligned ? 0 : 1),
                resampler.get_kernel_for_testing(),
                resampler.get_kernel_for_testing(), kKernelInterpolationFactor);
  }
  double total_time_seconds = (base::TimeTicks::Now() - start).InSecondsF();

  perf_test::PerfResultReporter reporter("sinc_resampler", trace_name);
  reporter.RegisterImportantMetric("_convolve", "runs/s");
  reporter.AddResult("_convolve", kBenchmarkIterations / total_time_seconds);
}

// Benchmark for the various Convolve() methods.  Make sure to build with
// branding=Chrome so that DCHECKs are compiled out when benchmarking.
TEST(SincResamplerPerfTest, Convolve_unoptimized_aligned) {
  RunConvolveBenchmark(SincResampler::Convolve_C, true, "unoptimized_aligned");
}

TEST(SincResamplerPerfTest, Convolve_optimized_aligned) {
#if defined(ARCH_CPU_X86_FAMILY)
  base::CPU cpu;
  if (cpu.has_avx2()) {
    RunConvolveBenchmark(SincResampler::Convolve_AVX2, true,
                         "optimized_aligned");
  } else if (cpu.has_sse2()) {
    RunConvolveBenchmark(SincResampler::Convolve_SSE, true,
                         "optimized_aligned");
  }
#elif defined(ARCH_CPU_ARM_FAMILY) && defined(USE_NEON)
  RunConvolveBenchmark(SincResampler::Convolve_NEON, true, "optimized_aligned");
#endif
}

TEST(SincResamplerPerfTest, Convolve_optimized_unaligned) {
#if defined(ARCH_CPU_X86_FAMILY)
  base::CPU cpu;
  if (cpu.has_avx2()) {
    RunConvolveBenchmark(SincResampler::Convolve_AVX2, false,
                         "optimized_unaligned");
  } else if (cpu.has_sse2()) {
    RunConvolveBenchmark(SincResampler::Convolve_SSE, false,
                         "optimized_unaligned");
  }
#elif defined(ARCH_CPU_ARM_FAMILY) && defined(USE_NEON)
  RunConvolveBenchmark(SincResampler::Convolve_NEON, false,
                       "optimized_unaligned");
#endif
}

} // namespace media
