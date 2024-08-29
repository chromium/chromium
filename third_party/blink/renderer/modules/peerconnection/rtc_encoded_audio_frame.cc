// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_audio_frame.h"

#include <utility>

#include "base/unguessable_token.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_encoded_audio_frame_metadata.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_encoded_audio_frame_options.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_audio_frame_delegate.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/webrtc/api/frame_transformer_interface.h"

namespace blink {
namespace {

struct SetMetadataValidationOutcome {
  bool allowed;
  String error_msg;
};

SetMetadataValidationOutcome IsAllowedSetMetadataChange(
    const RTCEncodedAudioFrameMetadata* current_metadata,
    const RTCEncodedAudioFrameMetadata* new_metadata) {
  // Only changing the RTP Timestamp is supported.

  if (new_metadata->hasSynchronizationSource() !=
          current_metadata->hasSynchronizationSource() ||
      (new_metadata->hasSynchronizationSource() &&
       current_metadata->synchronizationSource() !=
           new_metadata->synchronizationSource())) {
    return SetMetadataValidationOutcome{false, "Bad synchronizationSource"};
  }
  if (new_metadata->hasContributingSources() !=
          current_metadata->hasContributingSources() ||
      (new_metadata->hasContributingSources() &&
       current_metadata->contributingSources() !=
           new_metadata->contributingSources())) {
    return SetMetadataValidationOutcome{false, "Bad contributingSources"};
  }
  if (new_metadata->hasPayloadType() != current_metadata->hasPayloadType() ||
      (new_metadata->hasPayloadType() &&
       current_metadata->payloadType() != new_metadata->payloadType())) {
    return SetMetadataValidationOutcome{false, "Bad payloadType"};
  }
  if (new_metadata->hasSequenceNumber() !=
          current_metadata->hasSequenceNumber() ||
      (new_metadata->hasSequenceNumber() &&
       current_metadata->sequenceNumber() != new_metadata->sequenceNumber())) {
    return SetMetadataValidationOutcome{false, "Bad sequenceNumber"};
  }
  if (new_metadata->hasAbsCaptureTime() !=
          current_metadata->hasAbsCaptureTime() ||
      (new_metadata->hasAbsCaptureTime() &&
       current_metadata->absCaptureTime() != new_metadata->absCaptureTime())) {
    return SetMetadataValidationOutcome{false, "Bad absoluteCaptureTime"};
  }
  if (!new_metadata->hasRtpTimestamp()) {
    return SetMetadataValidationOutcome{false, "Bad rtpTimestamp"};
  }
  return SetMetadataValidationOutcome{true, String()};
}

}  // namespace

RTCEncodedAudioFrame* RTCEncodedAudioFrame::Create(
    RTCEncodedAudioFrame* original_frame,
    ExceptionState& exception_state) {
  return RTCEncodedAudioFrame::Create(original_frame, nullptr, exception_state);
}

RTCEncodedAudioFrame* RTCEncodedAudioFrame::Create(
    RTCEncodedAudioFrame* original_frame,
    const RTCEncodedAudioFrameOptions* options_dict,
    ExceptionState& exception_state) {
  RTCEncodedAudioFrame* new_frame;
  if (original_frame) {
    new_frame = MakeGarbageCollected<RTCEncodedAudioFrame>(
        original_frame->Delegate()->CloneWebRtcFrame());
  } else {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        "Cannot create a new AudioFrame: input Audioframe is empty.");
    return nullptr;
  }
  if (options_dict && options_dict->hasMetadata()) {
    base::expected<void, String> set_metadata =
        new_frame->SetMetadata(options_dict->metadata());
    if (!set_metadata.has_value()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kInvalidModificationError,
          "Cannot create a new AudioFrame: " + set_metadata.error());
      return nullptr;
    }
  }
  return new_frame;
}

RTCEncodedAudioFrame::RTCEncodedAudioFrame(
    std::unique_ptr<webrtc::TransformableAudioFrameInterface>
        webrtc_audio_frame)
    : RTCEncodedAudioFrame(std::move(webrtc_audio_frame),
                           base::UnguessableToken::Null(),
                           0) {}

