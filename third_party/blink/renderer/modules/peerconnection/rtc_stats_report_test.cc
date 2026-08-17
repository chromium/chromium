// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_stats_report.h"

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_inbound_rtp_stream_stats.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_stats.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/webrtc/api/stats/rtcstats_objects.h"
#include "third_party/webrtc/api/units/timestamp.h"

namespace blink {

// crbug.com/545843236
TEST(RTCStatsReportTest, BytesReceivedIs64Bit) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  auto webrtc_report =
      webrtc::RTCStatsReport::Create(webrtc::Timestamp::Micros(0));
  auto webrtc_stats = std::make_unique<webrtc::RTCInboundRtpStreamStats>(
      "inbound_rtp_id", webrtc::Timestamp::Micros(1234));
  const uint64_t kBytesReceived = UINT64_C(5000000000);
  webrtc_stats->bytes_received = kBytesReceived;
  webrtc_report->AddStats(std::move(webrtc_stats));

  auto report_platform = std::make_unique<RTCStatsReportPlatform>(
      base::WrapRefCounted(webrtc_report.get()));
  auto* report =
      MakeGarbageCollected<RTCStatsReport>(std::move(report_platform));

  ScriptObject out_obj;
  bool found =
      report->GetMapEntry(scope.GetScriptState(), "inbound_rtp_id", out_obj);
  EXPECT_TRUE(found);

  auto* v8_stats = RTCInboundRtpStreamStats::Create(
      scope.GetIsolate(), out_obj.V8Object(), scope.GetExceptionState());

  ASSERT_TRUE(v8_stats);
  EXPECT_TRUE(v8_stats->hasBytesReceived());
  EXPECT_EQ(kBytesReceived, v8_stats->bytesReceived());
}

}  // namespace blink
