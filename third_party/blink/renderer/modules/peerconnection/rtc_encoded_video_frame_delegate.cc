// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_video_frame_delegate.h"

#include <optional>
#include <utility>

#include "base/feature_list.h"
#include "base/time/time.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/modules/peerconnection/peer_connection_features.h"
#include "third_party/blink/renderer/modules/peerconnection/peer_connection_util.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/peerconnection/webrtc_util.h"
#include "third_party/webrtc/api/frame_transformer_factory.h"
#include "third_party/webrtc/api/frame_transformer_interface.h"

namespace blink {

static constexpr char kRTCEncodedVideoFrameDetachKey[] = "RTCEncodedVideoFrame";

const void* const RTCEncodedVideoFramesAttachment::kAttachmentKey = nullptr;

RTCEncodedVideoFrameDelegate::RTCEncodedVideoFrameDelegate(
    std::unique_ptr<webrtc::TransformableVideoFrameInterface> webrtc_frame)
    : webrtc_frame_(std::move(webrtc_frame)) {}

V8RTCEncodedVideoFrameType::Enum RTCEncodedVideoFrameDelegate::ComputeType()
    const {
  return webrtc_frame_->IsKeyFrame() ? V8RTCEncodedVideoFrameType::Enum::kKey
                                     : V8RTCEncodedVideoFrameType::Enum::kDelta;
}
V8RTCEncodedVideoFrameType::Enum RTCEncodedVideoFrameDelegate::Type() const {
  base::AutoLock lock(lock_);
  return webrtc_frame_ ? ComputeType() : post_neuter_metadata_.frame_type;
}

uint32_t RTCEncodedVideoFrameDelegate::RtpTimestamp() const {
  base::AutoLock lock(lock_);
  return webrtc_frame_ ? webrtc_frame_->GetTimestamp()
                       : post_neuter_metadata_.rtp_timestamp;
}

std::optional<webrtc::Timestamp>
RTCEncodedVideoFrameDelegate::PresentationTimestamp() const {
  base::AutoLock lock(lock_);
  return webrtc_frame_ ? webrtc_frame_->GetPresentationTimestamp()
                       : post_neuter_metadata_.presentation_timestamp;
}

DOMArrayBuffer* RTCEncodedVideoFrameDelegate::CreateDataBuffer(
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
                       V8AtomicString(isolate, kRTCEncodedVideoFrameDetachKey),
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

void RTCEncodedVideoFrameDelegate::SetData(const DOMArrayBuffer* data) {
  base::AutoLock lock(lock_);
  if (webrtc_frame_ && data) {
    webrtc_frame_->SetData(webrtc::ArrayView<const uint8_t>(
        static_cast<const uint8_t*>(data->Data()), data->ByteLength()));
  }
}

std::optional<uint8_t> RTCEncodedVideoFrameDelegate::PayloadType() const {
  base::AutoLock lock(lock_);
  return webrtc_frame_ ? std::make_optional(webrtc_frame_->GetPayloadType())
                       : post_neuter_metadata_.payload_type;
}

std::optional<std::string> RTCEncodedVideoFrameDelegate::MimeType() const {
  base::AutoLock lock(lock_);
  return webrtc_frame_ ? std::make_optional(webrtc_frame_->GetMimeType())
                       : post_neuter_metadata_.mime_type;
}

std::optional<webrtc::VideoFrameMetadata>
RTCEncodedVideoFrameDelegate::GetMetadata() const {
  base::AutoLock lock(lock_);
  return webrtc_frame_ ? std::optional<webrtc::VideoFrameMetadata>(
                             webrtc_frame_->Metadata())
                       : post_neuter_metadata_.video_frame_metadata;
}

std::optional<base::TimeTicks>
RTCEncodedVideoFrameDelegate::ComputeReceiveTime() const {
  return ConvertToOptionalTimeTicks(webrtc_frame_->ReceiveTime());
}

std::optional<base::TimeTicks> RTCEncodedVideoFrameDelegate::ReceiveTime()
    const {
  base::AutoLock lock(lock_);
  return webrtc_frame_ ? ComputeReceiveTime()
                       : post_neuter_metadata_.receive_time;
}

std::optional<CaptureTimeInfo>
RTCEncodedVideoFrameDelegate::ComputeCaptureTime() const {
  if (!webrtc_frame_->CaptureTime() ||
      webrtc_frame_->GetDirection() !=
          webrtc::TransformableFrameInterface::Direction::kReceiver) {
    return std::nullopt;
  }
  return CaptureTimeInfo(
      {.capture_time = base::Microseconds(webrtc_frame_->CaptureTime()->us()),
       .clock_type = CaptureTimeInfo::ClockType::kNtpRealClock});
}

std::optional<CaptureTimeInfo> RTCEncodedVideoFrameDelegate::CaptureTime()
    const {
  base::AutoLock lock(lock_);
  return webrtc_frame_ ? ComputeCaptureTime()
                       : post_neuter_metadata_.capture_time;
}

std::optional<base::TimeDelta>
RTCEncodedVideoFrameDelegate::ComputeSenderCaptureTimeOffset() const {
  return ConvertToOptionalTimeDelta(webrtc_frame_->SenderCaptureTimeOffset());
}

std::optional<base::TimeDelta>
RTCEncodedVideoFrameDelegate::SenderCaptureTimeOffset() const {
  base::AutoLock lock(lock_);
  return webrtc_frame_ ? ComputeSenderCaptureTimeOffset()
                       : post_neuter_metadata_.sender_capture_time_offset;
}

base::expected<void, String> RTCEncodedVideoFrameDelegate::SetMetadata(
    const webrtc::VideoFrameMetadata& metadata,
    uint32_t rtpTimestamp) {
  base::AutoLock lock(lock_);
  if (!webrtc_frame_) {
    return base::unexpected("underlying webrtc frame is empty.");
  }
  webrtc_frame_->SetMetadata(metadata);
  webrtc_frame_->SetRTPTimestamp(rtpTimestamp);
  return base::ok();
}

std::unique_ptr<webrtc::TransformableVideoFrameInterface>
RTCEncodedVideoFrameDelegate::PassWebRtcFrame() {
  base::AutoLock lock(lock_);
  if (webrtc_frame_) {
    if (base::FeatureList::IsEnabled(
            kWebRtcEncodedTransformRememberVideoFrameType)) {
      post_neuter_metadata_.frame_type = ComputeType();
    }
    if (base::FeatureList::IsEnabled(kWebRtcEncodedTransformRememberMetadata)) {
      post_neuter_metadata_.payload_type = webrtc_frame_->GetPayloadType();
      post_neuter_metadata_.mime_type = webrtc_frame_->GetMimeType();
      post_neuter_metadata_.video_frame_metadata = webrtc_frame_->Metadata();
      post_neuter_metadata_.receive_time = ComputeReceiveTime();
      post_neuter_metadata_.capture_time = ComputeCaptureTime();
      post_neuter_metadata_.sender_capture_time_offset =
          ComputeSenderCaptureTimeOffset();
      post_neuter_metadata_.rtp_timestamp = webrtc_frame_->GetTimestamp();
      post_neuter_metadata_.presentation_timestamp =
          webrtc_frame_->GetPresentationTimestamp();
    }
  }
  return std::move(webrtc_frame_);
}

std::unique_ptr<webrtc::TransformableVideoFrameInterface>
RTCEncodedVideoFrameDelegate::CloneWebRtcFrame() {
  base::AutoLock lock(lock_);
  if (!webrtc_frame_) {
    return nullptr;
  }
  return webrtc::CloneVideoFrame(webrtc_frame_.get());
}

}  // namespace blink
