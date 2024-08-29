// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_video_frame.h"

#include <utility>

#include "base/unguessable_token.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_codec_specifics_vp_8.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_decode_target_indication.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_encoded_video_frame_metadata.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_encoded_video_frame_options.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_video_frame_delegate.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/webrtc/api/frame_transformer_interface.h"

namespace blink {

// Allow all fields to be set when calling RTCEncodedVideoFrame.setMetadata.
BASE_FEATURE(kAllowRTCEncodedVideoFrameSetMetadataAllFields,
             "AllowRTCEncodedVideoFrameSetMetadataAllFields",
             base::FEATURE_DISABLED_BY_DEFAULT);

namespace {
static constexpr size_t kMaxNumDependencies = 8;

bool IsAllowedSetMetadataChange(
    const RTCEncodedVideoFrameMetadata* original_metadata,
    const RTCEncodedVideoFrameMetadata* metadata) {
  if (metadata->width() != original_metadata->width() ||
      metadata->height() != original_metadata->height() ||
      metadata->spatialIndex() != original_metadata->spatialIndex() ||
      metadata->temporalIndex() != original_metadata->temporalIndex()) {
    return false;
  }

  // It is possible to not have the RTP metadata values set. This condition
  // checks if the value exists and if it does, it should be the same.
  if ((metadata->hasSynchronizationSource() !=
           original_metadata->hasSynchronizationSource() ||
       (metadata->hasSynchronizationSource()
            ? metadata->synchronizationSource() !=
                  original_metadata->synchronizationSource()
            : false)) ||
      (metadata->hasContributingSources() !=
           original_metadata->hasContributingSources() ||
       (metadata->hasContributingSources()
            ? metadata->contributingSources() !=
                  original_metadata->contributingSources()
            : false))) {
    return false;
  }
  return true;
}

base::expected<void, String> ValidateMetadata(
    const RTCEncodedVideoFrameMetadata* metadata) {
  if (!metadata->hasWidth() || !metadata->hasHeight() ||
      !metadata->hasSpatialIndex() || !metadata->hasTemporalIndex() ||
      !metadata->hasRtpTimestamp()) {
    return base::unexpected("new metadata has member(s) missing.");
  }

  // This might happen if the dependency descriptor is not set.
  if (!metadata->hasFrameId() && metadata->hasDependencies()) {
    return base::unexpected(
        "new metadata has frameID missing, but has dependencies");
  }
  if (!metadata->hasDependencies()) {
    return base::ok();
  }

  // Ensure there are at most 8 deps. Enforced in WebRTC's
  // RtpGenericFrameDescriptor::AddFrameDependencyDiff().
  if (metadata->dependencies().size() > kMaxNumDependencies) {
    return base::unexpected("new metadata has too many dependencies.");
  }
  // Require deps to all be before frame_id, but within 2^14 of it. Enforced in
  // WebRTC by a DCHECK in RtpGenericFrameDescriptor::AddFrameDependencyDiff().
  for (const int64_t dep : metadata->dependencies()) {
    if ((dep >= metadata->frameId()) ||
        ((metadata->frameId() - dep) >= (1 << 14))) {
      return base::unexpected("new metadata has invalid frame dependencies.");
    }
  }

  return base::ok();
}

}  // namespace

RTCEncodedVideoFrame* RTCEncodedVideoFrame::Create(
    RTCEncodedVideoFrame* original_frame,
    ExceptionState& exception_state) {
  return RTCEncodedVideoFrame::Create(original_frame, nullptr, exception_state);
}

RTCEncodedVideoFrame* RTCEncodedVideoFrame::Create(
    RTCEncodedVideoFrame* original_frame,
    const RTCEncodedVideoFrameOptions* options_dict,
    ExceptionState& exception_state) {
  RTCEncodedVideoFrame* new_frame;
  if (original_frame) {
    new_frame = MakeGarbageCollected<RTCEncodedVideoFrame>(
        original_frame->Delegate()->CloneWebRtcFrame());
  } else {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        "Cannot create a new VideoFrame from an empty VideoFrame");
    return nullptr;
  }
  if (options_dict && options_dict->hasMetadata()) {
    base::expected<void, String> set_metadata =
        new_frame->SetMetadata(options_dict->metadata());
    if (!set_metadata.has_value()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kInvalidModificationError,
          "Cannot create a new VideoFrame: " + set_metadata.error());
      return nullptr;
    }
  }
  return new_frame;
}

