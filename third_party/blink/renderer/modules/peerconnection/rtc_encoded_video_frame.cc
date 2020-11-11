// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_video_frame.h"

#include <utility>

#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_encoded_video_frame_metadata.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_video_frame_delegate.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/webrtc/api/frame_transformer_interface.h"

namespace blink {

RTCEncodedVideoFrame::RTCEncodedVideoFrame(
    std::unique_ptr<webrtc::TransformableVideoFrameInterface> webrtc_frame)
    : delegate_(base::MakeRefCounted<RTCEncodedVideoFrameDelegate>(
          std::move(webrtc_frame))) {}

RTCEncodedVideoFrame::RTCEncodedVideoFrame(
    scoped_refptr<RTCEncodedVideoFrameDelegate> delegate)
    : delegate_(std::move(delegate)) {}

String RTCEncodedVideoFrame::type() const {
  return delegate_->Type();
}

uint64_t RTCEncodedVideoFrame::timestamp() const {
  return delegate_->Timestamp();
}

DOMArrayBuffer* RTCEncodedVideoFrame::data() const {
  if (!frame_data_)
    frame_data_ = delegate_->CreateDataBuffer();
  return frame_data_;
}

RTCEncodedVideoFrameMetadata* RTCEncodedVideoFrame::getMetadata() const {
  RTCEncodedVideoFrameMetadata* metadata =
      RTCEncodedVideoFrameMetadata::Create();
  metadata->setSynchronizationSource(delegate_->Ssrc());
  const auto* webrtc_metadata = delegate_->GetMetadata();
  if (!webrtc_metadata)
    return metadata;

  if (webrtc_metadata->GetFrameId())
    metadata->setFrameId(*webrtc_metadata->GetFrameId());

  Vector<int64_t> dependencies;
  for (const auto& dependency : webrtc_metadata->GetFrameDependencies())
    dependencies.push_back(dependency);
  metadata->setDependencies(dependencies);
  metadata->setWidth(webrtc_metadata->GetWidth());
  metadata->setHeight(webrtc_metadata->GetHeight());
  metadata->setSpatialIndex(webrtc_metadata->GetSpatialIndex());
  metadata->setTemporalIndex(webrtc_metadata->GetTemporalIndex());
  return metadata;
}

DOMArrayBuffer* RTCEncodedVideoFrame::additionalData() const {
  if (!additional_data_)
    additional_data_ = delegate_->CreateAdditionalDataBuffer();
  return additional_data_;
}

uint32_t RTCEncodedVideoFrame::synchronizationSource() const {
  return delegate_->Ssrc();
}

void RTCEncodedVideoFrame::setData(DOMArrayBuffer* data) {
  frame_data_ = data;
}

String RTCEncodedVideoFrame::toString() const {
  if (!delegate_)
    return "empty";

  StringBuilder sb;
  sb.Append("RTCEncodedVideoFrame{timestamp: ");
  sb.AppendNumber(timestamp());
  sb.Append("us, size: ");
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
  visitor->Trace(additional_data_);
}

}  // namespace blink
