// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/text/utf8.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_uchar.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink::unicode {

class UTF8PerfTest : public testing::Test {};

TEST_F(UTF8PerfTest, ConvertUtf16ToUtf8_ASCII) {
  const int kSize = 10000;
  const int kNumIterations = 100000;

  Vector<UChar> src(kSize);
  for (int i = 0; i < kSize; ++i) {
    src[i] = 'a' + (i % 26);
  }

  std::vector<uint8_t> dst(kSize * 3);  // Enough space for UTF-8

  base::TimeTicks start = base::TimeTicks::Now();
  for (int i = 0; i < kNumIterations; ++i) {
    ConvertUtf16ToUtf8(src, dst, true);
  }
  base::TimeTicks end = base::TimeTicks::Now();
  double time_per_op = (end - start).InMicrosecondsF() / kNumIterations;
  LOG(INFO) << "ConvertUtf16ToUtf8_ASCII (10k): " << time_per_op << " us/op";
}

TEST_F(UTF8PerfTest, ConvertUtf16ToUtf8_NonASCII) {
  const int kSize = 10000;
  const int kNumIterations = 100000;

  Vector<UChar> src(kSize);
  for (int i = 0; i < kSize; ++i) {
    src[i] = 0x0800 + (i % 26);  // 3-byte UTF-8
  }

  std::vector<uint8_t> dst(kSize * 3);

  base::TimeTicks start = base::TimeTicks::Now();
  for (int i = 0; i < kNumIterations; ++i) {
    ConvertUtf16ToUtf8(src, dst, false);
  }
  base::TimeTicks end = base::TimeTicks::Now();
  double time_per_op = (end - start).InMicrosecondsF() / kNumIterations;
  LOG(INFO) << "ConvertUtf16ToUtf8_NonASCII (10k): " << time_per_op << " us/op";
}

TEST_F(UTF8PerfTest, ConvertUtf16ToUtf8_Strict_NonASCII) {
  const int kSize = 10000;
  const int kNumIterations = 100000;

  Vector<UChar> src(kSize);
  for (int i = 0; i < kSize; ++i) {
    src[i] = 0x0800 + (i % 26);  // 3-byte UTF-8
  }

  std::vector<uint8_t> dst(kSize * 3);

  base::TimeTicks start = base::TimeTicks::Now();
  for (int i = 0; i < kNumIterations; ++i) {
    ConvertUtf16ToUtf8(src, dst, true);
  }
  base::TimeTicks end = base::TimeTicks::Now();
  double time_per_op = (end - start).InMicrosecondsF() / kNumIterations;
  LOG(INFO) << "ConvertUtf16ToUtf8_Strict_NonASCII (10k): " << time_per_op
            << " us/op";
}

}  // namespace blink::unicode