RTCEncodedVideoFrame::RTCEncodedVideoFrame(
    std::unique_ptr<webrtc::TransformableVideoFrameInterface> webrtc_frame)
    : RTCEncodedVideoFrame(std::move(webrtc_frame),
                           base::UnguessableToken::Null(),
                           0) {}

RTCEncodedVideoFrame::RTCEncodedVideoFrame(
    std::unique_ptr<webrtc::TransformableVideoFrameInterface> webrtc_frame,
    base::UnguessableToken owner_id,
    int64_t counter)
    : delegate_(base::MakeRefCounted<RTCEncodedVideoFrameDelegate>(
          std::move(webrtc_frame))),
      owner_id_(owner_id),
      counter_(counter) {}

RTCEncodedVideoFrame::RTCEncodedVideoFrame(
    scoped_refptr<RTCEncodedVideoFrameDelegate> delegate)
    : RTCEncodedVideoFrame(delegate->CloneWebRtcFrame()) {}

String RTCEncodedVideoFrame::type() const {
  return delegate_->Type();
}

uint32_t RTCEncodedVideoFrame::timestamp() const {
  return delegate_->RtpTimestamp();
}

DOMArrayBuffer* RTCEncodedVideoFrame::data(ExecutionContext* context) const {
  if (!frame_data_) {
    frame_data_ = delegate_->CreateDataBuffer(context->GetIsolate());
  }
  return frame_data_.Get();
}

RTCEncodedVideoFrameMetadata* RTCEncodedVideoFrame::getMetadata() const {
  RTCEncodedVideoFrameMetadata* metadata =
      RTCEncodedVideoFrameMetadata::Create();
  if (delegate_->PayloadType()) {
    metadata->setPayloadType(*delegate_->PayloadType());
  }
  if (delegate_->MimeType()) {
    metadata->setMimeType(WTF::String::FromUTF8(*delegate_->MimeType()));
  }

  if (RuntimeEnabledFeatures::RTCEncodedVideoFrameAdditionalMetadataEnabled()) {
    if (delegate_->PresentationTimestamp()) {
      metadata->setTimestamp(delegate_->PresentationTimestamp()->us());
    }
  }

  const std::optional<webrtc::VideoFrameMetadata> webrtc_metadata =
      delegate_->GetMetadata();
  if (!webrtc_metadata) {
    return metadata;
  }

  metadata->setSynchronizationSource(webrtc_metadata->GetSsrc());
  Vector<uint32_t> csrcs;
  for (uint32_t csrc : webrtc_metadata->GetCsrcs()) {
    csrcs.push_back(csrc);
  }
  metadata->setContributingSources(csrcs);

  if (webrtc_metadata->GetFrameId()) {
    metadata->setFrameId(*webrtc_metadata->GetFrameId());
  }

  Vector<int64_t> dependencies;
  for (const auto& dependency : webrtc_metadata->GetFrameDependencies()) {
    dependencies.push_back(dependency);
  }
  metadata->setDependencies(dependencies);
  metadata->setWidth(webrtc_metadata->GetWidth());
  metadata->setHeight(webrtc_metadata->GetHeight());
  metadata->setSpatialIndex(webrtc_metadata->GetSpatialIndex());
  metadata->setTemporalIndex(webrtc_metadata->GetTemporalIndex());
  metadata->setRtpTimestamp(delegate_->RtpTimestamp());

  return metadata;
}

base::UnguessableToken RTCEncodedVideoFrame::OwnerId() {
  return owner_id_;
}
int64_t RTCEncodedVideoFrame::Counter() {
  return counter_;
}

