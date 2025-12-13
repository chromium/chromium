// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_frame.h"

#include <stddef.h>

#include <algorithm>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"

namespace net {

namespace {

constexpr int kIterations = 100000;
constexpr int kLongPayloadSize = 1 << 16;
constexpr std::string_view kMaskingKey = "\xFE\xED\xBE\xEF";

static constexpr char kMetricPrefixWebSocketFrame[] = "WebSocketFrameMask.";
static constexpr char kMetricMaskTimeMs[] = "mask_time";

perf_test::PerfResultReporter SetUpWebSocketFrameMaskReporter(
    const std::string& story) {
  perf_test::PerfResultReporter reporter(kMetricPrefixWebSocketFrame, story);
  reporter.RegisterImportantMetric(kMetricMaskTimeMs, "ms");
  return reporter;
}

static_assert(kMaskingKey.size() == WebSocketFrameHeader::kMaskingKeyLength,
              "incorrect masking key size");

class WebSocketFrameTestMaskBenchmark : public ::testing::Test {
 protected:
  void Benchmark(const char* const story, base::span<const char> payload) {
    std::vector<char> scratch(payload.begin(), payload.end());
    WebSocketMaskingKey masking_key;
    base::as_writable_byte_span(masking_key.key)
        .copy_from(base::as_byte_span(kMaskingKey));
    auto reporter = SetUpWebSocketFrameMaskReporter(story);
    base::ElapsedTimer timer;
    for (int x = 0; x < kIterations; ++x) {
      MaskWebSocketFramePayload(masking_key, x % payload.size(),
                                base::as_writable_byte_span(scratch));
    }
    reporter.AddResult(kMetricMaskTimeMs, timer.Elapsed().InMillisecondsF());
  }
};

TEST_F(WebSocketFrameTestMaskBenchmark, BenchmarkMaskShortPayload) {
  static constexpr char kShortPayload[] = "Short Payload";
  Benchmark("short_payload", base::span_with_nul_from_cstring(kShortPayload));
}

TEST_F(WebSocketFrameTestMaskBenchmark, BenchmarkMaskLongPayload) {
  std::vector<char> payload(kLongPayloadSize, 'a');
  Benchmark("long_payload", base::span(payload));
}

// A 31-byte payload is guaranteed to do 7 byte mask operations and 3 vector
// mask operations with an 8-byte vector. With a 16-byte vector it will fall
// back to the byte-only code path and do 31 byte mask operations.
TEST_F(WebSocketFrameTestMaskBenchmark, Benchmark31BytePayload) {
  std::vector<char> payload(31, 'a');
  Benchmark("31_payload", base::span(payload));
}

}  // namespace

}  // namespace net
