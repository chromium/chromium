// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_track_event.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_track_event_init.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_track.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_receiver.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_transceiver.h"

namespace blink {

RTCTrackEvent* RTCTrackEvent::Create(const AtomicString& type,
                                     const RTCTrackEventInit* eventInitDict) {
  return MakeGarbageCollected<RTCTrackEvent>(type, eventInitDict);
}

RTCTrackEvent::RTCTrackEvent(const AtomicString& type,
                             const RTCTrackEventInit* eventInitDict)
    : Event(type, eventInitDict),
      receiver_(eventInitDict->receiver()),
      track_(eventInitDict->track()),
      streams_(eventInitDict->streams()),
      transceiver_(eventInitDict->transceiver()) {
  DCHECK(receiver_);
  DCHECK(track_);
}

RTCTrackEvent::RTCTrackEvent(RTCRtpReceiver* receiver,
                             MediaStreamTrack* track,
                             const HeapVector<Member<MediaStream>>& streams,
                             RTCRtpTransceiver* transceiver)
    : Event(event_type_names::kTrack, Bubbles::kNo, Cancelable::kNo),
      receiver_(receiver),
      track_(track),
      streams_(streams),
      transceiver_(transceiver) {
  DCHECK(receiver_);
  DCHECK(track_);
}

RTCRtpReceiver* RTCTrackEvent::receiver() const {
  return receiver_.Get();
}

MediaStreamTrack* RTCTrackEvent::track() const {
  return track_.Get();
}

const HeapVector<Member<MediaStream>>& RTCTrackEvent::streams() const {
  return streams_;
}

RTCRtpTransceiver* RTCTrackEvent::transceiver() const {
  return transceiver_.Get();
}

void RTCTrackEvent::Trace(Visitor* visitor) const {
  visitor->Trace(receiver_);
  visitor->Trace(track_);
  visitor->Trace(streams_);
  visitor->Trace(transceiver_);
  Event::Trace(visitor);
}

}  // namespace blink