base::expected<void, String> RTCEncodedVideoFrame::SetMetadata(
    const RTCEncodedVideoFrameMetadata* metadata) {
  const std::optional<webrtc::VideoFrameMetadata> original_webrtc_metadata =
      delegate_->GetMetadata();
  if (!original_webrtc_metadata) {
    return base::unexpected("underlying webrtc frame is an empty frame.");
  }

  base::expected<void, String> validate_metadata = ValidateMetadata(metadata);
  if (!validate_metadata.has_value()) {
    return validate_metadata;
  }

  RTCEncodedVideoFrameMetadata* original_metadata = getMetadata();
  if (!original_metadata) {
    return base::unexpected("internal error when calling getMetadata().");
  }
  if (!IsAllowedSetMetadataChange(original_metadata, metadata) &&
      !base::FeatureList::IsEnabled(
          kAllowRTCEncodedVideoFrameSetMetadataAllFields)) {
    return base::unexpected(
        "invalid modification of RTCEncodedVideoFrameMetadata.");
  }

  if ((metadata->hasPayloadType() != original_metadata->hasPayloadType()) ||
      (metadata->hasPayloadType() &&
       metadata->payloadType() != original_metadata->payloadType())) {
    return base::unexpected(
        "invalid modification of payloadType in RTCEncodedVideoFrameMetadata.");
  }

  // Initialize the new metadata from original_metadata to account for fields
  // not part of RTCEncodedVideoFrameMetadata.
  webrtc::VideoFrameMetadata webrtc_metadata = *original_webrtc_metadata;
  if (metadata->hasFrameId()) {
    webrtc_metadata.SetFrameId(metadata->frameId());
  }
  if (metadata->hasDependencies()) {
    webrtc_metadata.SetFrameDependencies(metadata->dependencies());
  }
  webrtc_metadata.SetWidth(metadata->width());
  webrtc_metadata.SetHeight(metadata->height());
  webrtc_metadata.SetSpatialIndex(metadata->spatialIndex());
  webrtc_metadata.SetTemporalIndex(metadata->temporalIndex());
  webrtc_metadata.SetSsrc(metadata->synchronizationSource());

  if (metadata->hasContributingSources()) {
    std::vector<uint32_t> csrcs;
    for (uint32_t csrc : metadata->contributingSources()) {
      csrcs.push_back(csrc);
    }
    webrtc_metadata.SetCsrcs(csrcs);
  }

  return delegate_->SetMetadata(webrtc_metadata, metadata->rtpTimestamp());
}

void RTCEncodedVideoFrame::setMetadata(RTCEncodedVideoFrameMetadata* metadata,
                                       ExceptionState& exception_state) {
  base::expected<void, String> set_metadata = SetMetadata(metadata);
  if (!set_metadata.has_value()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidModificationError,
        "Cannot setMetadata: " + set_metadata.error());
  }
}

void RTCEncodedVideoFrame::setData(ExecutionContext*, DOMArrayBuffer* data) {
  frame_data_ = data;
}

String RTCEncodedVideoFrame::toString(ExecutionContext* context) const {
  if (!delegate_) {
    return "empty";
  }

  StringBuilder sb;
  sb.Append("RTCEncodedVideoFrame{rtpTimestamp: ");
  sb.AppendNumber(timestamp());
  sb.Append(", size: ");
  sb.AppendNumber(data(context)->ByteLength());
  sb.Append(" bytes, type: ");
  sb.Append(type());
  sb.Append("}");
  return sb.ToString();
}

void RTCEncodedVideoFrame::SyncDelegate() const {
  delegate_->SetData(frame_data_);
}

scoped_refptr<RTCEncodedVideoFrameDelegate> RTCEncodedVideoFrame::Delegate()
    const {
  SyncDelegate();
  return delegate_;
}

std::unique_ptr<webrtc::TransformableVideoFrameInterface>
RTCEncodedVideoFrame::PassWebRtcFrame(v8::Isolate* isolate,
                                      bool detach_frame_data) {
  SyncDelegate();
  auto transformable_video_frame = delegate_->PassWebRtcFrame();
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
  return transformable_video_frame;
}

void RTCEncodedVideoFrame::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  visitor->Trace(frame_data_);
}

}  // namespace blink
