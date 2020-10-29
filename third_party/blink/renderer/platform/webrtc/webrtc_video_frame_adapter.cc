// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/webrtc/webrtc_video_frame_adapter.h"

#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/modules/webrtc/webrtc_logging.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/libyuv/include/libyuv/convert.h"
#include "third_party/libyuv/include/libyuv/scale.h"
#include "third_party/webrtc/api/video/i420_buffer.h"
#include "third_party/webrtc/common_video/include/video_frame_buffer.h"
#include "third_party/webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "third_party/webrtc/rtc_base/ref_counted_object.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace {

class I420FrameAdapter : public webrtc::I420BufferInterface {
 public:
  explicit I420FrameAdapter(scoped_refptr<media::VideoFrame> frame)
      : frame_(std::move(frame)) {
    DCHECK_EQ(frame_->format(), media::PIXEL_FORMAT_I420);
    DCHECK_EQ(frame_->visible_rect().size(), frame_->natural_size());
  }

  int width() const override { return frame_->visible_rect().width(); }
  int height() const override { return frame_->visible_rect().height(); }

  const uint8_t* DataY() const override {
    return frame_->visible_data(media::VideoFrame::kYPlane);
  }

  const uint8_t* DataU() const override {
    return frame_->visible_data(media::VideoFrame::kUPlane);
  }

  const uint8_t* DataV() const override {
    return frame_->visible_data(media::VideoFrame::kVPlane);
  }

  int StrideY() const override {
    return frame_->stride(media::VideoFrame::kYPlane);
  }

  int StrideU() const override {
    return frame_->stride(media::VideoFrame::kUPlane);
  }

  int StrideV() const override {
    return frame_->stride(media::VideoFrame::kVPlane);
  }

 protected:
  scoped_refptr<media::VideoFrame> frame_;
};

class I420AFrameAdapter : public webrtc::I420ABufferInterface {
 public:
  explicit I420AFrameAdapter(scoped_refptr<media::VideoFrame> frame)
      : frame_(std::move(frame)) {
    DCHECK_EQ(frame_->format(), media::PIXEL_FORMAT_I420A);
    DCHECK_EQ(frame_->visible_rect().size(), frame_->natural_size());
  }

  int width() const override { return frame_->visible_rect().width(); }
  int height() const override { return frame_->visible_rect().height(); }

  const uint8_t* DataY() const override {
    return frame_->visible_data(media::VideoFrame::kYPlane);
  }

  const uint8_t* DataU() const override {
    return frame_->visible_data(media::VideoFrame::kUPlane);
  }

  const uint8_t* DataV() const override {
    return frame_->visible_data(media::VideoFrame::kVPlane);
  }

  const uint8_t* DataA() const override {
    return frame_->visible_data(media::VideoFrame::kAPlane);
  }

  int StrideY() const override {
    return frame_->stride(media::VideoFrame::kYPlane);
  }

  int StrideU() const override {
    return frame_->stride(media::VideoFrame::kUPlane);
  }

  int StrideV() const override {
    return frame_->stride(media::VideoFrame::kVPlane);
  }

  int StrideA() const override {
    return frame_->stride(media::VideoFrame::kAPlane);
  }

 protected:
  scoped_refptr<media::VideoFrame> frame_;
};

class NV12FrameAdapter : public webrtc::NV12BufferInterface {
 public:
  explicit NV12FrameAdapter(scoped_refptr<media::VideoFrame> frame)
      : frame_(std::move(frame)) {
    DCHECK_EQ(frame_->format(), media::PIXEL_FORMAT_NV12);
    DCHECK_EQ(frame_->visible_rect().size(), frame_->natural_size());
  }

  int width() const override { return frame_->visible_rect().width(); }
  int height() const override { return frame_->visible_rect().height(); }

  const uint8_t* DataY() const override {
    return frame_->visible_data(media::VideoFrame::kYPlane);
  }

  const uint8_t* DataUV() const override {
    return frame_->visible_data(media::VideoFrame::kUVPlane);
  }

  int StrideY() const override {
    return frame_->stride(media::VideoFrame::kYPlane);
  }

  int StrideUV() const override {
    return frame_->stride(media::VideoFrame::kUVPlane);
  }

