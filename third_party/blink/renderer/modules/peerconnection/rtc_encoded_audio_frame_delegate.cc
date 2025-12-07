// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_audio_frame_delegate.h"

#include <optional>
#include <utility>

#include "base/feature_list.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/modules/peerconnection/peer_connection_features.h"
#include "third_party/blink/renderer/modules/peerconnection/peer_connection_util.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/peerconnection/webrtc_util.h"
#include "third_party/webrtc/api/frame_transformer_factory.h"
#include "third_party/webrtc/api/frame_transformer_interface.h"
#include "third_party/webrtc/api/units/time_delta.h"
#include "third_party/webrtc/api/units/timestamp.h"

namespace blink {

static constexpr char kRTCEncodedAudioFrameDetachKey[] = "RTCEncodedAudioFrame";
static constexpr int kAcceptableCaptureTimeDeltaMs = 1;

const void* RTCEncodedAudioFramesAttachment::kAttachmentKey;

RTCEncodedAudioFrameDelegate::RTCEncodedAudioFrameDelegate(
    std::unique_ptr<webrtc::TransformableAudioFrameInterface> webrtc_frame,
    webrtc::ArrayView<const unsigned int> contributing_sources,
    std::optional<uint16_t> sequence_number)
    : webrtc_frame_(std::move(webrtc_frame)),
      contributing_sources_(contributing_sources),
      sequence_number_(sequence_number) {}

uint32_t RTCEncodedAudioFrameDelegate::RtpTimestamp() const {
  base::AutoLock lock(lock_);
  return webrtc_frame_ ? webrtc_frame_->GetTimestamp()
                       : post_neuter_metadata_.rtp_timestamp;
}

DOMArrayBuffer* RTCEncodedAudioFrameDelegate::CreateDataBuffer(
    v8::Isolate* isolate) const {
  ArrayBufferContents contents;
  {
    base::AutoLock lock(lock_);
    if (!webrtc_frame_) {
      // WebRTC frame already passed, return a detached ArrayBuffer.
      DOMArrayBuffer* buffer = DOMArrayBuffer::Create(
          /*num_elements=*/static_cast<size_t>(0), /*element_byte_size=*/1);
      ArrayBufferContents contents_to_drop;
      NonThrowableExceptionState exception_state;
      buffer->Transfer(isolate,
                       V8AtomicString(isolate, kRTCEncodedAudioFrameDetachKey),
                       contents_to_drop, exception_state);
      return buffer;
    }

    auto data = webrtc_frame_->GetData();
    contents = ArrayBufferContents(
        data.size(), 1, ArrayBufferContents::kNotShared,
        ArrayBufferContents::kDontInitialize,
        ArrayBufferContents::AllocationFailureBehavior::kCrash);
    CHECK(contents.IsValid());
    contents.ByteSpan().copy_from(data);
  }
  return DOMArrayBuffer::Create(std::move(contents));
}

void RTCEncodedAudioFrameDelegate::SetData(const DOMArrayBuffer* data) {
  base::AutoLock lock(lock_);
  if (webrtc_frame_ && data) {
    webrtc_frame_->SetData(webrtc::ArrayView<const uint8_t>(
        static_cast<const uint8_t*>(data->Data()), data->ByteLength()));
  }
}

base::expected<void, String>
RTCEncodedAudioFrameDelegate::SetWebRtcFrameMetadata(
    ExecutionContext* context,
    const RTCEncodedAudioFrameMetadata* metadata) {
  base::AutoLock lock(lock_);
  if (!webrtc_frame_) {
    return base::unexpected("Underlying webrtc frame doesn't exist.");
  }

  // Payload type always has a current value. The new metadata must match it if
  // SetPayloadType is not supported.
  if (metadata->hasPayloadType()) {
    if (metadata->payloadType() != webrtc_frame_->GetPayloadType() &&
        !webrtc_frame_->CanSetPayloadType()) {
      return base::unexpected("payloadType cannot be modified");
    }
    // Payload types must be in the [0,127] range, but values in the [64,95]
    // range are reserved for RCTP. For additional details, see
    // https://tools.ietf.org/html/rfc5761#section-4
    if ((metadata->payloadType() >= 64u && metadata->payloadType() <= 95u) ||
        metadata->payloadType() > 127u) {
      return base::unexpected("invalid payloadType value");
    }
  }

  std::optional<uint8_t> audio_level_dbov;
  if (metadata->hasAudioLevel()) {
    audio_level_dbov = FromLinearAudioLevel(metadata->audioLevel());
  }
  if (audio_level_dbov != webrtc_frame_->AudioLevel() &&
      !webrtc_frame_->CanSetAudioLevel()) {
    return base::unexpected("audioLevel cannot be modified");
  }

  std::optional<webrtc::Timestamp> capture_time;
  if (metadata->hasCaptureTime()) {
    CaptureTimeInfo::ClockType clock_type;
    switch (webrtc_frame_->GetDirection()) {
      case webrtc::TransformableFrameInterface::Direction::kReceiver:
        clock_type = CaptureTimeInfo::ClockType::kNtpRealClock;
        break;
      case webrtc::TransformableFrameInterface::Direction::kSender:
        clock_type = CaptureTimeInfo::ClockType::kTimeTicks;
        break;
      case webrtc::TransformableFrameInterface::Direction::kUnknown:
        return base::unexpected("captureTime not supported for this frame");
    }
    base::TimeDelta capture_time_delta = RTCEncodedFrameTimestampToCaptureTime(
        context, metadata->captureTime(), clock_type);
    capture_time =
        webrtc::Timestamp::Micros(capture_time_delta.InMicroseconds());
  }

  bool capture_time_is_different = false;
  if (!webrtc_frame_->CanSetCaptureTime() && capture_time.has_value()) {
    if (!webrtc_frame_->CaptureTime().has_value()) {
      capture_time_is_different = true;
    } else {
      // Ignore small differences in capture time.
      webrtc::TimeDelta delta = *capture_time - *webrtc_frame_->CaptureTime();
      if (delta.Abs() >
          webrtc::TimeDelta::Millis(kAcceptableCaptureTimeDeltaMs)) {
        capture_time_is_different = true;
      }
    }
  }

  if (capture_time_is_different && !webrtc_frame_->CanSetCaptureTime()) {
    return base::unexpected("captureTime cannot be modified");
  }

  webrtc_frame_->SetRTPTimestamp(metadata->rtpTimestamp());
  if (metadata->hasPayloadType() && webrtc_frame_->CanSetPayloadType()) {
    webrtc_frame_->SetPayloadType(metadata->payloadType());
  }
  if (webrtc_frame_->CanSetCaptureTime()) {
    webrtc_frame_->SetCaptureTime(capture_time);
  }
  if (webrtc_frame_->CanSetAudioLevel()) {
    webrtc_frame_->SetAudioLevel(audio_level_dbov);
  }

  return base::ok();
}

std::optional<uint32_t> RTCEncodedAudioFrameDelegate::Ssrc() const {
  base::AutoLock lock(lock_);
  return webrtc_frame_ ? std::make_optional(webrtc_frame_->GetSsrc())
                       : post_neuter_metadata_.ssrc;
}

std::optional<uint8_t> RTCEncodedAudioFrameDelegate::PayloadType() const {
  base::AutoLock lock(lock_);
  return webrtc_frame_ ? std::make_optional(webrtc_frame_->GetPayloadType())
                       : post_neuter_metadata_.payload_type;
}

std::optional<std::string> RTCEncodedAudioFrameDelegate::MimeType() const {
  base::AutoLock lock(lock_);
  return webrtc_frame_ ? std::make_optional(webrtc_frame_->GetMimeType())
                       : post_neuter_metadata_.mime_type;
}

std::optional<uint16_t> RTCEncodedAudioFrameDelegate::SequenceNumber() const {
  return sequence_number_;
}

Vector<uint32_t> RTCEncodedAudioFrameDelegate::ContributingSources() const {
  return contributing_sources_;
}

std::optional<base::TimeTicks>
RTCEncodedAudioFrameDelegate::ComputeReceiveTime() const {
  return ConvertToOptionalTimeTicks(webrtc_frame_->ReceiveTime());
}

std::optional<base::TimeTicks> RTCEncodedAudioFrameDelegate::ReceiveTime()
    const {
  base::AutoLock lock(lock_);
  return webrtc_frame_ ? ComputeReceiveTime()
                       : post_neuter_metadata_.receive_time;
}

std::optional<CaptureTimeInfo>
RTCEncodedAudioFrameDelegate::ComputeCaptureTime() const {
  if (!webrtc_frame_->CaptureTime()) {
    return std::nullopt;
  }
  CaptureTimeInfo::ClockType clock_type;
  switch (webrtc_frame_->GetDirection()) {
    case webrtc::TransformableFrameInterface::Direction::kReceiver:
      clock_type = CaptureTimeInfo::ClockType::kNtpRealClock;
      break;
    case webrtc::TransformableFrameInterface::Direction::kSender:
      clock_type = CaptureTimeInfo::ClockType::kTimeTicks;
      break;
    case webrtc::TransformableFrameInterface::Direction::kUnknown:
      return std::nullopt;
  }
  return CaptureTimeInfo(
      {.capture_time = base::Microseconds(webrtc_frame_->CaptureTime()->us()),
       .clock_type = clock_type});
}

std::optional<CaptureTimeInfo> RTCEncodedAudioFrameDelegate::CaptureTime()
    const {
  base::AutoLock lock(lock_);
  return webrtc_frame_ ? ComputeCaptureTime()
                       : post_neuter_metadata_.capture_time_info;
}

std::optional<base::TimeDelta>
RTCEncodedAudioFrameDelegate::ComputeSenderCaptureTimeOffset() const {
  return ConvertToOptionalTimeDelta(webrtc_frame_->SenderCaptureTimeOffset());
}

std::optional<base::TimeDelta>
RTCEncodedAudioFrameDelegate::SenderCaptureTimeOffset() const {
  base::AutoLock lock(lock_);
  return webrtc_frame_ ? ComputeSenderCaptureTimeOffset()
                       : post_neuter_metadata_.sender_capture_time_offset;
}

std::optional<double> RTCEncodedAudioFrameDelegate::ComputeAudioLevel() const {
  return webrtc_frame_->AudioLevel() ? std::make_optional(ToLinearAudioLevel(
                                           *webrtc_frame_->AudioLevel()))
                                     : std::nullopt;
}

std::optional<double> RTCEncodedAudioFrameDelegate::AudioLevel() const {
  base::AutoLock lock(lock_);
  return webrtc_frame_ ? ComputeAudioLevel()
                       : post_neuter_metadata_.audio_level;
}

std::unique_ptr<webrtc::TransformableAudioFrameInterface>
RTCEncodedAudioFrameDelegate::PassWebRtcFrame() {
  base::AutoLock lock(lock_);
  if (base::FeatureList::IsEnabled(kWebRtcEncodedTransformRememberMetadata) &&
      webrtc_frame_) {
    post_neuter_metadata_.ssrc = webrtc_frame_->GetSsrc();
    post_neuter_metadata_.payload_type = webrtc_frame_->GetPayloadType();
    post_neuter_metadata_.mime_type = webrtc_frame_->GetMimeType();
    post_neuter_metadata_.receive_time = ComputeReceiveTime();
    post_neuter_metadata_.capture_time_info = ComputeCaptureTime();
    post_neuter_metadata_.sender_capture_time_offset =
        ComputeSenderCaptureTimeOffset();
    post_neuter_metadata_.audio_level = ComputeAudioLevel();
    post_neuter_metadata_.rtp_timestamp = webrtc_frame_->GetTimestamp();
  }
  return std::move(webrtc_frame_);
}

std::unique_ptr<webrtc::TransformableAudioFrameInterface>
RTCEncodedAudioFrameDelegate::CloneWebRtcFrame() {
  base::AutoLock lock(lock_);
  if (!webrtc_frame_) {
    return nullptr;
  }
  return webrtc::CloneAudioFrame(webrtc_frame_.get());
}

}  // namespace blink
