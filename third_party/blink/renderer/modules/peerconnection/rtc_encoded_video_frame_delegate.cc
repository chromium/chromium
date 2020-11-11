// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_video_frame_delegate.h"

#include <utility>

#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/webrtc/api/frame_transformer_interface.h"

namespace blink {

const void* const RTCEncodedVideoFramesAttachment::kAttachmentKey = nullptr;

RTCEncodedVideoFrameDelegate::RTCEncodedVideoFrameDelegate(
    std::unique_ptr<webrtc::TransformableVideoFrameInterface> webrtc_frame)
    : webrtc_frame_(std::move(webrtc_frame)) {}

String RTCEncodedVideoFrameDelegate::Type() const {
  MutexLocker lock(mutex_);
  if (!webrtc_frame_)
    return "empty";

  return webrtc_frame_->IsKeyFrame() ? "key" : "delta";
}

uint64_t RTCEncodedVideoFrameDelegate::Timestamp() const {
  MutexLocker lock(mutex_);
  return webrtc_frame_ ? webrtc_frame_->GetTimestamp() : 0;
}

DOMArrayBuffer* RTCEncodedVideoFrameDelegate::CreateDataBuffer() const {
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

void RTCEncodedVideoFrameDelegate::SetData(const DOMArrayBuffer* data) {
  MutexLocker lock(mutex_);
  if (webrtc_frame_ && data) {
    webrtc_frame_->SetData(rtc::ArrayView<const uint8_t>(
        static_cast<const uint8_t*>(data->Data()), data->ByteLength()));
  }
}

DOMArrayBuffer* RTCEncodedVideoFrameDelegate::CreateAdditionalDataBuffer()
    const {
  ArrayBufferContents contents;
  {
    MutexLocker lock(mutex_);
    if (!webrtc_frame_)
      return nullptr;

    auto additional_data = webrtc_frame_->GetAdditionalData();
    contents = ArrayBufferContents(additional_data.size(), 1,
                                   ArrayBufferContents::kNotShared,
                                   ArrayBufferContents::kDontInitialize);
    if (UNLIKELY(!contents.Data()))
      OOM_CRASH(additional_data.size());
    memcpy(contents.Data(), additional_data.data(), additional_data.size());
  }
  return DOMArrayBuffer::Create(std::move(contents));
}

uint32_t RTCEncodedVideoFrameDelegate::Ssrc() const {
  MutexLocker lock(mutex_);
  return webrtc_frame_ ? webrtc_frame_->GetSsrc() : 0;
}

const webrtc::VideoFrameMetadata* RTCEncodedVideoFrameDelegate::GetMetadata()
    const {
  MutexLocker lock(mutex_);
  return webrtc_frame_ ? &webrtc_frame_->GetMetadata() : nullptr;
}

std::unique_ptr<webrtc::TransformableVideoFrameInterface>
RTCEncodedVideoFrameDelegate::PassWebRtcFrame() {
  MutexLocker lock(mutex_);
  return std::move(webrtc_frame_);
}

}  // namespace blink
