// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_video_frame_delegate.h"

#include <utility>

#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/webrtc/api/frame_transformer_factory.h"
#include "third_party/webrtc/api/frame_transformer_interface.h"

namespace blink {

const void* const RTCEncodedVideoFramesAttachment::kAttachmentKey = nullptr;

RTCEncodedVideoFrameDelegate::RTCEncodedVideoFrameDelegate(
    std::unique_ptr<webrtc::TransformableVideoFrameInterface> webrtc_frame)
    : webrtc_frame_(std::move(webrtc_frame)) {}

String RTCEncodedVideoFrameDelegate::Type() const {
  base::AutoLock lock(lock_);
  if (!webrtc_frame_)
    return "empty";

  return webrtc_frame_->IsKeyFrame() ? "key" : "delta";
}

uint32_t RTCEncodedVideoFrameDelegate::Timestamp() const {
  base::AutoLock lock(lock_);
  return webrtc_frame_ ? webrtc_frame_->GetTimestamp() : 0;
}

void RTCEncodedVideoFrameDelegate::SetTimestamp(
    uint32_t timestamp,
    ExceptionState& exception_state) {
  base::AutoLock lock(lock_);
  if (webrtc_frame_) {
    webrtc_frame_->SetRTPTimestamp(timestamp);
  } else {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Video frame is empty.");
  }
}

absl::optional<webrtc::Timestamp>
RTCEncodedVideoFrameDelegate::CaptureTimeIdentifier() const {
  base::AutoLock lock(lock_);
  return webrtc_frame_ ? webrtc_frame_->GetCaptureTimeIdentifier()
                       : absl::nullopt;
}

DOMArrayBuffer* RTCEncodedVideoFrameDelegate::CreateDataBuffer() const {
  ArrayBufferContents contents;
  {
    base::AutoLock lock(lock_);
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
  base::AutoLock lock(lock_);
  if (webrtc_frame_ && data) {
    webrtc_frame_->SetData(rtc::ArrayView<const uint8_t>(
        static_cast<const uint8_t*>(data->Data()), data->ByteLength()));
  }
}

absl::optional<uint8_t> RTCEncodedVideoFrameDelegate::PayloadType() const {
  base::AutoLock lock(lock_);
  return webrtc_frame_ ? absl::make_optional(webrtc_frame_->GetPayloadType())
                       : absl::nullopt;
}

absl::optional<webrtc::VideoFrameMetadata>
RTCEncodedVideoFrameDelegate::GetMetadata() const {
  base::AutoLock lock(lock_);
  return webrtc_frame_ ? absl::optional<webrtc::VideoFrameMetadata>(
                             webrtc_frame_->Metadata())
                       : absl::nullopt;
}

void RTCEncodedVideoFrameDelegate::SetMetadata(
    const webrtc::VideoFrameMetadata& metadata) {
  base::AutoLock lock(lock_);
  if (!webrtc_frame_) {
    return;
  }
  webrtc_frame_->SetMetadata(metadata);
}

std::unique_ptr<webrtc::TransformableVideoFrameInterface>
RTCEncodedVideoFrameDelegate::PassWebRtcFrame() {
  base::AutoLock lock(lock_);
  return std::move(webrtc_frame_);
}

std::unique_ptr<webrtc::TransformableVideoFrameInterface>
RTCEncodedVideoFrameDelegate::CloneWebRtcFrame() {
  base::AutoLock lock(lock_);
  if (!webrtc_frame_) {
    return nullptr;
  }
  return webrtc::CloneVideoFrame(webrtc_frame_.get());
}

}  // namespace blink