  rtc::scoped_refptr<webrtc::I420BufferInterface> ToI420() override {
    rtc::scoped_refptr<webrtc::I420Buffer> i420_buffer;
    i420_buffer = webrtc::I420Buffer::Create(width(), height());
    libyuv::NV12ToI420(DataY(), StrideY(), DataUV(), StrideUV(),
                       i420_buffer->MutableDataY(), i420_buffer->StrideY(),
                       i420_buffer->MutableDataU(), i420_buffer->StrideU(),
                       i420_buffer->MutableDataV(), i420_buffer->StrideV(),
                       width(), height());
    return i420_buffer;
  }

 protected:
  scoped_refptr<media::VideoFrame> frame_;
};

rtc::scoped_refptr<webrtc::VideoFrameBuffer> MakeFrameAdapter(
    scoped_refptr<media::VideoFrame> video_frame) {
  switch (video_frame->format()) {
    case media::PIXEL_FORMAT_I420:
      return new rtc::RefCountedObject<I420FrameAdapter>(
          std::move(video_frame));
    case media::PIXEL_FORMAT_I420A:
      return new rtc::RefCountedObject<I420AFrameAdapter>(
          std::move(video_frame));
    case media::PIXEL_FORMAT_NV12:
      return new rtc::RefCountedObject<NV12FrameAdapter>(
          std::move(video_frame));
    default:
      NOTREACHED();
      return nullptr;
  }
}

void IsValidFrame(const media::VideoFrame& frame) {
  // Paranoia checks.
  DCHECK(media::VideoFrame::IsValidConfig(
      frame.format(), frame.storage_type(), frame.coded_size(),
      frame.visible_rect(), frame.natural_size()));
  DCHECK(media::PIXEL_FORMAT_I420 == frame.format() ||
         media::PIXEL_FORMAT_I420A == frame.format() ||
         media::PIXEL_FORMAT_NV12 == frame.format());
  if (media::PIXEL_FORMAT_NV12 == frame.format()) {
    CHECK(
        reinterpret_cast<const void*>(frame.data(media::VideoFrame::kYPlane)));
    CHECK(
        reinterpret_cast<const void*>(frame.data(media::VideoFrame::kUVPlane)));
    CHECK(frame.stride(media::VideoFrame::kYPlane));
    CHECK(frame.stride(media::VideoFrame::kUVPlane));
  } else {
    CHECK(
        reinterpret_cast<const void*>(frame.data(media::VideoFrame::kYPlane)));
    CHECK(
        reinterpret_cast<const void*>(frame.data(media::VideoFrame::kUPlane)));
    CHECK(
        reinterpret_cast<const void*>(frame.data(media::VideoFrame::kVPlane)));
    CHECK(frame.stride(media::VideoFrame::kYPlane));
    CHECK(frame.stride(media::VideoFrame::kUPlane));
    CHECK(frame.stride(media::VideoFrame::kVPlane));
  }
}

scoped_refptr<media::VideoFrame> WrapGmbVideoFrameForMappedMemoryAccess(
    scoped_refptr<media::VideoFrame> source_frame) {
  DCHECK_EQ(source_frame->natural_size(), source_frame->visible_rect().size());
  gfx::GpuMemoryBuffer* gmb = source_frame->GetGpuMemoryBuffer();
  if (!gmb || !gmb->Map()) {
    return nullptr;
  }
  // Y and UV planes from the gmb.
  uint8_t* plane_addresses[2] = {static_cast<uint8_t*>(gmb->memory(0)),
                                 static_cast<uint8_t*>(gmb->memory(1))};
  scoped_refptr<media::VideoFrame> destination_frame =
      media::VideoFrame::WrapExternalYuvData(
          media::VideoPixelFormat::PIXEL_FORMAT_NV12,
          source_frame->coded_size(), source_frame->visible_rect(),
          source_frame->natural_size(), gmb->stride(0), gmb->stride(1),
          plane_addresses[0], plane_addresses[1], source_frame->timestamp());
  if (!destination_frame) {
    gmb->Unmap();
    LOG(ERROR) << "Failed to wrap gmb buffer";
    return nullptr;
  }
  destination_frame->set_color_space(source_frame->ColorSpace());
  destination_frame->metadata()->MergeMetadataFrom(source_frame->metadata());
  destination_frame->AddDestructionObserver(WTF::Bind(
      [](scoped_refptr<media::VideoFrame> frame) {
        CHECK(frame->HasGpuMemoryBuffer());
        frame->GetGpuMemoryBuffer()->Unmap();
      },
      std::move(source_frame)));
  return destination_frame;
}

