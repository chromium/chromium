// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "media/base/vector_math.h"

#include <algorithm>
#include <memory>

#include "base/containers/span_writer.h"
#include "base/cpu.h"
#include "base/memory/aligned_memory.h"
#include "base/time/time.h"
#include "build/build_config.h"
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
  reporter.RegisterImportantMetric("_fclamp", "runs/s");
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
    input_vector_ = base::AlignedUninit<float>(kVectorSize,
                                               vector_math::kRequiredAlignment);
    output_vector_ = base::AlignedUninit<float>(
        kVectorSize, vector_math::kRequiredAlignment);
    std::ranges::fill(input_vector_, 1.0f);
    std::ranges::fill(output_vector_, 0.0f);
  }

  VectorMathPerfTest(const VectorMathPerfTest&) = delete;
  VectorMathPerfTest& operator=(const VectorMathPerfTest&) = delete;

  void RunBenchmark(void (*fn)(const float[], float, int, float[]),
                    bool aligned,
                    const std::string& metric_suffix,
                    const std::string& trace_name) {
    TimeTicks start = TimeTicks::Now();
    for (int i = 0; i < kBenchmarkIterations; ++i) {
      fn(input_vector_.data(), kScale, kVectorSize - (aligned ? 0 : 1),
         output_vector_.data());
    }
    double total_time_seconds = (TimeTicks::Now() - start).InSecondsF();
    perf_test::PerfResultReporter reporter = SetUpReporter(trace_name);
    reporter.AddResult(metric_suffix,
                       kBenchmarkIterations / total_time_seconds);
  }

  void RunClampingBenchmark(void (*fn)(const float[], int, float[]),
                            bool aligned,
                            const std::string& metric_suffix,
                            const std::string& trace_name) {
    FillInputWithUnclampedData();

    TimeTicks start = TimeTicks::Now();
    for (int i = 0; i < kBenchmarkIterations; ++i) {
      fn(input_vector_.data(), kVectorSize - (aligned ? 0 : 1),
         output_vector_.data());
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
      fn(0.5f, input_vector_.data(), len, 0.1f);
    }
    double total_time_seconds = (TimeTicks::Now() - start).InSecondsF();
    perf_test::PerfResultReporter reporter = SetUpReporter(trace_name);
    reporter.AddResult(metric_suffix,
                       kEWMABenchmarkIterations / total_time_seconds);
  }

 protected:
  base::AlignedHeapArray<float> input_vector_;
  base::AlignedHeapArray<float> output_vector_;

 private:
  // Fills `input_vector_` with repeating values, some of which are unclamped.
  void FillInputWithUnclampedData() {
    static const float kUnclampedInput[] = {-2.0, -1.0, -0.5, 0.0,
                                            0.5,  1.0,  2.0};
    auto input_span = base::span(kUnclampedInput);
    auto writer = base::SpanWriter(base::span(input_vector_));

    while (writer.remaining() > input_span.size()) {
      writer.Write(input_span);
    }

    if (writer.remaining()) {
      writer.Write(input_span.first(writer.remaining()));
    }
  }
};

// Benchmarks for each optimized vector_math::FMAC() method.
// Benchmark FMAC_C().
TEST_F(VectorMathPerfTest, FMAC_unoptimized) {
  RunBenchmark(vector_math::FMAC_C, true, "_fmac", "unoptimized");
}

// Benchmark FMAC_FUNC() with unaligned size.
TEST_F(VectorMathPerfTest, FMAC_optimized_unaligned) {
  ASSERT_NE((kVectorSize - 1) % (vector_math::kRequiredAlignment /
                                 sizeof(float)), 0U);
#if defined(ARCH_CPU_X86_FAMILY)
  base::CPU cpu;
  if (cpu.has_avx2() && cpu.has_fma3()) {
    RunBenchmark(vector_math::FMAC_AVX2, false, "_fmac", "optimized_unaligned");
  } else if (cpu.has_sse2()) {
    RunBenchmark(vector_math::FMAC_SSE, false, "_fmac", "optimized_unaligned");
  }
#elif defined(ARCH_CPU_ARM_FAMILY) && defined(USE_NEON)
  RunBenchmark(vector_math::FMAC_NEON, false, "_fmac", "optimized_unaligned");
#endif
}

