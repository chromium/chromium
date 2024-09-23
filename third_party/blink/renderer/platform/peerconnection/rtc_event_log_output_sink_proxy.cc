// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/peerconnection/rtc_event_log_output_sink_proxy.h"

#include <string_view>

#include "base/check.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_event_log_output_sink.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

RtcEventLogOutputSinkProxy::RtcEventLogOutputSinkProxy(
    RtcEventLogOutputSink* sink)
    : sink_(sink) {
  CHECK(sink_);
}

RtcEventLogOutputSinkProxy::~RtcEventLogOutputSinkProxy() = default;

bool RtcEventLogOutputSinkProxy::IsActive() const {
  return true;  // Active until the proxy is destroyed.
}

bool RtcEventLogOutputSinkProxy::Write(std::string_view output) {
  WTF::Vector<uint8_t> converted_output;
  converted_output.AppendRange(output.begin(), output.end());

  sink_.Lock()->OnWebRtcEventLogWrite(converted_output);
  return true;
}

}  // namespace blink