scoped_refptr<media::VideoFrame> MakeScaledI420VideoFrame(
    scoped_refptr<media::VideoFrame> source_frame,
    scoped_refptr<blink::WebRtcVideoFrameAdapter::BufferPoolOwner>
        scaled_frame_pool) {
  gfx::GpuMemoryBuffer* gmb = source_frame->GetGpuMemoryBuffer();
  if (!gmb || !gmb->Map()) {
    return nullptr;
  }
  // Crop to the visible rectangle specified in |source_frame|.
  const uint8_t* src_y = (reinterpret_cast<const uint8_t*>(gmb->memory(0)) +
                          source_frame->visible_rect().x() +
                          (source_frame->visible_rect().y() * gmb->stride(0)));
  const uint8_t* src_uv =
      (reinterpret_cast<const uint8_t*>(gmb->memory(1)) +
       ((source_frame->visible_rect().x() / 2) * 2) +
       ((source_frame->visible_rect().y() / 2) * gmb->stride(1)));

  // Convert to I420 and scale to the natural size specified in
  // |source_frame|.
  auto dst_frame = scaled_frame_pool->CreateFrame(
      media::PIXEL_FORMAT_I420, source_frame->natural_size(),
      gfx::Rect(source_frame->natural_size()), source_frame->natural_size(),
      source_frame->timestamp());
  if (!dst_frame) {
    gmb->Unmap();
    LOG(ERROR) << "Failed to create I420 frame from pool.";
    return nullptr;
  }
  dst_frame->metadata()->MergeMetadataFrom(source_frame->metadata());
  const auto& i420_planes = dst_frame->layout().planes();
  webrtc::NV12ToI420Scaler scaler;
  scaler.NV12ToI420Scale(src_y, gmb->stride(0), src_uv, gmb->stride(1),
                         source_frame->visible_rect().width(),
                         source_frame->visible_rect().height(),
                         dst_frame->data(media::VideoFrame::kYPlane),
                         i420_planes[media::VideoFrame::kYPlane].stride,
                         dst_frame->data(media::VideoFrame::kUPlane),
                         i420_planes[media::VideoFrame::kUPlane].stride,
                         dst_frame->data(media::VideoFrame::kVPlane),
                         i420_planes[media::VideoFrame::kVPlane].stride,
                         dst_frame->coded_size().width(),
                         dst_frame->coded_size().height());
  gmb->Unmap();
  return dst_frame;
}

scoped_refptr<media::VideoFrame> MakeScaledNV12VideoFrame(
    scoped_refptr<media::VideoFrame> source_frame,
    scoped_refptr<blink::WebRtcVideoFrameAdapter::BufferPoolOwner>
        scaled_frame_pool) {
  gfx::GpuMemoryBuffer* gmb = source_frame->GetGpuMemoryBuffer();
  if (!gmb || !gmb->Map()) {
    return nullptr;
  }
  // Crop to the visible rectangle specified in |source_frame|.
  const uint8_t* src_y = (reinterpret_cast<const uint8_t*>(gmb->memory(0)) +
                          source_frame->visible_rect().x() +
                          (source_frame->visible_rect().y() * gmb->stride(0)));
  const uint8_t* src_uv =
      (reinterpret_cast<const uint8_t*>(gmb->memory(1)) +
       ((source_frame->visible_rect().x() / 2) * 2) +
       ((source_frame->visible_rect().y() / 2) * gmb->stride(1)));

  auto dst_frame = scaled_frame_pool->CreateFrame(
      media::PIXEL_FORMAT_NV12, source_frame->natural_size(),
      gfx::Rect(source_frame->natural_size()), source_frame->natural_size(),
      source_frame->timestamp());
  dst_frame->metadata()->MergeMetadataFrom(source_frame->metadata());
  const auto& nv12_planes = dst_frame->layout().planes();
  libyuv::NV12Scale(src_y, gmb->stride(0), src_uv, gmb->stride(1),
                    source_frame->visible_rect().width(),
                    source_frame->visible_rect().height(),
                    dst_frame->data(media::VideoFrame::kYPlane),
                    nv12_planes[media::VideoFrame::kYPlane].stride,
                    dst_frame->data(media::VideoFrame::kUVPlane),
                    nv12_planes[media::VideoFrame::kUVPlane].stride,
                    dst_frame->coded_size().width(),
                    dst_frame->coded_size().height(), libyuv::kFilterBox);
  gmb->Unmap();
  return dst_frame;
}

