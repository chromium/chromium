// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/audio/vector_math.h"

#include <cmath>
#include <limits>
#include <numeric>
#include <random>

#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink::vector_math {
namespace {

struct MemoryLayout {
  size_t byte_alignment;
  size_t stride;
};

// This is the minimum aligned needed by AVX on x86 family architectures.
constexpr size_t kMaxBitAlignment = 256u;
constexpr size_t kMaxByteAlignment = kMaxBitAlignment / 8u;

constexpr size_t kMaxStride = 2u;

constexpr MemoryLayout kMemoryLayouts[] = {
    {kMaxByteAlignment / 4u, 1u},
    {kMaxByteAlignment / 2u, 1u},
    {kMaxByteAlignment / 2u + kMaxByteAlignment / 4u, 1u},
    {kMaxByteAlignment, 1u},
    {0u, kMaxStride}};
constexpr size_t kMemoryLayoutCount =
    sizeof(kMemoryLayouts) / sizeof(*kMemoryLayouts);

// This is the minimum vector size in bytes needed for MSA instructions on
// MIPS.
constexpr size_t kMaxVectorSizeInBytes = 1024u;
constexpr size_t kVectorSizesInBytes[] = {
    kMaxVectorSizeInBytes,
    // This vector size in bytes is chosen so that the following optimization
    // paths can be tested on x86 family architectures using different memory
    // layouts:
    //  * AVX + SSE + scalar
    //  * scalar + SSE + AVX
    //  * SSE + AVX + scalar
    //  * scalar + AVX + SSE
    // On other architectures, this vector size in bytes results in either
    // optimization + scalar path or scalar path to be tested.
    kMaxByteAlignment + kMaxByteAlignment / 2u + kMaxByteAlignment / 4u};
constexpr size_t kVectorSizeCount =
    sizeof(kVectorSizesInBytes) / sizeof(*kVectorSizesInBytes);

// Compare two floats and consider all NaNs to be equal.
bool Equal(float a, float b) {
  if (std::isnan(a)) {
    return std::isnan(b);
  }
  return a == b;
}

// This represents a real source or destination vector which is aligned, can be
// non-contiguous and can be used as a source or destination vector for
// blink::vector_math functions.
template <typename T>
class TestVector {
  DISALLOW_NEW();

  class Iterator {
    STACK_ALLOCATED();

   public:
    // These types are used by std::iterator_traits used by base::ranges::equal
    // used by TestVector::operator==.
    using difference_type = ptrdiff_t;
    using iterator_category = std::bidirectional_iterator_tag;
    using pointer = T*;
    using reference = T&;
    using value_type = T;

    constexpr Iterator() = default;

    Iterator(T* p, int stride) : p_(p), stride_(stride) {}

    Iterator& operator++() {
      p_ += stride_;
      return *this;
    }
    Iterator operator++(int) {
      Iterator iter = *this;
      ++(*this);
      return iter;
    }
    Iterator& operator--() {
      p_ -= stride_;
      return *this;
    }
    Iterator operator--(int) {
      Iterator iter = *this;
      --(*this);
      return iter;
    }
    bool operator==(const Iterator& other) const { return p_ == other.p_; }
    bool operator!=(const Iterator& other) const { return !(*this == other); }
    T& operator*() const { return *p_; }

   private:
    T* p_ = nullptr;
    size_t stride_ = 0;
  };

 public:
  using ReverseIterator = std::reverse_iterator<Iterator>;

  // These types are used internally by Google Test.
  using const_iterator = Iterator;
  using iterator = Iterator;

  TestVector() = default;
  TestVector(T* base, const MemoryLayout* memory_layout, size_t size)
      : p_(GetAligned(base, memory_layout->byte_alignment)),
        memory_layout_(memory_layout),
        size_(size) {}
  TestVector(T* base, const TestVector<const T>& primary_vector)
      : TestVector(base,
                   primary_vector.memory_layout(),
                   primary_vector.size()) {}

