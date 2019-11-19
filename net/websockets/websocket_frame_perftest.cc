// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_frame.h"

#include <algorithm>
#include <vector>

#include "base/stl_util.h"
#include "base/timer/elapsed_timer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"

namespace net {

namespace {

const int kIterations = 100000;
const int kLongPayloadSize = 1 << 16;
const char kMaskingKey[] = "\xFE\xED\xBE\xEF";

static constexpr char kMetricPrefixWebSocketFrame[] = "WebSocketFrameMask.";
static constexpr char kMetricMaskTimeMs[] = "mask_time";

perf_test::PerfResultReporter SetUpWebSocketFrameMaskReporter(
    const std::string& story) {
  perf_test::PerfResultReporter reporter(kMetricPrefixWebSocketFrame, story);
  reporter.RegisterImportantMetric(kMetricMaskTimeMs, "ms");
  return reporter;
}

static_assert(base::size(kMaskingKey) ==
                  WebSocketFrameHeader::kMaskingKeyLength + 1,
              "incorrect masking key size");

class WebSocketFrameTestMaskBenchmark : public ::testing::Test {
 protected:
  void Benchmark(const char* const story,
                 const char* const payload,
                 size_t size) {
    std::vector<char> scratch(payload, payload + size);
    WebSocketMaskingKey masking_key;
    std::copy(kMaskingKey,
              kMaskingKey + WebSocketFrameHeader::kMaskingKeyLength,
              masking_key.key);
    auto reporter = SetUpWebSocketFrameMaskReporter(story);
    base::ElapsedTimer timer;
    for (int x = 0; x < kIterations; ++x) {
      MaskWebSocketFramePayload(masking_key, x % size, scratch.data(),
                                scratch.size());
    }
    reporter.AddResult(kMetricMaskTimeMs, timer.Elapsed().InMillisecondsF());
  }
};

TEST_F(WebSocketFrameTestMaskBenchmark, BenchmarkMaskShortPayload) {
  static const char kShortPayload[] = "Short Payload";
  Benchmark("short_payload", kShortPayload, base::size(kShortPayload));
}

TEST_F(WebSocketFrameTestMaskBenchmark, BenchmarkMaskLongPayload) {
  std::vector<char> payload(kLongPayloadSize, 'a');
  Benchmark("long_payload", payload.data(), payload.size());
}

// A 31-byte payload is guaranteed to do 7 byte mask operations and 3 vector
// mask operations with an 8-byte vector. With a 16-byte vector it will fall
// back to the byte-only code path and do 31 byte mask operations.
TEST_F(WebSocketFrameTestMaskBenchmark, Benchmark31BytePayload) {
  std::vector<char> payload(31, 'a');
  Benchmark("31_payload", payload.data(), payload.size());
}

}  // namespace

}  // namespace net