scoped_refptr<media::VideoFrame> ConstructVideoFrameFromGpu(
    scoped_refptr<media::VideoFrame> source_frame,
    scoped_refptr<blink::WebRtcVideoFrameAdapter::BufferPoolOwner>
        scaled_frame_pool) {
  CHECK(source_frame);
  CHECK(scaled_frame_pool);
  // NV12 is the only supported format.
  DCHECK_EQ(source_frame->format(), media::PIXEL_FORMAT_NV12);
  DCHECK_EQ(source_frame->storage_type(),
            media::VideoFrame::STORAGE_GPU_MEMORY_BUFFER);

  // Convert to I420 and scale to the natural size specified in |source_frame|.
  const bool dont_convert_nv12_image =
      base::FeatureList::IsEnabled(blink::features::kWebRtcLibvpxEncodeNV12);
  if (!dont_convert_nv12_image) {
    return MakeScaledI420VideoFrame(std::move(source_frame),
                                    std::move(scaled_frame_pool));
  } else if (source_frame->natural_size() ==
             source_frame->visible_rect().size()) {
    return WrapGmbVideoFrameForMappedMemoryAccess(std::move(source_frame));
  } else {
    return MakeScaledNV12VideoFrame(std::move(source_frame),
                                    std::move(scaled_frame_pool));
  }
}

}  // anonymous namespace

