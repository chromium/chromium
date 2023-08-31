// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_audio_frame_delegate.h"

#include <utility>

#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/webrtc/api/frame_transformer_factory.h"

namespace blink {

const void* RTCEncodedAudioFramesAttachment::kAttachmentKey;

RTCEncodedAudioFrameDelegate::RTCEncodedAudioFrameDelegate(
    std::unique_ptr<webrtc::TransformableAudioFrameInterface> webrtc_frame,
    rtc::ArrayView<const unsigned int> contributing_sources,
    absl::optional<uint16_t> sequence_number)
    : webrtc_frame_(std::move(webrtc_frame)),
      sequence_number_(sequence_number) {
  contributing_sources_.assign(contributing_sources);
}

uint32_t RTCEncodedAudioFrameDelegate::RtpTimestamp() const {
  base::AutoLock lock(lock_);
  return webrtc_frame_ ? webrtc_frame_->GetTimestamp() : 0;
}

DOMArrayBuffer* RTCEncodedAudioFrameDelegate::CreateDataBuffer() const {
  ArrayBufferContents contents;
  {
    base::AutoLock lock(lock_);
    if (!webrtc_frame_) {
      return nullptr;
    }

    auto data = webrtc_frame_->GetData();
    contents =
        ArrayBufferContents(data.size(), 1, ArrayBufferContents::kNotShared,
                            ArrayBufferContents::kDontInitialize);
    if (UNLIKELY(!contents.Data())) {
      OOM_CRASH(data.size());
    }
    memcpy(contents.Data(), data.data(), data.size());
  }
  return DOMArrayBuffer::Create(std::move(contents));
}

void RTCEncodedAudioFrameDelegate::SetData(const DOMArrayBuffer* data) {
  base::AutoLock lock(lock_);
  if (webrtc_frame_ && data) {
    webrtc_frame_->SetData(rtc::ArrayView<const uint8_t>(
        static_cast<const uint8_t*>(data->Data()), data->ByteLength()));
  }
}

void RTCEncodedAudioFrameDelegate::SetRtpTimestamp(
    uint32_t timestamp,
    ExceptionState& exception_state) {
  base::AutoLock lock(lock_);
  if (webrtc_frame_) {
    webrtc_frame_->SetRTPTimestamp(timestamp);
  } else {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Underlying webrtc frame doesn't exist.");
  }
}

absl::optional<uint32_t> RTCEncodedAudioFrameDelegate::Ssrc() const {
  base::AutoLock lock(lock_);
  return webrtc_frame_ ? absl::make_optional(webrtc_frame_->GetSsrc())
                       : absl::nullopt;
}

absl::optional<uint8_t> RTCEncodedAudioFrameDelegate::PayloadType() const {
  base::AutoLock lock(lock_);
  return webrtc_frame_ ? absl::make_optional(webrtc_frame_->GetPayloadType())
                       : absl::nullopt;
}

absl::optional<uint16_t> RTCEncodedAudioFrameDelegate::SequenceNumber() const {
  base::AutoLock lock(lock_);
  return sequence_number_;
}

Vector<uint32_t> RTCEncodedAudioFrameDelegate::ContributingSources() const {
  base::AutoLock lock(lock_);
  return contributing_sources_;
}

absl::optional<uint64_t> RTCEncodedAudioFrameDelegate::AbsCaptureTime() const {
  base::AutoLock lock(lock_);
  if (webrtc_frame_ &&
      webrtc_frame_->GetDirection() ==
          webrtc::TransformableFrameInterface::Direction::kReceiver) {
    webrtc::TransformableAudioFrameInterface* incoming_audio_frame =
        static_cast<webrtc::TransformableAudioFrameInterface*>(
            webrtc_frame_.get());

    return incoming_audio_frame->AbsoluteCaptureTimestamp();
  }

  return absl::nullopt;
}

std::unique_ptr<webrtc::TransformableAudioFrameInterface>
RTCEncodedAudioFrameDelegate::PassWebRtcFrame() {
  base::AutoLock lock(lock_);
  return std::move(webrtc_frame_);
}

std::unique_ptr<webrtc::TransformableAudioFrameInterface>
RTCEncodedAudioFrameDelegate::CloneWebRtcFrame() {
  base::AutoLock lock(lock_);
  return webrtc::CloneAudioFrame(webrtc_frame_.get());
}

}  // namespace blink
