// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_audio_frame.h"

#include <inttypes.h>

#include <algorithm>
#include <utility>

#include "base/containers/span.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/unguessable_token.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_audio_content_type.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_encoded_audio_frame_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_encoded_audio_frame_metadata.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_encoded_audio_frame_options.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/modules/peerconnection/peer_connection_util.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_audio_frame_delegate.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/webrtc/api/frame_transformer_factory.h"
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
  if (new_metadata->hasSequenceNumber() !=
          current_metadata->hasSequenceNumber() ||
      (new_metadata->hasSequenceNumber() &&
       current_metadata->sequenceNumber() != new_metadata->sequenceNumber())) {
    return SetMetadataValidationOutcome{false, "Bad sequenceNumber"};
  }
  // TODO(https://crbug.com/420408159): Make rtpTimestamp optional.
  if (!new_metadata->hasRtpTimestamp()) {
    return SetMetadataValidationOutcome{false, "Bad rtpTimestamp"};
  }
  if (RuntimeEnabledFeatures::RTCEncodedFrameTimestampsEnabled()) {
    if (new_metadata->hasReceiveTime() != current_metadata->hasReceiveTime() ||
        (new_metadata->hasReceiveTime() &&
         current_metadata->receiveTime() != new_metadata->receiveTime())) {
      return SetMetadataValidationOutcome{false, "Bad receiveTime"};
    }
    if (new_metadata->hasSenderCaptureTimeOffset() !=
            current_metadata->hasSenderCaptureTimeOffset() ||
        (new_metadata->hasSenderCaptureTimeOffset() &&
         current_metadata->senderCaptureTimeOffset() !=
             new_metadata->senderCaptureTimeOffset())) {
      return SetMetadataValidationOutcome{false, "Bad senderCaptureTimeOffset"};
    }
  }
  return SetMetadataValidationOutcome{true, String()};
}

}  // namespace

RTCEncodedAudioFrame* RTCEncodedAudioFrame::Create(
    ExecutionContext* execution_context,
    RTCEncodedAudioFrame* original_frame,
    ExceptionState& exception_state) {
  return RTCEncodedAudioFrame::Create(execution_context, original_frame,
                                      nullptr, exception_state);
}

RTCEncodedAudioFrame* RTCEncodedAudioFrame::Create(
    ExecutionContext* execution_context,
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
        new_frame->SetMetadata(execution_context, options_dict->metadata());
    if (!set_metadata.has_value()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kInvalidModificationError,
          StrCat({"Cannot create a new AudioFrame: ", set_metadata.error()}));
      return nullptr;
    }
  }
  return new_frame;
}

RTCEncodedAudioFrame* RTCEncodedAudioFrame::Create(
    ExecutionContext* execution_context,
    const RTCEncodedAudioFrameInit* init,
    ExceptionState& exception_state) {
  DOMArrayBuffer* payload_data_buffer = init->data();
  base::span<uint8_t> buffer_span = payload_data_buffer->ByteSpan();

  auto frame_type =
      webrtc::TransformableAudioFrameInterface::FrameType::kEmptyFrame;
  if (buffer_span.size() > 0) {
    frame_type =
        (init->contentType().AsEnum() == V8RTCAudioContentType::Enum::kSpeech)
            ? webrtc::TransformableAudioFrameInterface::FrameType::
                  kAudioFrameSpeech
            : webrtc::TransformableAudioFrameInterface::FrameType::
                  kAudioFrameCN;
  }

  uint8_t payload_type = init->payloadType();
  uint32_t rtp_timestamp_without_offset = init->rtpTimestampWithoutOffset();

  std::optional<uint64_t> absolute_capture_timestamp_ms;
  if (init->hasCaptureTime()) {
    base::TimeDelta capture_time = RTCEncodedFrameTimestampToCaptureTime(
        execution_context, init->captureTime(),
        CaptureTimeInfo::ClockType::kTimeTicks);
    absolute_capture_timestamp_ms = capture_time.InMilliseconds();
  }

  std::vector<uint32_t> csrcs(init->contributingSources().begin(),
                              init->contributingSources().end());
  std::string mime_type = init->hasMimeType() ? init->mimeType().Utf8() : "";

  std::optional<uint8_t> audio_level_dbov;
  if (init->hasAudioLevel()) {
    audio_level_dbov = FromLinearAudioLevel(init->audioLevel());
  }

  std::unique_ptr<webrtc::TransformableAudioFrameInterface> webrtc_frame =
      webrtc::CreateOutgoingAudioFrame(
          frame_type, payload_type, rtp_timestamp_without_offset,
          buffer_span.data(), buffer_span.size(), absolute_capture_timestamp_ms,
          /*ssrc*/ 0, csrcs, mime_type,
          /*sequence_number=*/std::nullopt, audio_level_dbov);

  return MakeGarbageCollected<RTCEncodedAudioFrame>(std::move(webrtc_frame));
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
  return delegate_->RtpTimestamp().value_or(0);
}