// Benchmark FMAC_FUNC() with aligned size.
TEST_F(VectorMathPerfTest, FMAC_optimized_aligned) {
  ASSERT_EQ(kVectorSize % (vector_math::kRequiredAlignment / sizeof(float)),
            0U);
#if defined(ARCH_CPU_X86_FAMILY)
  base::CPU cpu;
  if (cpu.has_avx2() && cpu.has_fma3()) {
    RunBenchmark(vector_math::FMAC_AVX2, true, "_fmac", "optimized_aligned");
  } else if (cpu.has_sse2()) {
    RunBenchmark(vector_math::FMAC_SSE, true, "_fmac", "optimized_aligned");
  }
#elif defined(ARCH_CPU_ARM_FAMILY) && defined(USE_NEON)
  RunBenchmark(vector_math::FMAC_NEON, true, "_fmac", "optimized_aligned");
#endif
}

// Benchmarks for each optimized vector_math::FMUL() method.
// Benchmark FMUL_C().
TEST_F(VectorMathPerfTest, FMUL_unoptimized) {
  RunBenchmark(vector_math::FMUL_C, true, "_fmul", "unoptimized");
}

// Benchmark FMUL_FUNC() with unaligned size.
TEST_F(VectorMathPerfTest, FMUL_optimized_unaligned) {
  ASSERT_NE((kVectorSize - 1) % (vector_math::kRequiredAlignment /
                                 sizeof(float)), 0U);
#if defined(ARCH_CPU_X86_FAMILY)
  base::CPU cpu;
  if (cpu.has_avx2()) {
    RunBenchmark(vector_math::FMUL_AVX2, false, "_fmul", "optimized_unaligned");
  } else if (cpu.has_sse2()) {
    RunBenchmark(vector_math::FMUL_SSE, false, "_fmul", "optimized_unaligned");
  }
#elif defined(ARCH_CPU_ARM_FAMILY) && defined(USE_NEON)
  RunBenchmark(vector_math::FMUL_NEON, false, "_fmul", "optimized_unaligned");
#endif
}

// Benchmark FMUL_FUNC() with aligned size.
TEST_F(VectorMathPerfTest, FMUL_optimized_aligned) {
  ASSERT_EQ(kVectorSize % (vector_math::kRequiredAlignment / sizeof(float)),
            0U);
#if defined(ARCH_CPU_X86_FAMILY)
  base::CPU cpu;
  if (cpu.has_avx2()) {
    RunBenchmark(vector_math::FMUL_AVX2, true, "_fmul", "optimized_aligned");
  } else if (cpu.has_sse2()) {
    RunBenchmark(vector_math::FMUL_SSE, true, "_fmul", "optimized_aligned");
  }
#elif defined(ARCH_CPU_ARM_FAMILY) && defined(USE_NEON)
  RunBenchmark(vector_math::FMUL_NEON, true, "_fmul", "optimized_aligned");
#endif
}

// Benchmarks for each optimized vector_math::FCLAMP() method.
// Benchmark FCLAMP_C().
TEST_F(VectorMathPerfTest, FCLAMP_unoptimized) {
  RunClampingBenchmark(vector_math::FCLAMP_C, true, "_fclamp", "unoptimized");
}

