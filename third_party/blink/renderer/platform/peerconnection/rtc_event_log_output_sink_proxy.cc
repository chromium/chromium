// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/peerconnection/rtc_event_log_output_sink_proxy.h"

#include "base/logging.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_event_log_output_sink.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

std::unique_ptr<webrtc::RtcEventLogOutput> CreateRtcEventLogOutputSinkProxy(
    RtcEventLogOutputSink* sink) {
  return std::make_unique<RtcEventLogOutputSinkProxy>(sink);
}

RtcEventLogOutputSinkProxy::RtcEventLogOutputSinkProxy(
    RtcEventLogOutputSink* sink)
    : sink_(sink) {
  CHECK(sink_);
}

RtcEventLogOutputSinkProxy::~RtcEventLogOutputSinkProxy() = default;

bool RtcEventLogOutputSinkProxy::IsActive() const {
  return true;  // Active until the proxy is destroyed.
}

bool RtcEventLogOutputSinkProxy::Write(const std::string& output) {
  sink_->OnWebRtcEventLogWrite(String::FromUTF8(output.c_str()));
  return true;
}

}  // namespace blink
