// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_audio_frame.h"

#include <utility>

#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_encoded_audio_frame_metadata.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_audio_frame_delegate.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/webrtc/api/frame_transformer_interface.h"

namespace blink {

RTCEncodedAudioFrame::RTCEncodedAudioFrame(
    std::unique_ptr<webrtc::TransformableFrameInterface> webrtc_frame)
    : delegate_(base::MakeRefCounted<RTCEncodedAudioFrameDelegate>(
          std::move(webrtc_frame),
          Vector<uint32_t>(),
          absl::nullopt)) {}

RTCEncodedAudioFrame::RTCEncodedAudioFrame(
    std::unique_ptr<webrtc::TransformableAudioFrameInterface>
        webrtc_audio_frame) {
  Vector<uint32_t> contributing_sources;
  absl::optional<uint16_t> sequence_number;
  if (webrtc_audio_frame) {
    contributing_sources.assign(webrtc_audio_frame->GetContributingSources());
    if (webrtc_audio_frame->GetDirection() ==
        webrtc::TransformableFrameInterface::Direction::kReceiver) {
      sequence_number = webrtc_audio_frame->GetHeader().sequenceNumber;
    }
  }
  delegate_ = base::MakeRefCounted<RTCEncodedAudioFrameDelegate>(
      std::move(webrtc_audio_frame), std::move(contributing_sources),
      sequence_number);
}

RTCEncodedAudioFrame::RTCEncodedAudioFrame(
    scoped_refptr<RTCEncodedAudioFrameDelegate> delegate)
    : delegate_(std::move(delegate)) {}

uint32_t RTCEncodedAudioFrame::timestamp() const {
  return delegate_->Timestamp();
}

DOMArrayBuffer* RTCEncodedAudioFrame::data() const {
  if (!frame_data_) {
    frame_data_ = delegate_->CreateDataBuffer();
  }
  return frame_data_;
}

RTCEncodedAudioFrameMetadata* RTCEncodedAudioFrame::getMetadata() const {
  RTCEncodedAudioFrameMetadata* metadata =
      RTCEncodedAudioFrameMetadata::Create();
  if (delegate_->Ssrc()) {
    metadata->setSynchronizationSource(*delegate_->Ssrc());
  }
  metadata->setContributingSources(delegate_->ContributingSources());
  if (delegate_->PayloadType()) {
    metadata->setPayloadType(*delegate_->PayloadType());
  }
  if (delegate_->SequenceNumber()) {
    metadata->setSequenceNumber(*delegate_->SequenceNumber());
  }
  return metadata;
}

void RTCEncodedAudioFrame::setData(DOMArrayBuffer* data) {
  frame_data_ = data;
}

String RTCEncodedAudioFrame::toString() const {
  StringBuilder sb;
  sb.Append("RTCEncodedAudioFrame{rtpTimestamp: ");
  sb.AppendNumber(delegate_->Timestamp());
  sb.Append(", size: ");
  sb.AppendNumber(data() ? data()->ByteLength() : 0);
  sb.Append("}");
  return sb.ToString();
}

RTCEncodedAudioFrame* RTCEncodedAudioFrame::clone(
    ExceptionState& exception_state) const {
  String exception_message;
  std::unique_ptr<webrtc::TransformableFrameInterface> new_webrtc_frame =
      delegate_->CloneWebRtcFrame(exception_message);
  if (!new_webrtc_frame) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataCloneError,
                                      exception_message);
    return nullptr;
  }
  return MakeGarbageCollected<RTCEncodedAudioFrame>(
      std::move(new_webrtc_frame));
}

void RTCEncodedAudioFrame::SyncDelegate() const {
  delegate_->SetData(frame_data_);
}

scoped_refptr<RTCEncodedAudioFrameDelegate> RTCEncodedAudioFrame::Delegate()
    const {
  SyncDelegate();
  return delegate_;
}

std::unique_ptr<webrtc::TransformableFrameInterface>
RTCEncodedAudioFrame::PassWebRtcFrame() {
  SyncDelegate();
  return delegate_->PassWebRtcFrame();
}

void RTCEncodedAudioFrame::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  visitor->Trace(frame_data_);
}

}  // namespace blink
