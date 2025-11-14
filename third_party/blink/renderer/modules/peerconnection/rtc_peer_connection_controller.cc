// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_peer_connection_controller.h"

#include "services/metrics/public/cpp/ukm_builders.h"

namespace blink {

// static
RTCPeerConnectionController& RTCPeerConnectionController::From(
    Document& document) {
  RTCPeerConnectionController* supplement =
      document.GetRTCPeerConnectionController();
  if (!supplement) {
    supplement = MakeGarbageCollected<RTCPeerConnectionController>(document);
    document.SetRTCPeerConnectionController(supplement);
  }
  return *supplement;
}

RTCPeerConnectionController::RTCPeerConnectionController(Document& document)
    : document_(&document) {}

void RTCPeerConnectionController::MaybeReportComplexSdp(
    ComplexSdpCategory complex_sdp_category) {
  if (has_reported_ukm_)
    return;

  // Report only the first observation for the document and ignore all others.
  // This provides a good balance between privacy and meaningful metrics.
  has_reported_ukm_ = true;
  ukm::SourceId source_id = document_->UkmSourceID();
  ukm::builders::WebRTC_ComplexSdp(source_id)
      .SetCategory(static_cast<int64_t>(complex_sdp_category))
      .Record(document_->UkmRecorder());
}

void RTCPeerConnectionController::Trace(Visitor* visitor) const {
  visitor->Trace(document_);
}

}  // namespace blink