  Iterator begin() const { return Iterator(p_.get(), stride()); }
  Iterator end() const { return Iterator(p_ + size() * stride(), stride()); }
  ReverseIterator rbegin() const { return ReverseIterator(end()); }
  ReverseIterator rend() const { return ReverseIterator(begin()); }
  const MemoryLayout* memory_layout() const { return memory_layout_; }
  T* p() const { return p_; }
  size_t size() const { return size_; }
  int stride() const { return static_cast<int>(memory_layout()->stride); }

  bool operator==(const TestVector& other) const {
    return base::ranges::equal(*this, other, Equal);
  }
  T& operator[](size_t i) const { return p_[i * stride()]; }

 private:
  static T* GetAligned(T* base, size_t byte_alignment) {
    size_t base_byte_alignment = GetByteAlignment(base);
    size_t byte_offset =
        (byte_alignment - base_byte_alignment + kMaxByteAlignment) %
        kMaxByteAlignment;
    T* p = base + byte_offset / sizeof(T);
    size_t p_byte_alignment = GetByteAlignment(p);
    CHECK_EQ(byte_alignment % kMaxByteAlignment, p_byte_alignment);
    return p;
  }
  static size_t GetByteAlignment(T* p) {
    return reinterpret_cast<size_t>(p) % kMaxByteAlignment;
  }

  raw_ptr<T, AllowPtrArithmetic> p_;
  raw_ptr<const MemoryLayout> memory_layout_;
  size_t size_;
};

// Get primary input vectors with difference memory layout and size
// combinations.
template <typename T>
Vector<TestVector<const T>> GetPrimaryVectors(const T* base) {
  Vector<TestVector<const T>> vectors(kVectorSizeCount * kMemoryLayoutCount);
  for (auto& vector : vectors) {
    ptrdiff_t i = &vector - &vectors[0];
    ptrdiff_t memory_layout_index = i % kMemoryLayoutCount;
    ptrdiff_t size_index = i / kMemoryLayoutCount;
    vector = TestVector<const T>(base, &kMemoryLayouts[memory_layout_index],
                                 kVectorSizesInBytes[size_index] / sizeof(T));
  }
  return vectors;
}

// Get secondary input or output vectors. As the size of a secondary vector
// must always be the same as the size of the primary input vector, there are
// only  two interesting secondary vectors:
//  - A vector with the same memory layout as the primary input vector has and
//    which therefore is aligned whenever the primary input vector is aligned.
//  - A vector with a different memory layout than the primary input vector has
//    and which therefore is not aligned when the primary input vector is
//    aligned.
template <typename T>
Vector<TestVector<T>> GetSecondaryVectors(
    T* base,
    const MemoryLayout* primary_memory_layout,
    size_t size) {
  Vector<TestVector<T>> vectors(2u);
  const MemoryLayout* other_memory_layout =
      &kMemoryLayouts[primary_memory_layout == &kMemoryLayouts[0]];
  CHECK_NE(primary_memory_layout, other_memory_layout);
  CHECK_NE(primary_memory_layout->byte_alignment,
           other_memory_layout->byte_alignment);
  vectors[0] = TestVector<T>(base, primary_memory_layout, size);
  vectors[1] = TestVector<T>(base, other_memory_layout, size);
  return vectors;
}

template <typename T>
Vector<TestVector<T>> GetSecondaryVectors(
    T* base,
    const TestVector<const float>& primary_vector) {
  return GetSecondaryVectors(base, primary_vector.memory_layout(),
                             primary_vector.size());
}

class VectorMathTest : public testing::Test {
 protected:
  enum {
    kDestinationCount = 4u,
    kFloatArraySize =
        (kMaxStride * kMaxVectorSizeInBytes + kMaxByteAlignment - 1u) /
        sizeof(float),
    kFullyFiniteSource = 4u,
    kFullyFiniteSource2 = 5u,
    kFullyNonNanSource = 6u,
    kSourceCount = 7u
  };

