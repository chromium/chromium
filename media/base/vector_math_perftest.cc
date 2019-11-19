// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/macros.h"
#include "base/memory/aligned_memory.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "media/base/vector_math.h"
#include "media/base/vector_math_testing.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"

using base::TimeTicks;
using std::fill;

namespace {

perf_test::PerfResultReporter SetUpReporter(const std::string& story_name) {
  perf_test::PerfResultReporter reporter("vector_math", story_name);
  reporter.RegisterImportantMetric("_fmac", "runs/s");
  reporter.RegisterImportantMetric("_fmul", "runs/s");
  reporter.RegisterImportantMetric("_ewma_and_max_power", "runs/s");
  return reporter;
}

}  // namespace

namespace media {

static const int kBenchmarkIterations = 200000;
static const int kEWMABenchmarkIterations = 50000;
static const float kScale = 0.5;
static const int kVectorSize = 8192;

class VectorMathPerfTest : public testing::Test {
 public:
  VectorMathPerfTest() {
    // Initialize input and output vectors.
    input_vector_.reset(static_cast<float*>(base::AlignedAlloc(
        sizeof(float) * kVectorSize, vector_math::kRequiredAlignment)));
    output_vector_.reset(static_cast<float*>(base::AlignedAlloc(
        sizeof(float) * kVectorSize, vector_math::kRequiredAlignment)));
    fill(input_vector_.get(), input_vector_.get() + kVectorSize, 1.0f);
    fill(output_vector_.get(), output_vector_.get() + kVectorSize, 0.0f);
  }

  void RunBenchmark(void (*fn)(const float[], float, int, float[]),
                    bool aligned,
                    const std::string& metric_suffix,
                    const std::string& trace_name) {
    TimeTicks start = TimeTicks::Now();
    for (int i = 0; i < kBenchmarkIterations; ++i) {
      fn(input_vector_.get(),
         kScale,
         kVectorSize - (aligned ? 0 : 1),
         output_vector_.get());
    }
    double total_time_seconds = (TimeTicks::Now() - start).InSecondsF();
    perf_test::PerfResultReporter reporter = SetUpReporter(trace_name);
    reporter.AddResult(metric_suffix,
                       kBenchmarkIterations / total_time_seconds);
  }

  void RunBenchmark(
      std::pair<float, float> (*fn)(float, const float[], int, float),
      int len,
      const std::string& metric_suffix,
      const std::string& trace_name) {
    TimeTicks start = TimeTicks::Now();
    for (int i = 0; i < kEWMABenchmarkIterations; ++i) {
      fn(0.5f, input_vector_.get(), len, 0.1f);
    }
    double total_time_seconds = (TimeTicks::Now() - start).InSecondsF();
    perf_test::PerfResultReporter reporter = SetUpReporter(trace_name);
    reporter.AddResult(metric_suffix,
                       kEWMABenchmarkIterations / total_time_seconds);
  }

 protected:
  std::unique_ptr<float, base::AlignedFreeDeleter> input_vector_;
  std::unique_ptr<float, base::AlignedFreeDeleter> output_vector_;

  DISALLOW_COPY_AND_ASSIGN(VectorMathPerfTest);
};

// Define platform dependent function names for SIMD optimized methods.
#if defined(ARCH_CPU_X86_FAMILY)
#define FMAC_FUNC FMAC_SSE
#define FMUL_FUNC FMUL_SSE
#define EWMAAndMaxPower_FUNC EWMAAndMaxPower_SSE
#elif defined(ARCH_CPU_ARM_FAMILY) && defined(USE_NEON)
#define FMAC_FUNC FMAC_NEON
#define FMUL_FUNC FMUL_NEON
#define EWMAAndMaxPower_FUNC EWMAAndMaxPower_NEON
#endif

// Benchmarks for each optimized vector_math::FMAC() method.
// Benchmark FMAC_C().
TEST_F(VectorMathPerfTest, FMAC_unoptimized) {
  RunBenchmark(vector_math::FMAC_C, true, "_fmac", "unoptimized");
}

#if defined(FMAC_FUNC)
// Benchmark FMAC_FUNC() with unaligned size.
TEST_F(VectorMathPerfTest, FMAC_optimized_unaligned) {
  ASSERT_NE((kVectorSize - 1) % (vector_math::kRequiredAlignment /
                                 sizeof(float)), 0U);
  RunBenchmark(vector_math::FMAC_FUNC, false, "_fmac", "optimized_unaligned");
}

// Benchmark FMAC_FUNC() with aligned size.
TEST_F(VectorMathPerfTest, FMAC_optimized_aligned) {
  ASSERT_EQ(kVectorSize % (vector_math::kRequiredAlignment / sizeof(float)),
            0U);
  RunBenchmark(vector_math::FMAC_FUNC, true, "_fmac", "optimized_aligned");
}
#endif

// Benchmarks for each optimized vector_math::FMUL() method.
// Benchmark FMUL_C().
TEST_F(VectorMathPerfTest, FMUL_unoptimized) {
  RunBenchmark(vector_math::FMUL_C, true, "_fmul", "unoptimized");
}

#if defined(FMUL_FUNC)
// Benchmark FMUL_FUNC() with unaligned size.
TEST_F(VectorMathPerfTest, FMUL_optimized_unaligned) {
  ASSERT_NE((kVectorSize - 1) % (vector_math::kRequiredAlignment /
                                 sizeof(float)), 0U);
  RunBenchmark(vector_math::FMUL_FUNC, false, "_fmul", "optimized_unaligned");
}

// Benchmark FMUL_FUNC() with aligned size.
TEST_F(VectorMathPerfTest, FMUL_optimized_aligned) {
  ASSERT_EQ(kVectorSize % (vector_math::kRequiredAlignment / sizeof(float)),
            0U);
  RunBenchmark(vector_math::FMUL_FUNC, true, "_fmul", "optimized_aligned");
}
#endif

// Benchmarks for each optimized vector_math::EWMAAndMaxPower() method.
// Benchmark EWMAAndMaxPower_C().
TEST_F(VectorMathPerfTest, EWMAAndMaxPower_unoptimized) {
  RunBenchmark(vector_math::EWMAAndMaxPower_C, kVectorSize,
               "_ewma_and_max_power", "unoptimized");
}

#if defined(EWMAAndMaxPower_FUNC)
// Benchmark EWMAAndMaxPower_FUNC() with unaligned size.
TEST_F(VectorMathPerfTest, EWMAAndMaxPower_optimized_unaligned) {
  ASSERT_NE((kVectorSize - 1) % (vector_math::kRequiredAlignment /
                                 sizeof(float)), 0U);
  RunBenchmark(vector_math::EWMAAndMaxPower_FUNC, kVectorSize - 1,
               "_ewma_and_max_power", "optimized_unaligned");
}

// Benchmark EWMAAndMaxPower_FUNC() with aligned size.
TEST_F(VectorMathPerfTest, EWMAAndMaxPower_optimized_aligned) {
  ASSERT_EQ(kVectorSize % (vector_math::kRequiredAlignment / sizeof(float)),
            0U);
  RunBenchmark(vector_math::EWMAAndMaxPower_FUNC, kVectorSize,
               "_ewma_and_max_power", "optimized_aligned");
}
#endif

} // namespace media
