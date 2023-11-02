// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_peer_connection_controller.h"

#include "services/metrics/public/cpp/ukm_builders.h"

namespace blink {

// static
const char RTCPeerConnectionController::kSupplementName[] =
    "RTCPeerConnectionController";

// static
RTCPeerConnectionController& RTCPeerConnectionController::From(
    Document& document) {
  RTCPeerConnectionController* supplement =
      Supplement<Document>::From<RTCPeerConnectionController>(document);
  if (!supplement) {
    supplement = MakeGarbageCollected<RTCPeerConnectionController>(document);
    Supplement<Document>::ProvideTo(document, supplement);
  }
  return *supplement;
}

RTCPeerConnectionController::RTCPeerConnectionController(Document& document)
    : Supplement<Document>(document) {}

void RTCPeerConnectionController::MaybeReportComplexSdp(
    ComplexSdpCategory complex_sdp_category) {
  if (has_reported_ukm_)
    return;

  // Report only the first observation for the document and ignore all others.
  // This provides a good balance between privacy and meaningful metrics.
  has_reported_ukm_ = true;
  ukm::SourceId source_id = GetSupplementable()->UkmSourceID();
  ukm::builders::WebRTC_ComplexSdp(source_id)
      .SetCategory(static_cast<int64_t>(complex_sdp_category))
      .Record(GetSupplementable()->UkmRecorder());
}

void RTCPeerConnectionController::Trace(Visitor* visitor) const {
  Supplement<Document>::Trace(visitor);
}

}  // namespace blink
