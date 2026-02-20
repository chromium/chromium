// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/text/string_impl.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class StringSIMDPerfTest : public testing::Test {};

TEST_F(StringSIMDPerfTest, EqualIgnoringAsciiCase_LChar_Large) {
  const int kSize = 10000;
  const int kNumIterations = 100000;

  Vector<LChar> v1(kSize);
  Vector<LChar> v2(kSize);
  for (int i = 0; i < kSize; ++i) {
    v1[i] = 'a' + (i % 26);
    v2[i] = 'A' + (i % 26);
  }
  v1[kSize - 1] = 'x';

  base::TimeTicks start = base::TimeTicks::Now();
  int count = 0;
  for (int i = 0; i < kNumIterations; ++i) {
    if (EqualIgnoringAsciiCase(base::span<const LChar>(v1),
                               base::span<const LChar>(v2))) {
      count++;
    }
  }
  base::TimeTicks end = base::TimeTicks::Now();
  double time_per_op = (end - start).InMicrosecondsF() / kNumIterations;
  LOG(INFO) << "EqualIgnoringAsciiCase_LChar_Large (10k): " << time_per_op
            << " us/op (count=" << count << ")";
}

TEST_F(StringSIMDPerfTest, EqualIgnoringAsciiCase_LChar_Small) {
  const int kSize = 10;  // Smaller than SIMD lane size (16)
  const int kNumIterations = 10000000;

  Vector<LChar> v1(kSize);
  Vector<LChar> v2(kSize);
  for (int i = 0; i < kSize; ++i) {
    v1[i] = 'a' + (i % 26);
    v2[i] = 'A' + (i % 26);
  }
  // Make them match exactly

  base::TimeTicks start = base::TimeTicks::Now();
  int count = 0;
  for (int i = 0; i < kNumIterations; ++i) {
    if (EqualIgnoringAsciiCase(base::span<const LChar>(v1),
                               base::span<const LChar>(v2))) {
      count++;
    }
  }
  base::TimeTicks end = base::TimeTicks::Now();
  double time_per_op = (end - start).InMicrosecondsF() / kNumIterations;
  LOG(INFO) << "EqualIgnoringAsciiCase_LChar_Small (10): " << time_per_op
            << " us/op (count=" << count << ")";
}

TEST_F(StringSIMDPerfTest, EqualIgnoringAsciiCase_LChar_Medium) {
  const int kSize = 20;  // Slightly larger than one vector
  const int kNumIterations = 10000000;

  Vector<LChar> v1(kSize);
  Vector<LChar> v2(kSize);
  for (int i = 0; i < kSize; ++i) {
    v1[i] = 'a' + (i % 26);
    v2[i] = 'A' + (i % 26);
  }

  base::TimeTicks start = base::TimeTicks::Now();
  int count = 0;
  for (int i = 0; i < kNumIterations; ++i) {
    if (EqualIgnoringAsciiCase(base::span<const LChar>(v1),
                               base::span<const LChar>(v2))) {
      count++;
    }
  }
  base::TimeTicks end = base::TimeTicks::Now();
  double time_per_op = (end - start).InMicrosecondsF() / kNumIterations;
  LOG(INFO) << "EqualIgnoringAsciiCase_LChar_Medium (20): " << time_per_op
            << " us/op (count=" << count << ")";
}

TEST_F(StringSIMDPerfTest, CopyChars_LCharToUChar) {
  const int kSize = 10000;
  const int kNumIterations = 100000;

  Vector<LChar> src(kSize);
  for (int i = 0; i < kSize; ++i) {
    src[i] = 'a' + (i % 26);
  }
  Vector<UChar> dst(kSize);

  base::TimeTicks start = base::TimeTicks::Now();
  for (int i = 0; i < kNumIterations; ++i) {
    StringImpl::CopyChars(base::span<UChar>(dst), base::span<const LChar>(src));
  }
  base::TimeTicks end = base::TimeTicks::Now();
  double time_per_op = (end - start).InMicrosecondsF() / kNumIterations;
  LOG(INFO) << "CopyChars_LCharToUChar: " << time_per_op << " us/op";
}

}  // namespace blink