RTCEncodedAudioFrame::RTCEncodedAudioFrame(
    std::unique_ptr<webrtc::TransformableAudioFrameInterface>
        webrtc_audio_frame,
    base::UnguessableToken owner_id,
    int64_t counter)
    : delegate_(base::MakeRefCounted<RTCEncodedAudioFrameDelegate>(
          std::move(webrtc_audio_frame),
          webrtc_audio_frame ? webrtc_audio_frame->GetContributingSources()
                             : Vector<uint32_t>(),
          webrtc_audio_frame ? webrtc_audio_frame->SequenceNumber()
                             : std::nullopt)),
      owner_id_(owner_id),
      counter_(counter) {}

RTCEncodedAudioFrame::RTCEncodedAudioFrame(
    scoped_refptr<RTCEncodedAudioFrameDelegate> delegate)
    : RTCEncodedAudioFrame(delegate->CloneWebRtcFrame()) {}

uint32_t RTCEncodedAudioFrame::timestamp() const {
  return delegate_->RtpTimestamp();
}

DOMArrayBuffer* RTCEncodedAudioFrame::data(ExecutionContext* context) const {
  if (!frame_data_) {
    frame_data_ = delegate_->CreateDataBuffer(context->GetIsolate());
  }
  return frame_data_.Get();
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
  if (delegate_->AbsCaptureTime()) {
    metadata->setAbsCaptureTime(*delegate_->AbsCaptureTime());
  }
  metadata->setRtpTimestamp(delegate_->RtpTimestamp());
  if (delegate_->MimeType()) {
    metadata->setMimeType(WTF::String::FromUTF8(*delegate_->MimeType()));
  }
  return metadata;
}

base::expected<void, String> RTCEncodedAudioFrame::SetMetadata(
    const RTCEncodedAudioFrameMetadata* metadata) {
  SetMetadataValidationOutcome validation =
      IsAllowedSetMetadataChange(getMetadata(), metadata);
  if (!validation.allowed) {
    return base::unexpected(
        "Invalid modification of RTCEncodedAudioFrameMetadata. " +
        validation.error_msg);
  }

  return delegate_->SetRtpTimestamp(metadata->rtpTimestamp());
}

void RTCEncodedAudioFrame::setMetadata(RTCEncodedAudioFrameMetadata* metadata,
                                       ExceptionState& exception_state) {
  base::expected<void, String> set_metadata = SetMetadata(metadata);
  if (!set_metadata.has_value()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidModificationError,
        "Cannot setMetadata: " + set_metadata.error());
  }
}

void RTCEncodedAudioFrame::setData(ExecutionContext*, DOMArrayBuffer* data) {
  frame_data_ = data;
}

String RTCEncodedAudioFrame::toString(ExecutionContext* context) const {
  StringBuilder sb;
  sb.Append("RTCEncodedAudioFrame{rtpTimestamp: ");
  sb.AppendNumber(delegate_->RtpTimestamp());
  sb.Append(", size: ");
  sb.AppendNumber(data(context) ? data(context)->ByteLength() : 0);
  sb.Append("}");
  return sb.ToString();
}

base::UnguessableToken RTCEncodedAudioFrame::OwnerId() {
  return owner_id_;
}
int64_t RTCEncodedAudioFrame::Counter() {
  return counter_;
}

void RTCEncodedAudioFrame::SyncDelegate() const {
  delegate_->SetData(frame_data_);
}

scoped_refptr<RTCEncodedAudioFrameDelegate> RTCEncodedAudioFrame::Delegate()
    const {
  SyncDelegate();
  return delegate_;
}

std::unique_ptr<webrtc::TransformableAudioFrameInterface>
RTCEncodedAudioFrame::PassWebRtcFrame(v8::Isolate* isolate,
                                      bool detach_frame_data) {
  SyncDelegate();
  auto transformable_audio_frame = delegate_->PassWebRtcFrame();
  // Detach the `frame_data_` ArrayBuffer if it's been created, as described in
  // the transfer on step 5 of the encoded transform spec write steps
  // (https://www.w3.org/TR/webrtc-encoded-transform/#stream-processing)
  if (detach_frame_data && frame_data_ && !frame_data_->IsDetached()) {
    CHECK(isolate);
    ArrayBufferContents contents_to_drop;
    NonThrowableExceptionState exception_state;
    CHECK(frame_data_->Transfer(isolate, v8::Local<v8::Value>(),
                                contents_to_drop, exception_state));
  }
  return transformable_audio_frame;
}

void RTCEncodedAudioFrame::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  visitor->Trace(frame_data_);
}

}  // namespace blink
