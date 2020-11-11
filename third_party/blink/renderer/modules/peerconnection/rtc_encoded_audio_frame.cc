// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_audio_frame.h"

#include <utility>

#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_encoded_audio_frame_metadata.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_audio_frame_delegate.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/webrtc/api/frame_transformer_interface.h"

namespace blink {

RTCEncodedAudioFrame::RTCEncodedAudioFrame(
    std::unique_ptr<webrtc::TransformableFrameInterface> webrtc_frame)
    : delegate_(base::MakeRefCounted<RTCEncodedAudioFrameDelegate>(
          std::move(webrtc_frame),
          Vector<uint32_t>())) {}

RTCEncodedAudioFrame::RTCEncodedAudioFrame(
    std::unique_ptr<webrtc::TransformableAudioFrameInterface>
        webrtc_audio_frame) {
  Vector<uint32_t> contributing_sources;
  if (webrtc_audio_frame) {
    wtf_size_t num_csrcs = webrtc_audio_frame->GetHeader().numCSRCs;
    contributing_sources.ReserveInitialCapacity(num_csrcs);
    for (wtf_size_t i = 0; i < num_csrcs; i++) {
      contributing_sources.push_back(
          webrtc_audio_frame->GetHeader().arrOfCSRCs[i]);
    }
  }
  delegate_ = base::MakeRefCounted<RTCEncodedAudioFrameDelegate>(
      std::move(webrtc_audio_frame), std::move(contributing_sources));
}

RTCEncodedAudioFrame::RTCEncodedAudioFrame(
    scoped_refptr<RTCEncodedAudioFrameDelegate> delegate)
    : delegate_(std::move(delegate)) {}

uint64_t RTCEncodedAudioFrame::timestamp() const {
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
  metadata->setSynchronizationSource(delegate_->Ssrc());
  metadata->setContributingSources(delegate_->ContributingSources());
  return metadata;
}

DOMArrayBuffer* RTCEncodedAudioFrame::additionalData() const {
  return nullptr;
}

void RTCEncodedAudioFrame::setData(DOMArrayBuffer* data) {
  frame_data_ = data;
}

uint32_t RTCEncodedAudioFrame::synchronizationSource() const {
  return delegate_->Ssrc();
}

Vector<uint32_t> RTCEncodedAudioFrame::contributingSources() const {
  return delegate_->ContributingSources();
}

String RTCEncodedAudioFrame::toString() const {
  StringBuilder sb;
  sb.Append("RTCEncodedAudioFrame{timestamp: ");
  sb.AppendNumber(timestamp());
  sb.Append("us, size: ");
  sb.AppendNumber(data() ? data()->ByteLength() : 0);
  sb.Append("}");
  return sb.ToString();
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