namespace blink {

scoped_refptr<media::VideoFrame>
WebRtcVideoFrameAdapter::BufferPoolOwner::CreateFrame(
    media::VideoPixelFormat format,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    base::TimeDelta timestamp) {
  return pool_.CreateFrame(format, coded_size, visible_rect, natural_size,
                           timestamp);
}

WebRtcVideoFrameAdapter::BufferPoolOwner::BufferPoolOwner() = default;

WebRtcVideoFrameAdapter::BufferPoolOwner::~BufferPoolOwner() = default;

WebRtcVideoFrameAdapter::WebRtcVideoFrameAdapter(
    scoped_refptr<media::VideoFrame> frame)
    : WebRtcVideoFrameAdapter(frame, nullptr) {}

WebRtcVideoFrameAdapter::WebRtcVideoFrameAdapter(
    scoped_refptr<media::VideoFrame> frame,
    scoped_refptr<BufferPoolOwner> scaled_frame_pool)
    : frame_(std::move(frame)), scaled_frame_pool_(scaled_frame_pool) {}

WebRtcVideoFrameAdapter::~WebRtcVideoFrameAdapter() {}

webrtc::VideoFrameBuffer::Type WebRtcVideoFrameAdapter::type() const {
  return Type::kNative;
}

int WebRtcVideoFrameAdapter::width() const {
  return frame_->natural_size().width();
}

int WebRtcVideoFrameAdapter::height() const {
  return frame_->natural_size().height();
}

rtc::scoped_refptr<webrtc::VideoFrameBuffer>
WebRtcVideoFrameAdapter::CreateFrameAdapter() const {
  if (frame_->storage_type() ==
      media::VideoFrame::StorageType::STORAGE_GPU_MEMORY_BUFFER) {
    auto video_frame = ConstructVideoFrameFromGpu(frame_, scaled_frame_pool_);
    if (!video_frame) {
      return MakeFrameAdapter(media::VideoFrame::CreateColorFrame(
          frame_->natural_size(), 0u, 0x80, 0x80, frame_->timestamp()));
    }
    // Keep |frame_| alive until |video_frame| is destroyed.
    video_frame->AddDestructionObserver(
        ConvertToBaseOnceCallback(CrossThreadBindOnce(
            base::DoNothing::Once<scoped_refptr<media::VideoFrame>>(),
            frame_)));

    IsValidFrame(*video_frame);
    return MakeFrameAdapter(std::move(video_frame));
  } else if (frame_->HasTextures()) {
    // We cant convert texture synchronously due to threading issues, see
    // https://crbug.com/663452. Instead, return a black frame (yuv = {0, 0x80,
    // 0x80}).
    DLOG(ERROR) << "Texture backed frame cannot be accessed.";
    return MakeFrameAdapter(media::VideoFrame::CreateColorFrame(
        frame_->natural_size(), 0u, 0x80, 0x80, frame_->timestamp()));
  }
  IsValidFrame(*frame_);

  // Since scaling is required, hard-apply both the cropping and scaling
  // before we hand the frame over to WebRTC.
  const bool has_alpha = frame_->format() == media::PIXEL_FORMAT_I420A;

  gfx::Size scaled_size = frame_->natural_size();
  scoped_refptr<media::VideoFrame> scaled_frame = frame_;
  if (scaled_size != frame_->visible_rect().size()) {
    CHECK(scaled_frame_pool_);
    scaled_frame = scaled_frame_pool_->CreateFrame(
        has_alpha ? media::PIXEL_FORMAT_I420A : media::PIXEL_FORMAT_I420,
        scaled_size, gfx::Rect(scaled_size), scaled_size, frame_->timestamp());
    libyuv::I420Scale(
        frame_->visible_data(media::VideoFrame::kYPlane),
        frame_->stride(media::VideoFrame::kYPlane),
        frame_->visible_data(media::VideoFrame::kUPlane),
        frame_->stride(media::VideoFrame::kUPlane),
        frame_->visible_data(media::VideoFrame::kVPlane),
        frame_->stride(media::VideoFrame::kVPlane),
        frame_->visible_rect().width(), frame_->visible_rect().height(),
        scaled_frame->data(media::VideoFrame::kYPlane),
        scaled_frame->stride(media::VideoFrame::kYPlane),
        scaled_frame->data(media::VideoFrame::kUPlane),
        scaled_frame->stride(media::VideoFrame::kUPlane),
        scaled_frame->data(media::VideoFrame::kVPlane),
        scaled_frame->stride(media::VideoFrame::kVPlane), scaled_size.width(),
        scaled_size.height(), libyuv::kFilterBilinear);
    if (has_alpha) {
      libyuv::ScalePlane(
          frame_->visible_data(media::VideoFrame::kAPlane),
          frame_->stride(media::VideoFrame::kAPlane),
          frame_->visible_rect().width(), frame_->visible_rect().height(),
          scaled_frame->data(media::VideoFrame::kAPlane),
          scaled_frame->stride(media::VideoFrame::kAPlane), scaled_size.width(),
          scaled_size.height(), libyuv::kFilterBilinear);
    }
  }
  return MakeFrameAdapter(std::move(scaled_frame));
}

rtc::scoped_refptr<webrtc::I420BufferInterface>
WebRtcVideoFrameAdapter::ToI420() {
  base::AutoLock auto_lock(adapter_lock_);
  if (!frame_adapter_) {
    frame_adapter_ = CreateFrameAdapter();
  }
  return frame_adapter_->ToI420();
}

rtc::scoped_refptr<webrtc::VideoFrameBuffer>
WebRtcVideoFrameAdapter::GetMappedFrameBuffer(
    rtc::ArrayView<webrtc::VideoFrameBuffer::Type> types) {
  base::AutoLock auto_lock(adapter_lock_);
  if (!frame_adapter_) {
    frame_adapter_ = CreateFrameAdapter();
  }
  if (base::Contains(types, frame_adapter_->type())) {
    return frame_adapter_;
  }
  return nullptr;
}

const webrtc::I420BufferInterface* WebRtcVideoFrameAdapter::GetI420() const {
  base::AutoLock auto_lock(adapter_lock_);
  if (!frame_adapter_) {
    frame_adapter_ = CreateFrameAdapter();
  }
  if (frame_adapter_->type() == webrtc::VideoFrameBuffer::Type::kI420) {
    return frame_adapter_->GetI420();
  }
  if (frame_adapter_->type() == webrtc::VideoFrameBuffer::Type::kI420A) {
    return frame_adapter_->GetI420A();
  }
  return nullptr;
}

}  // namespace blink
