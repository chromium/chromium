// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_audio_frame_delegate.h"

#include <utility>

#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"

namespace blink {

const void* RTCEncodedAudioFramesAttachment::kAttachmentKey;

RTCEncodedAudioFrameDelegate::RTCEncodedAudioFrameDelegate(
    std::unique_ptr<webrtc::TransformableFrameInterface> webrtc_frame,
    Vector<uint32_t> contributing_sources)
    : webrtc_frame_(std::move(webrtc_frame)),
      contributing_sources_(std::move(contributing_sources)) {}

uint64_t RTCEncodedAudioFrameDelegate::Timestamp() const {
  MutexLocker lock(mutex_);
  return webrtc_frame_ ? webrtc_frame_->GetTimestamp() : 0;
}

DOMArrayBuffer* RTCEncodedAudioFrameDelegate::CreateDataBuffer() const {
  ArrayBufferContents contents;
  {
    MutexLocker lock(mutex_);
    if (!webrtc_frame_)
      return nullptr;

    auto data = webrtc_frame_->GetData();
    contents =
        ArrayBufferContents(data.size(), 1, ArrayBufferContents::kNotShared,
                            ArrayBufferContents::kDontInitialize);
    if (UNLIKELY(!contents.Data()))
      OOM_CRASH(data.size());
    memcpy(contents.Data(), data.data(), data.size());
  }
  return DOMArrayBuffer::Create(std::move(contents));
}

void RTCEncodedAudioFrameDelegate::SetData(const DOMArrayBuffer* data) {
  MutexLocker lock(mutex_);
  if (webrtc_frame_ && data) {
    webrtc_frame_->SetData(rtc::ArrayView<const uint8_t>(
        static_cast<const uint8_t*>(data->Data()), data->ByteLength()));
  }
}

uint32_t RTCEncodedAudioFrameDelegate::Ssrc() const {
  MutexLocker lock(mutex_);
  return webrtc_frame_ ? webrtc_frame_->GetSsrc() : 0;
}

Vector<uint32_t> RTCEncodedAudioFrameDelegate::ContributingSources() const {
  MutexLocker lock(mutex_);
  return contributing_sources_;
}

std::unique_ptr<webrtc::TransformableFrameInterface>
RTCEncodedAudioFrameDelegate::PassWebRtcFrame() {
  MutexLocker lock(mutex_);
  return std::move(webrtc_frame_);
}

}  // namespace blink