DOMArrayBuffer* RTCEncodedAudioFrame::data(ExecutionContext* context) const {
  if (!frame_data_) {
    frame_data_ = delegate_->CreateDataBuffer(context->GetIsolate());
  }
  return frame_data_.Get();
}

RTCEncodedAudioFrameMetadata* RTCEncodedAudioFrame::getMetadata(
    ExecutionContext* execution_context) const {
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
  if (delegate_->RtpTimestamp()) {
    metadata->setRtpTimestamp(*delegate_->RtpTimestamp());
  }
  if (delegate_->MimeType()) {
    metadata->setMimeType(String::FromUtf8(*delegate_->MimeType()));
  }
  if (RuntimeEnabledFeatures::RTCEncodedFrameTimestampsEnabled()) {
    if (std::optional<base::TimeTicks> receive_time =
            delegate_->ReceiveTime()) {
      metadata->setReceiveTime(
          RTCTimeStampFromTimeTicks(execution_context, *receive_time));
    }
    if (std::optional<CaptureTimeInfo> capture_time_info =
            delegate_->CaptureTime()) {
      metadata->setCaptureTime(RTCEncodedFrameTimestampFromCaptureTimeInfo(
          execution_context, *capture_time_info));
    }
    if (std::optional<base::TimeDelta> sender_capture_time_offset =
            delegate_->SenderCaptureTimeOffset()) {
      metadata->setSenderCaptureTimeOffset(CalculateRTCEncodedFrameTimeDelta(
          execution_context, *sender_capture_time_offset));
    }
  }
  if (std::optional<double> audio_level_dbov = delegate_->AudioLevel()) {
    metadata->setAudioLevel(*audio_level_dbov);
  }
  return metadata;
}

base::expected<void, String> RTCEncodedAudioFrame::SetMetadata(
    ExecutionContext* execution_context,
    const RTCEncodedAudioFrameMetadata* metadata) {
  SetMetadataValidationOutcome validation =
      IsAllowedSetMetadataChange(getMetadata(execution_context), metadata);
  if (!validation.allowed) {
    return base::unexpected(
        StrCat({"Invalid modification of RTCEncodedAudioFrameMetadata. ",
                validation.error_msg}));
  }

  return delegate_->SetWebRtcFrameMetadata(execution_context, metadata);
}

void RTCEncodedAudioFrame::setMetadata(ExecutionContext* execution_context,
                                       RTCEncodedAudioFrameMetadata* metadata,
                                       ExceptionState& exception_state) {
  base::expected<void, String> set_metadata =
      SetMetadata(execution_context, metadata);
  if (!set_metadata.has_value()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidModificationError,
        StrCat({"Cannot setMetadata: ", set_metadata.error()}));
  }
}

void RTCEncodedAudioFrame::setData(ExecutionContext*, DOMArrayBuffer* data) {
  frame_data_ = data;
}

String RTCEncodedAudioFrame::toString(ExecutionContext* context) const {
  StringBuilder sb;
  sb.Append("RTCEncodedAudioFrame{");
  if (std::optional<uint32_t> rtp_timestamp = delegate_->RtpTimestamp()) {
    sb.Append("rtpTimestamp: ");
    sb.AppendNumber(*rtp_timestamp);
    sb.Append(", ");
  }
  sb.Append("size: ");
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