  // Get a destination buffer containing initially uninitialized floats.
  float* GetDestination(size_t i) {
    CHECK_LT(i, static_cast<size_t>(kDestinationCount));
    return destinations_[i];
  }
  // Get a source buffer containing random floats.
  const float* GetSource(size_t i) {
    CHECK_LT(i, static_cast<size_t>(kSourceCount));
    return sources_[i];
  }

  static void SetUpTestSuite() {
    std::minstd_rand generator(3141592653u);
    // Fill in source buffers with finite random floats.
    std::uniform_real_distribution<float> float_distribution(-10.0f, 10.0f);
    std::generate_n(&**sources_, sizeof(sources_) / sizeof(**sources_),
                    [&]() { return float_distribution(generator); });
    // Add INFINITYs and NANs to most source buffers.
    std::uniform_int_distribution<size_t> index_distribution(
        0u, kFloatArraySize / 2u - 1u);
    for (size_t i = 0u; i < kSourceCount; ++i) {
      if (i == kFullyFiniteSource || i == kFullyFiniteSource2) {
        continue;
      }
      sources_[i][index_distribution(generator)] = INFINITY;
      sources_[i][index_distribution(generator)] = -INFINITY;
      if (i != kFullyNonNanSource) {
        sources_[i][index_distribution(generator)] = NAN;
      }
    }
  }