// Benchmark FCLAMP_FUNC() with aligned size.
TEST_F(VectorMathPerfTest, FCLAMP_optimized_aligned) {
  ASSERT_EQ(kVectorSize % (vector_math::kRequiredAlignment / sizeof(float)),
            0U);
#if defined(ARCH_CPU_X86_FAMILY)
  base::CPU cpu;
  if (cpu.has_avx()) {
    RunClampingBenchmark(vector_math::FCLAMP_AVX, true, "_fclamp",
                         "optimized_aligned");
  } else {
    RunClampingBenchmark(vector_math::FCLAMP_SSE, true, "_fclamp",
                         "optimized_aligned");
  }
#elif defined(ARCH_CPU_ARM_FAMILY) && defined(USE_NEON)
  RunBenchmark(vector_math::FMUL_NEON, true, "_fclamp", "optimized_aligned");
#endif
}

// Benchmark FCLAMP_FUNC() with unaligned size.
TEST_F(VectorMathPerfTest, FCLAMP_optimized_unaligned) {
  ASSERT_NE(
      (kVectorSize - 1) % (vector_math::kRequiredAlignment / sizeof(float)),
      0U);
#if defined(ARCH_CPU_X86_FAMILY)
  base::CPU cpu;
  if (cpu.has_avx()) {
    RunClampingBenchmark(vector_math::FCLAMP_AVX, false, "_fclamp",
                         "optimized_unaligned");
  } else {
    RunClampingBenchmark(vector_math::FCLAMP_SSE, false, "_fclamp",
                         "optimized_unaligned");
  }
#elif defined(ARCH_CPU_ARM_FAMILY) && defined(USE_NEON)
  RunBenchmark(vector_math::FMUL_NEON, false, "_fclamp", "optimized_unaligned");
#endif
}

// Benchmarks for each optimized vector_math::EWMAAndMaxPower() method.
// Benchmark EWMAAndMaxPower_C().
TEST_F(VectorMathPerfTest, EWMAAndMaxPower_unoptimized) {
  RunBenchmark(vector_math::EWMAAndMaxPower_C, kVectorSize,
               "_ewma_and_max_power", "unoptimized");
}

// Benchmark EWMAAndMaxPower_FUNC() with unaligned size.
TEST_F(VectorMathPerfTest, EWMAAndMaxPower_optimized_unaligned) {
  ASSERT_NE((kVectorSize - 1) % (vector_math::kRequiredAlignment /
                                 sizeof(float)), 0U);
#if defined(ARCH_CPU_X86_FAMILY)
  base::CPU cpu;
  if (cpu.has_avx2() && cpu.has_fma3()) {
    RunBenchmark(vector_math::EWMAAndMaxPower_AVX2, kVectorSize - 1,
                 "_ewma_and_max_power", "optimized_unaligned");
  } else if (cpu.has_sse2()) {
    RunBenchmark(vector_math::EWMAAndMaxPower_SSE, kVectorSize - 1,
                 "_ewma_and_max_power", "optimized_unaligned");
  }
#elif defined(ARCH_CPU_ARM_FAMILY) && defined(USE_NEON)
  RunBenchmark(vector_math::EWMAAndMaxPower_NEON, kVectorSize - 1,
               "_ewma_and_max_power", "optimized_unaligned");
#endif
}

// Benchmark EWMAAndMaxPower_FUNC() with aligned size.
TEST_F(VectorMathPerfTest, EWMAAndMaxPower_optimized_aligned) {
  ASSERT_EQ(kVectorSize % (vector_math::kRequiredAlignment / sizeof(float)),
            0U);
#if defined(ARCH_CPU_X86_FAMILY)
  base::CPU cpu;
  if (cpu.has_avx2() && cpu.has_fma3()) {
    RunBenchmark(vector_math::EWMAAndMaxPower_AVX2, kVectorSize,
                 "_ewma_and_max_power", "optimized_aligned");
  } else if (cpu.has_sse2()) {
    RunBenchmark(vector_math::EWMAAndMaxPower_SSE, kVectorSize,
                 "_ewma_and_max_power", "optimized_aligned");
  }
#elif defined(ARCH_CPU_ARM_FAMILY) && defined(USE_NEON)
  RunBenchmark(vector_math::EWMAAndMaxPower_NEON, kVectorSize,
               "_ewma_and_max_power", "optimized_aligned");
#endif
}

} // namespace media
