// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_video_frame.h"

#include <utility>

#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_codec_specifics_vp_8.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_decode_target_indication.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_encoded_video_frame_metadata.h"
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

bool ValidateMetadata(const RTCEncodedVideoFrameMetadata* metadata,
                      String& error_message) {
  if (!metadata->hasWidth() || !metadata->hasHeight() ||
      !metadata->hasSpatialIndex() || !metadata->hasTemporalIndex() ||
      !metadata->hasRtpTimestamp()) {
    error_message = "Member(s) missing in RTCEncodedVideoFrameMetadata.";
    return false;
  }

  // This might happen if the dependency descriptor is not set.
  if (!metadata->hasFrameId() && metadata->hasDependencies()) {
    error_message =
        "frameID missing, but has dependencies in "
        "RTCEncodedVideoFrameMetadata.";
    return false;
  }
  if (!metadata->hasDependencies()) {
    return true;
  }

  // Ensure there are at most 8 deps. Enforced in WebRTC's
  // RtpGenericFrameDescriptor::AddFrameDependencyDiff().
  if (metadata->dependencies().size() > kMaxNumDependencies) {
    error_message = "Too many dependencies.";
    return false;
  }
  // Require deps to all be before frame_id, but within 2^14 of it. Enforced in
  // WebRTC by a DCHECK in RtpGenericFrameDescriptor::AddFrameDependencyDiff().
  for (const int64_t dep : metadata->dependencies()) {
    if ((dep >= metadata->frameId()) ||
        ((metadata->frameId() - dep) >= (1 << 14))) {
      error_message = "Invalid frame dependency.";
      return false;
    }
  }

  return true;
}

}  // namespace

RTCEncodedVideoFrame::RTCEncodedVideoFrame(
    std::unique_ptr<webrtc::TransformableVideoFrameInterface> webrtc_frame)
    : delegate_(base::MakeRefCounted<RTCEncodedVideoFrameDelegate>(
          std::move(webrtc_frame))) {}

RTCEncodedVideoFrame::RTCEncodedVideoFrame(
    scoped_refptr<RTCEncodedVideoFrameDelegate> delegate)
    : RTCEncodedVideoFrame(delegate->CloneWebRtcFrame()) {}

String RTCEncodedVideoFrame::type() const {
  return delegate_->Type();
}

uint32_t RTCEncodedVideoFrame::timestamp() const {
  return delegate_->RtpTimestamp();
}

void RTCEncodedVideoFrame::setTimestamp(uint32_t timestamp,
                                        ExceptionState& exception_state) {
  delegate_->SetRtpTimestamp(timestamp, exception_state);
}

DOMArrayBuffer* RTCEncodedVideoFrame::data() const {
  if (!frame_data_) {
    frame_data_ = delegate_->CreateDataBuffer();
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
    if (delegate_->CaptureTimeIdentifier()) {
      metadata->setCaptureTimestamp(delegate_->CaptureTimeIdentifier()->us());
    }
  }

  const absl::optional<webrtc::VideoFrameMetadata> webrtc_metadata =
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

void RTCEncodedVideoFrame::setMetadata(RTCEncodedVideoFrameMetadata* metadata,
                                       ExceptionState& exception_state) {
  const absl::optional<webrtc::VideoFrameMetadata> original_webrtc_metadata =
      delegate_->GetMetadata();
  if (!original_webrtc_metadata) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidModificationError,
        "Cannot set metadata on an empty frame.");
    return;
  }

  String error_message;
  if (!ValidateMetadata(metadata, error_message)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidModificationError, error_message);
    return;
  }

  RTCEncodedVideoFrameMetadata* original_metadata = getMetadata();
  if (!original_metadata) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidModificationError,
        "Internal error when calling setMetadata.");
    return;
  }
  if (!IsAllowedSetMetadataChange(original_metadata, metadata) &&
      !base::FeatureList::IsEnabled(
          kAllowRTCEncodedVideoFrameSetMetadataAllFields)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidModificationError,
        "Invalid modification of RTCEncodedVideoFrameMetadata.");
    return;
  }

  if ((metadata->hasPayloadType() != original_metadata->hasPayloadType()) ||
      (metadata->hasPayloadType() &&
       metadata->payloadType() != original_metadata->payloadType())) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidModificationError,
        "Invalid modification of payloadType in RTCEncodedVideoFrameMetadata.");
    return;
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

  delegate_->SetMetadata(webrtc_metadata);
  delegate_->SetRtpTimestamp(metadata->rtpTimestamp(), exception_state);
}

void RTCEncodedVideoFrame::setData(DOMArrayBuffer* data) {
  frame_data_ = data;
}

String RTCEncodedVideoFrame::toString() const {
  if (!delegate_) {
    return "empty";
  }

  StringBuilder sb;
  sb.Append("RTCEncodedVideoFrame{rtpTimestamp: ");
  sb.AppendNumber(timestamp());
  sb.Append(", size: ");
  sb.AppendNumber(data()->ByteLength());
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
RTCEncodedVideoFrame::PassWebRtcFrame() {
  SyncDelegate();
  return delegate_->PassWebRtcFrame();
}

void RTCEncodedVideoFrame::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  visitor->Trace(frame_data_);
}

}  // namespace blink