 private:
  static float destinations_[kDestinationCount][kFloatArraySize];
  static float sources_[kSourceCount][kFloatArraySize];
};

float VectorMathTest::destinations_[kDestinationCount][kFloatArraySize];
float VectorMathTest::sources_[kSourceCount][kFloatArraySize];

TEST_F(VectorMathTest, Conv) {
  for (const auto& source : GetPrimaryVectors(GetSource(kFullyFiniteSource))) {
    if (source.stride() != 1) {
      continue;
    }
    for (size_t filter_size : {3u, 32u, 64u, 128u}) {
      // The maximum number of frames which could be processed here is
      // |source.size() - filter_size + 1|. However, in order to test
      // optimization paths, |frames_to_process| should be optimal (divisible
      // by a power of 2) whenever |filter_size| is optimal. Therefore, let's
      // process only |source.size() - filter_size| frames here.
      if (filter_size >= source.size()) {
        break;
      }
      uint32_t frames_to_process = source.size() - filter_size;
      // The stride of a convolution filter must be -1. Let's first create
      // a reversed filter whose stride is 1.
      TestVector<const float> reversed_filter(
          GetSource(kFullyFiniteSource2), source.memory_layout(), filter_size);
      // The filter begins from the reverse beginning of the reversed filter
      // and grows downwards.
      const float* filter_p = &*reversed_filter.rbegin();
      TestVector<float> expected_dest(
          GetDestination(0u), source.memory_layout(), frames_to_process);
      for (size_t i = 0u; i < frames_to_process; ++i) {
        expected_dest[i] = 0u;
        for (size_t j = 0u; j < filter_size; ++j) {
          expected_dest[i] += source[i + j] * *(filter_p - j);
        }
      }
      for (auto& dest : GetSecondaryVectors(
               GetDestination(1u), source.memory_layout(), frames_to_process)) {
        AudioFloatArray prepared_filter;
        PrepareFilterForConv(filter_p, -1, filter_size, &prepared_filter);
        Conv(source.p(), 1, filter_p, -1, dest.p(), 1, frames_to_process,
             filter_size, &prepared_filter);
        for (size_t i = 0u; i < frames_to_process; ++i) {
          EXPECT_NEAR(expected_dest[i], dest[i],
                      1e-3 * std::abs(expected_dest[i]));
        }
      }
    }
  }
}

TEST_F(VectorMathTest, Vadd) {
  for (const auto& source1 : GetPrimaryVectors(GetSource(0u))) {
    for (const auto& source2 : GetSecondaryVectors(GetSource(1u), source1)) {
      TestVector<float> expected_dest(GetDestination(0u), source1);
      for (size_t i = 0u; i < source1.size(); ++i) {
        expected_dest[i] = source1[i] + source2[i];
      }
      for (auto& dest : GetSecondaryVectors(GetDestination(1u), source1)) {
        Vadd(source1.p(), source1.stride(), source2.p(), source2.stride(),
             dest.p(), dest.stride(), source1.size());
        EXPECT_EQ(expected_dest, dest);
      }
    }
  }
}

TEST_F(VectorMathTest, Vsub) {
  for (const auto& source1 : GetPrimaryVectors(GetSource(0u))) {
    for (const auto& source2 : GetSecondaryVectors(GetSource(1u), source1)) {
      TestVector<float> expected_dest(GetDestination(0u), source1);
      for (size_t i = 0u; i < source1.size(); ++i) {
        expected_dest[i] = source1[i] - source2[i];
      }
      for (auto& dest : GetSecondaryVectors(GetDestination(1u), source1)) {
        Vsub(source1.p(), source1.stride(), source2.p(), source2.stride(),
             dest.p(), dest.stride(), source1.size());
        EXPECT_EQ(expected_dest, dest);
      }
    }
  }
}

TEST_F(VectorMathTest, Vclip) {
  // Vclip does not accept NaNs thus let's use only sources without NaNs.
  for (const auto& source : GetPrimaryVectors(GetSource(kFullyNonNanSource))) {
    const float* thresholds = GetSource(kFullyFiniteSource);
    const float low_threshold = std::min(thresholds[0], thresholds[1]);
    const float high_threshold = std::max(thresholds[0], thresholds[1]);
    TestVector<float> expected_dest(GetDestination(0u), source);
    for (size_t i = 0u; i < source.size(); ++i) {
      expected_dest[i] = ClampTo(source[i], low_threshold, high_threshold);
    }
    for (auto& dest : GetSecondaryVectors(GetDestination(1u), source)) {
      Vclip(source.p(), source.stride(), &low_threshold, &high_threshold,
            dest.p(), dest.stride(), source.size());
      EXPECT_EQ(expected_dest, dest);
    }
  }
}

TEST_F(VectorMathTest, Vmaxmgv) {
  const auto maxmg = [](float init, float x) {
    return std::max(init, std::abs(x));
  };
  // Vmaxmgv does not accept NaNs thus let's use only sources without NaNs.
  for (const float* source_base :
       {GetSource(kFullyFiniteSource), GetSource(kFullyNonNanSource)}) {
    for (const auto& source : GetPrimaryVectors(source_base)) {
      const float expected_max =
          std::accumulate(source.begin(), source.end(), 0.0f, maxmg);
      float max;
      Vmaxmgv(source.p(), source.stride(), &max, source.size());
      EXPECT_EQ(expected_max, max) << testing::PrintToString(source);
    }
  }
}

TEST_F(VectorMathTest, Vmul) {
  for (const auto& source1 : GetPrimaryVectors(GetSource(0u))) {
    for (const auto& source2 : GetSecondaryVectors(GetSource(1u), source1)) {
      TestVector<float> expected_dest(GetDestination(0u), source1);
      for (size_t i = 0u; i < source1.size(); ++i) {
        expected_dest[i] = source1[i] * source2[i];
      }
      for (auto& dest : GetSecondaryVectors(GetDestination(1u), source1)) {
        Vmul(source1.p(), source1.stride(), source2.p(), source2.stride(),
             dest.p(), dest.stride(), source1.size());
        EXPECT_EQ(expected_dest, dest);
      }
    }
  }
}

TEST_F(VectorMathTest, Vsma) {
  for (const auto& source : GetPrimaryVectors(GetSource(0u))) {
    const float scale = *GetSource(1u);
    const TestVector<const float> dest_source(GetSource(2u), source);
    TestVector<float> expected_dest(GetDestination(0u), source);
    for (size_t i = 0u; i < source.size(); ++i) {
      expected_dest[i] = dest_source[i] + scale * source[i];
    }
    for (auto& dest : GetSecondaryVectors(GetDestination(1u), source)) {
      base::ranges::copy(dest_source, dest.begin());
      Vsma(source.p(), source.stride(), &scale, dest.p(), dest.stride(),
           source.size());
      // Different optimizations may use different precisions for intermediate
      // results which may result in different rounding errors thus let's
      // expect only mostly equal floats.
      for (size_t i = 0u; i < source.size(); ++i) {
        if (std::isfinite(expected_dest[i])) {
#if BUILDFLAG(IS_MAC)
          // On Mac, OS provided vectorized functions are used which may result
          // in bigger rounding errors than functions used on other OSes.
          EXPECT_NEAR(expected_dest[i], dest[i],
                      1e-5 * std::abs(expected_dest[i]));
#else
          EXPECT_FLOAT_EQ(expected_dest[i], dest[i]);
#endif
        } else {
          EXPECT_PRED2(Equal, expected_dest[i], dest[i]);
        }
      }
    }
  }
}

TEST_F(VectorMathTest, Vsmul) {
  for (const auto& source : GetPrimaryVectors(GetSource(0u))) {
    const float scale = *GetSource(1u);
    TestVector<float> expected_dest(GetDestination(0u), source);
    for (size_t i = 0u; i < source.size(); ++i) {
      expected_dest[i] = scale * source[i];
    }
    for (auto& dest : GetSecondaryVectors(GetDestination(1u), source)) {
      Vsmul(source.p(), source.stride(), &scale, dest.p(), dest.stride(),
            source.size());
      EXPECT_EQ(expected_dest, dest);
    }
  }
}

TEST_F(VectorMathTest, Vsadd) {
  for (const auto& source : GetPrimaryVectors(GetSource(0u))) {
    const float addend = *GetSource(1u);
    TestVector<float> expected_dest(GetDestination(0u), source);
    for (size_t i = 0u; i < source.size(); ++i) {
      expected_dest[i] = addend + source[i];
    }
    for (auto& dest : GetSecondaryVectors(GetDestination(1u), source)) {
      Vsadd(source.p(), source.stride(), &addend, dest.p(), dest.stride(),
            source.size());
      EXPECT_EQ(expected_dest, dest);
    }
  }
}

TEST_F(VectorMathTest, Vsvesq) {
  const auto sqsum = [](float init, float x) { return init + x * x; };
  for (const float* source_base :
       {GetSource(0u), GetSource(kFullyFiniteSource)}) {
    for (const auto& source : GetPrimaryVectors(source_base)) {
      const float expected_sum =
          std::accumulate(source.begin(), source.end(), 0.0f, sqsum);
      float sum;
      Vsvesq(source.p(), source.stride(), &sum, source.size());
      if (std::isfinite(expected_sum)) {
        // Optimized paths in Vsvesq use parallel partial sums which may result
        // in different rounding errors than the non-partial sum algorithm used
        // here and in non-optimized paths in Vsvesq.
        EXPECT_FLOAT_EQ(expected_sum, sum);
      } else {
        EXPECT_PRED2(Equal, expected_sum, sum);
      }
    }
  }
}

TEST_F(VectorMathTest, Zvmul) {
  constexpr float kMax = std::numeric_limits<float>::max();
  Vector<Vector<float>> sources(4u);
  for (size_t i = 0u; i < sources.size(); ++i) {
    sources[i].resize(kFloatArraySize);
    // Initialize a local source with a randomized test case source.
    std::copy_n(GetSource(i), kFloatArraySize, sources[i].begin());
    // Put +FLT_MAX and -FLT_MAX in the middle of the source. Use a different
    // sequence for each source in order to get 16 different combinations.
    for (size_t j = 0u; j < 16u; ++j) {
      sources[i][kFloatArraySize / 2u + j] = ((j >> i) & 1) ? -kMax : kMax;
    }
  }
  for (const auto& real1 : GetPrimaryVectors(sources[0u].data())) {
    if (real1.stride() != 1) {
      continue;
    }
    const TestVector<const float> imag1(sources[1u].data(), real1);
    const TestVector<const float> real2(sources[2u].data(), real1);
    const TestVector<const float> imag2(sources[3u].data(), real1);
    TestVector<float> expected_dest_real(GetDestination(0u), real1);
    TestVector<float> expected_dest_imag(GetDestination(1u), real1);
    for (size_t i = 0u; i < real1.size(); ++i) {
      expected_dest_real[i] = real1[i] * real2[i] - imag1[i] * imag2[i];
      expected_dest_imag[i] = real1[i] * imag2[i] + imag1[i] * real2[i];
      if (&real1[i] >= &sources[0u][kFloatArraySize / 2u] &&
          &real1[i] < &sources[0u][kFloatArraySize / 2u] + 16u) {
        // FLT_MAX products should have overflowed.
        EXPECT_TRUE(std::isinf(expected_dest_real[i]) ||
                    std::isnan(expected_dest_real[i]));
        EXPECT_TRUE(std::isinf(expected_dest_imag[i]) ||
                    std::isnan(expected_dest_imag[i]));
      }
    }
    for (auto& dest_real : GetSecondaryVectors(GetDestination(2u), real1)) {
      TestVector<float> dest_imag(GetDestination(3u), real1);
      ASSERT_EQ(1, dest_real.stride());
      Zvmul(real1.p(), imag1.p(), real2.p(), imag2.p(), dest_real.p(),
            dest_imag.p(), real1.size());
      // Different optimizations may use different precisions for intermediate
      // results which may result in different rounding errors thus let's
      // expect only mostly equal floats.
#if BUILDFLAG(IS_MAC)
#if defined(ARCH_CPU_ARM64)
      const float threshold = 1.900e-5;
#else
      const float threshold = 1.5e-5;
#endif
#endif
      for (size_t i = 0u; i < real1.size(); ++i) {
        if (std::isfinite(expected_dest_real[i])) {
#if BUILDFLAG(IS_MAC)
          // On Mac, OS provided vectorized functions are used which may result
          // in bigger rounding errors than functions used on other OSes.
          EXPECT_NEAR(expected_dest_real[i], dest_real[i],
                      threshold * std::abs(expected_dest_real[i]));
#else
          EXPECT_FLOAT_EQ(expected_dest_real[i], dest_real[i]);
#endif
        } else {
#if BUILDFLAG(IS_MAC)
          // On Mac, OS provided vectorized functions are used which may result
          // in different NaN handling than functions used on other OSes.
          EXPECT_TRUE(!std::isfinite(dest_real[i]));
#else
          EXPECT_PRED2(Equal, expected_dest_real[i], dest_real[i]);
#endif
        }
        if (std::isfinite(expected_dest_imag[i])) {
#if BUILDFLAG(IS_MAC)
          // On Mac, OS provided vectorized functions are used which may result
          // in bigger rounding errors than functions used on other OSes.
          EXPECT_NEAR(expected_dest_imag[i], dest_imag[i],
                      1e-5 * std::abs(expected_dest_imag[i]));
#else
          EXPECT_FLOAT_EQ(expected_dest_imag[i], dest_imag[i]);
#endif
        } else {
#if BUILDFLAG(IS_MAC)
          // On Mac, OS provided vectorized functions are used which may result
          // in different NaN handling than functions used on other OSes.
          EXPECT_TRUE(!std::isfinite(dest_imag[i]));
#else
          EXPECT_PRED2(Equal, expected_dest_imag[i], dest_imag[i]);
#endif
        }
      }
    }
  }
}

}  // namespace
}  // namespace blink::vector_math
