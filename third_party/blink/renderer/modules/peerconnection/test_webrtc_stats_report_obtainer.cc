// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/test_webrtc_stats_report_obtainer.h"

#include "base/bind.h"
#include "base/callback.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_stats.h"

namespace blink {

TestWebRTCStatsReportObtainer::TestWebRTCStatsReportObtainer() {}

TestWebRTCStatsReportObtainer::~TestWebRTCStatsReportObtainer() {}

blink::WebRTCStatsReportCallback
TestWebRTCStatsReportObtainer::GetStatsCallbackWrapper() {
  return base::BindOnce(&TestWebRTCStatsReportObtainer::OnStatsDelivered, this);
}

RTCStatsReportPlatform* TestWebRTCStatsReportObtainer::report() const {
  return report_.get();
}

RTCStatsReportPlatform* TestWebRTCStatsReportObtainer::WaitForReport() {
  run_loop_.Run();
  return report_.get();
}

void TestWebRTCStatsReportObtainer::OnStatsDelivered(
    std::unique_ptr<RTCStatsReportPlatform> report) {
  report_ = std::move(report);
  run_loop_.Quit();
}

}  // namespace blink
