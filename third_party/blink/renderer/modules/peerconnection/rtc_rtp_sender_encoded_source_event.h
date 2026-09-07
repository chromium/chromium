#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_RTP_SENDER_ENCODED_SOURCE_EVENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_RTP_SENDER_ENCODED_SOURCE_EVENT_H_

#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/modules/event_modules.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_sender_encoded_source.h"

namespace blink {

class RTCRtpSenderEncodedSourceEvent final : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit RTCRtpSenderEncodedSourceEvent(
      RTCRtpSenderEncodedSource* encoded_source)
      : Event(event_type_names::kRtcsenderencodedsource,
              Bubbles::kNo,
              Cancelable::kNo),
        encoded_source_(encoded_source) {}

  RTCRtpSenderEncodedSource* encodedSource() const {
    return encoded_source_.Get();
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(encoded_source_);
    Event::Trace(visitor);
  }

 private:
  Member<RTCRtpSenderEncodedSource> encoded_source_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_RTP_SENDER_ENCODED_SOURCE_EVENT_H_
