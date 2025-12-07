// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/webrtc/convert_to_webrtc_video_frame_buffer.h"

#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "media/base/video_frame_converter.h"
#include "media/base/video_util.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/modules/webrtc/webrtc_logging.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/webrtc/webrtc_video_frame_adapter.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/libyuv/include/libyuv/convert.h"
#include "third_party/libyuv/include/libyuv/convert_from_argb.h"
#include "third_party/libyuv/include/libyuv/scale.h"
#include "third_party/webrtc/api/video/i420_buffer.h"
#include "third_party/webrtc/common_video/include/video_frame_buffer.h"
#include "third_party/webrtc/rtc_base/ref_counted_object.h"

namespace blink {

// Template specializations of [1], needed to be able to pass WTF callbacks
// that have VideoTrackAdapterSettings or gfx::Size parameters across threads.
//
// [1] third_party/blink/renderer/platform/wtf/cross_thread_copier.h.

}  // namespace blink

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
    return frame_->visible_data(media::VideoFrame::Plane::kY);
  }

  const uint8_t* DataU() const override {
    return frame_->visible_data(media::VideoFrame::Plane::kU);
  }

  const uint8_t* DataV() const override {
    return frame_->visible_data(media::VideoFrame::Plane::kV);
  }

  int StrideY() const override {
    return frame_->stride(media::VideoFrame::Plane::kY);
  }

  int StrideU() const override {
    return frame_->stride(media::VideoFrame::Plane::kU);
  }

  int StrideV() const override {
    return frame_->stride(media::VideoFrame::Plane::kV);
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
    return frame_->visible_data(media::VideoFrame::Plane::kY);
  }

  const uint8_t* DataU() const override {
    return frame_->visible_data(media::VideoFrame::Plane::kU);
  }

  const uint8_t* DataV() const override {
    return frame_->visible_data(media::VideoFrame::Plane::kV);
  }

  const uint8_t* DataA() const override {
    return frame_->visible_data(media::VideoFrame::Plane::kA);
  }

  int StrideY() const override {
    return frame_->stride(media::VideoFrame::Plane::kY);
  }

  int StrideU() const override {
    return frame_->stride(media::VideoFrame::Plane::kU);
  }

  int StrideV() const override {
    return frame_->stride(media::VideoFrame::Plane::kV);
  }

  int StrideA() const override {
    return frame_->stride(media::VideoFrame::Plane::kA);
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
    return frame_->visible_data(media::VideoFrame::Plane::kY);
  }

  const uint8_t* DataUV() const override {
    return frame_->visible_data(media::VideoFrame::Plane::kUV);
  }

  int StrideY() const override {
    return frame_->stride(media::VideoFrame::Plane::kY);
  }

  int StrideUV() const override {
    return frame_->stride(media::VideoFrame::Plane::kUV);
  }

  webrtc::scoped_refptr<webrtc::I420BufferInterface> ToI420() override {
    webrtc::scoped_refptr<webrtc::I420Buffer> i420_buffer;
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

webrtc::scoped_refptr<webrtc::VideoFrameBuffer> MakeFrameAdapter(
    scoped_refptr<media::VideoFrame> video_frame) {
  switch (video_frame->format()) {
    case media::PIXEL_FORMAT_I420:
      return webrtc::scoped_refptr<webrtc::VideoFrameBuffer>(
          new webrtc::RefCountedObject<I420FrameAdapter>(
              std::move(video_frame)));
    case media::PIXEL_FORMAT_I420A:
      return webrtc::scoped_refptr<webrtc::VideoFrameBuffer>(
          new webrtc::RefCountedObject<I420AFrameAdapter>(
              std::move(video_frame)));
    case media::PIXEL_FORMAT_NV12:
      return webrtc::scoped_refptr<webrtc::VideoFrameBuffer>(
          new webrtc::RefCountedObject<NV12FrameAdapter>(
              std::move(video_frame)));
    default:
      NOTREACHED();
  }
}

scoped_refptr<media::VideoFrame> MakeScaledVideoFrame(
    scoped_refptr<media::VideoFrame> source_frame,
    scoped_refptr<blink::WebRtcVideoFrameAdapter::SharedResources>
        shared_resources,
    bool source_is_nv12) {
  media::VideoPixelFormat dst_format = media::PIXEL_FORMAT_UNKNOWN;
  if (source_is_nv12) {
    DCHECK_EQ(source_frame->format(), media::PIXEL_FORMAT_NV12);
    dst_format = media::PIXEL_FORMAT_NV12;
  } else {
    // ARGB pixel format may be produced by readback of texture backed frames.
    if (source_frame->format() != media::PIXEL_FORMAT_I420 &&
        source_frame->format() != media::PIXEL_FORMAT_I420A) {
      DCHECK(source_frame->format() == media::PIXEL_FORMAT_ARGB ||
             source_frame->format() == media::PIXEL_FORMAT_XRGB ||
             source_frame->format() == media::PIXEL_FORMAT_ABGR ||
             source_frame->format() == media::PIXEL_FORMAT_XBGR);
    }

    dst_format = source_frame->format() == media::PIXEL_FORMAT_I420A
                     ? media::PIXEL_FORMAT_I420A
                     : media::PIXEL_FORMAT_I420;
  }

  // Convert to dst format and scale to the natural size specified in
  // |source_frame|.
  auto dst_frame = shared_resources->CreateFrame(
      dst_format, source_frame->natural_size(),
      gfx::Rect(source_frame->natural_size()), source_frame->natural_size(),
      source_frame->timestamp());
  if (!dst_frame) {
    LOG(ERROR) << "Failed to create dst frame from pool.";
    return nullptr;
  }
  dst_frame->metadata().MergeMetadataFrom(source_frame->metadata());

  auto status = shared_resources->ConvertAndScale(*source_frame, *dst_frame);
  return status.is_ok() ? dst_frame : nullptr;
}

scoped_refptr<media::VideoFrame> MaybeConvertAndScaleFrame(
    scoped_refptr<media::VideoFrame> source_frame,
    scoped_refptr<blink::WebRtcVideoFrameAdapter::SharedResources>
        shared_resources) {
  if (!source_frame)
    return nullptr;
  // Texture frames may be readback in ARGB format.
  RTC_DCHECK(source_frame->format() == media::PIXEL_FORMAT_I420 ||
             source_frame->format() == media::PIXEL_FORMAT_I420A ||
             source_frame->format() == media::PIXEL_FORMAT_NV12 ||
             source_frame->format() == media::PIXEL_FORMAT_ARGB ||
             source_frame->format() == media::PIXEL_FORMAT_XRGB ||
             source_frame->format() == media::PIXEL_FORMAT_ABGR ||
             source_frame->format() == media::PIXEL_FORMAT_XBGR);
  RTC_DCHECK(shared_resources);

  const bool source_is_i420 =
      source_frame->format() == media::PIXEL_FORMAT_I420 ||
      source_frame->format() == media::PIXEL_FORMAT_I420A;
  const bool source_is_nv12 =
      source_frame->format() == media::PIXEL_FORMAT_NV12;
  const bool no_scaling_needed =
      source_frame->natural_size() == source_frame->visible_rect().size();

  if ((source_is_nv12 || source_is_i420) && no_scaling_needed) {
    // |source_frame| already has correct pixel format and resolution.
    return source_frame;
  }
  return MakeScaledVideoFrame(std::move(source_frame),
                              std::move(shared_resources), source_is_nv12);
}

}  // anonymous namespace

namespace blink {

// static
bool CanConvertToWebRtcVideoFrameBuffer(const media::VideoFrame* frame) {
  // Currently accept I420, I420A, NV12 formats in a mapped frame,
  // or a texture or GPU memory buffer frame.
  return (frame->IsMappable() &&
          base::Contains(GetPixelFormatsMappableToWebRtcVideoFrameBuffer(),
                         frame->format())) ||
         frame->storage_type() ==
             media::VideoFrame::STORAGE_MAPPABLE_SHARED_IMAGE ||
         frame->HasSharedImage();
}

// static
base::span<const media::VideoPixelFormat>
GetPixelFormatsMappableToWebRtcVideoFrameBuffer() {
  static constexpr media::VideoPixelFormat
      kGetPixelFormatsMappableToWebRtcVideoFrameBuffer[] = {
          media::PIXEL_FORMAT_I420, media::PIXEL_FORMAT_I420A,
          media::PIXEL_FORMAT_NV12, media::PIXEL_FORMAT_ARGB,
          media::PIXEL_FORMAT_XRGB, media::PIXEL_FORMAT_ABGR,
          media::PIXEL_FORMAT_XBGR};
  return base::span(kGetPixelFormatsMappableToWebRtcVideoFrameBuffer);
}

webrtc::scoped_refptr<webrtc::VideoFrameBuffer> ConvertToWebRtcVideoFrameBuffer(
    scoped_refptr<media::VideoFrame> video_frame,
    scoped_refptr<WebRtcVideoFrameAdapter::SharedResources> shared_resources) {
  DCHECK(CanConvertToWebRtcVideoFrameBuffer(video_frame.get()))
      << "Can not create WebRTC frame buffer for frame "
      << video_frame->AsHumanReadableString();

  auto create_placeholder_frame = [](const media::VideoFrame& frame) {
    LOG(ERROR)
        << "Mapping frame failed. Generating black frame instead. Frame: "
        << frame.AsHumanReadableString();
    return MakeFrameAdapter(media::VideoFrame::CreateColorFrame(
        frame.natural_size(), 0u, 0x80, 0x80, frame.timestamp()));
  };

  if (video_frame->storage_type() ==
      media::VideoFrame::StorageType::STORAGE_MAPPABLE_SHARED_IMAGE) {
    auto converted_frame =
        shared_resources
            ? shared_resources->ConstructVideoFrameFromGpu(video_frame)
            : nullptr;
    converted_frame =
        MaybeConvertAndScaleFrame(converted_frame, shared_resources);
    if (!converted_frame) {
      return create_placeholder_frame(*video_frame);
    }
    return MakeFrameAdapter(std::move(converted_frame));
  } else if (video_frame->HasSharedImage()) {
    auto converted_frame =
        shared_resources
            ? shared_resources->ConstructVideoFrameFromTexture(video_frame)
            : nullptr;
    converted_frame =
        MaybeConvertAndScaleFrame(converted_frame, shared_resources);
    if (!converted_frame) {
      DLOG(ERROR) << "Texture backed frame cannot be accessed.";
      return create_placeholder_frame(*video_frame);
    }
    return MakeFrameAdapter(std::move(converted_frame));
  }

  // Since scaling is required, hard-apply both the cropping and scaling
  // before we hand the frame over to WebRTC.
  auto scaled_frame = MaybeConvertAndScaleFrame(video_frame, shared_resources);
  if (!scaled_frame) {
    return create_placeholder_frame(*video_frame);
  }
  return MakeFrameAdapter(std::move(scaled_frame));
}

scoped_refptr<media::VideoFrame> ConvertFromMappedWebRtcVideoFrameBuffer(
    webrtc::scoped_refptr<webrtc::VideoFrameBuffer> buffer,
    base::TimeDelta timestamp) {
  RTC_DCHECK(buffer->type() != webrtc::VideoFrameBuffer::Type::kNative);
  const gfx::Size size(buffer->width(), buffer->height());
  scoped_refptr<media::VideoFrame> video_frame;
  switch (buffer->type()) {
    case webrtc::VideoFrameBuffer::Type::kI420A: {
      const webrtc::I420ABufferInterface* yuva_buffer = buffer->GetI420A();
      const media::VideoPixelFormat pixel_format = media::PIXEL_FORMAT_I420A;
      const size_t luma_rows = size.height();
      const size_t chroma_rows =
          media::VideoFrame::PlaneSizeInSamples(
              pixel_format, media::VideoFrame::Plane::kU, size)
              .height();
      // TODO(issues.webrtc.org/issues/42225170):
      // webrtc::I420ABufferInterface should expose ArrayView instead of raw
      // pointers.
      auto y_plane = UNSAFE_TODO(base::span<const uint8_t>(
          yuva_buffer->DataY(),
          base::saturated_cast<size_t>(yuva_buffer->StrideY()) * luma_rows));
      auto u_plane = UNSAFE_TODO(base::span<const uint8_t>(
          yuva_buffer->DataU(),
          base::saturated_cast<size_t>(yuva_buffer->StrideU()) * chroma_rows));
      auto v_plane = UNSAFE_TODO(base::span<const uint8_t>(
          yuva_buffer->DataV(),
          base::saturated_cast<size_t>(yuva_buffer->StrideV()) * chroma_rows));
      auto a_plane = UNSAFE_TODO(base::span<const uint8_t>(
          yuva_buffer->DataA(),
          base::saturated_cast<size_t>(yuva_buffer->StrideA()) * luma_rows));
      video_frame = media::VideoFrame::WrapExternalYuvaData(
          pixel_format, size, gfx::Rect(size), size, yuva_buffer->StrideY(),
          yuva_buffer->StrideU(), yuva_buffer->StrideV(),
          yuva_buffer->StrideA(), y_plane, u_plane, v_plane, a_plane,
          timestamp);
      break;
    }
    case webrtc::VideoFrameBuffer::Type::kI420: {
      const webrtc::I420BufferInterface* yuv_buffer = buffer->GetI420();
      const media::VideoPixelFormat pixel_format = media::PIXEL_FORMAT_I420;
      const size_t luma_rows = size.height();
      const size_t chroma_rows =
          media::VideoFrame::PlaneSizeInSamples(
              pixel_format, media::VideoFrame::Plane::kU, size)
              .height();
      // TODO(issues.webrtc.org/issues/42225170):
      // webrtc::I420BufferInterface should expose ArrayView instead of raw
      // pointers.
      auto y_plane = UNSAFE_TODO(base::span<const uint8_t>(
          yuv_buffer->DataY(),
          base::saturated_cast<size_t>(yuv_buffer->StrideY()) * luma_rows));
      auto u_plane = UNSAFE_TODO(base::span<const uint8_t>(
          yuv_buffer->DataU(),
          base::saturated_cast<size_t>(yuv_buffer->StrideU()) * chroma_rows));
      auto v_plane = UNSAFE_TODO(base::span<const uint8_t>(
          yuv_buffer->DataV(),
          base::saturated_cast<size_t>(yuv_buffer->StrideV()) * chroma_rows));

      video_frame = media::VideoFrame::WrapExternalYuvData(
          pixel_format, size, gfx::Rect(size), size, yuv_buffer->StrideY(),
          yuv_buffer->StrideU(), yuv_buffer->StrideV(), y_plane, u_plane,
          v_plane, timestamp);
      break;
    }
    case webrtc::VideoFrameBuffer::Type::kI444: {
      const webrtc::I444BufferInterface* yuv_buffer = buffer->GetI444();
      const media::VideoPixelFormat pixel_format = media::PIXEL_FORMAT_I444;
      const size_t luma_rows = size.height();
      const size_t chroma_rows =
          media::VideoFrame::PlaneSizeInSamples(
              pixel_format, media::VideoFrame::Plane::kU, size)
              .height();
      // TODO(crbug.com/338570700): webrtc::I444BufferInterface should expose
      // ArrayView instead of raw pointers.
      auto y_plane = UNSAFE_TODO(base::span<const uint8_t>(
          yuv_buffer->DataY(),
          base::saturated_cast<size_t>(yuv_buffer->StrideY()) * luma_rows));
      auto u_plane = UNSAFE_TODO(base::span<const uint8_t>(
          yuv_buffer->DataU(),
          base::saturated_cast<size_t>(yuv_buffer->StrideU()) * chroma_rows));
      auto v_plane = UNSAFE_TODO(base::span<const uint8_t>(
          yuv_buffer->DataV(),
          base::saturated_cast<size_t>(yuv_buffer->StrideV()) * chroma_rows));
      video_frame = media::VideoFrame::WrapExternalYuvData(
          pixel_format, size, gfx::Rect(size), size, yuv_buffer->StrideY(),
          yuv_buffer->StrideU(), yuv_buffer->StrideV(), y_plane, u_plane,
          v_plane, timestamp);
      break;
    }
    case webrtc::VideoFrameBuffer::Type::kI422: {
      const webrtc::I422BufferInterface* yuv_buffer = buffer->GetI422();
      const media::VideoPixelFormat pixel_format = media::PIXEL_FORMAT_I422;
      const size_t luma_rows = size.height();
      const size_t chroma_rows =
          media::VideoFrame::PlaneSizeInSamples(
              pixel_format, media::VideoFrame::Plane::kU, size)
              .height();
      // TODO(crbug.com/338570700): webrtc::I422BufferInterface should expose
      // ArrayView instead of raw pointers.
      auto y_plane = UNSAFE_TODO(base::span<const uint8_t>(
          yuv_buffer->DataY(),
          base::saturated_cast<size_t>(yuv_buffer->StrideY()) * luma_rows));
      auto u_plane = UNSAFE_TODO(base::span<const uint8_t>(
          yuv_buffer->DataU(),
          base::saturated_cast<size_t>(yuv_buffer->StrideU()) * chroma_rows));
      auto v_plane = UNSAFE_TODO(base::span<const uint8_t>(
          yuv_buffer->DataV(),
          base::saturated_cast<size_t>(yuv_buffer->StrideV()) * chroma_rows));
      video_frame = media::VideoFrame::WrapExternalYuvData(
          pixel_format, size, gfx::Rect(size), size, yuv_buffer->StrideY(),
          yuv_buffer->StrideU(), yuv_buffer->StrideV(), y_plane, u_plane,
          v_plane, timestamp);
      break;
    }
    case webrtc::VideoFrameBuffer::Type::kI010: {
      const webrtc::I010BufferInterface* yuv_buffer = buffer->GetI010();
      const media::VideoPixelFormat pixel_format =
          media::PIXEL_FORMAT_YUV420P10;
      const size_t luma_rows = size.height();
      const size_t chroma_rows =
          media::VideoFrame::PlaneSizeInSamples(
              pixel_format, media::VideoFrame::Plane::kU, size)
              .height();
      // WebRTC defines I010 data as uint16 whereas Chromium uses uint8 for all
      // video formats, so conversion and cast is needed.
      // TODO(crbug.com/338570700): webrtc::I010BufferInterface should expose
      // ArrayView instead of raw pointers.
      auto y_plane = UNSAFE_TODO(base::span<const uint8_t>(
          reinterpret_cast<const uint8_t*>(yuv_buffer->DataY()),
          base::saturated_cast<size_t>(yuv_buffer->StrideY()) * 2 * luma_rows));
      auto u_plane = UNSAFE_TODO(base::span<const uint8_t>(
          reinterpret_cast<const uint8_t*>(yuv_buffer->DataU()),
          base::saturated_cast<size_t>(yuv_buffer->StrideU()) * 2 *
              chroma_rows));
      auto v_plane = UNSAFE_TODO(base::span<const uint8_t>(
          reinterpret_cast<const uint8_t*>(yuv_buffer->DataV()),
          base::saturated_cast<size_t>(yuv_buffer->StrideV()) * 2 *
              chroma_rows));
      video_frame = media::VideoFrame::WrapExternalYuvData(
          pixel_format, size, gfx::Rect(size), size, yuv_buffer->StrideY() * 2,
          yuv_buffer->StrideU() * 2, yuv_buffer->StrideV() * 2, y_plane,
          u_plane, v_plane, timestamp);
      break;
    }
    case webrtc::VideoFrameBuffer::Type::kI210: {
      const webrtc::I210BufferInterface* yuv_buffer = buffer->GetI210();
      const media::VideoPixelFormat pixel_format =
          media::PIXEL_FORMAT_YUV422P10;
      const size_t luma_rows = size.height();
      const size_t chroma_rows =
          media::VideoFrame::PlaneSizeInSamples(
              pixel_format, media::VideoFrame::Plane::kU, size)
              .height();
      // WebRTC defines I210 data as uint16 whereas Chromium uses uint8 for all
      // video formats, so conversion and cast is needed.
      // TODO(crbug.com/338570700): webrtc::I210BufferInterface should expose
      // ArrayView instead of raw pointers.
      auto y_plane = UNSAFE_TODO(base::span<const uint8_t>(
          reinterpret_cast<const uint8_t*>(yuv_buffer->DataY()),
          base::saturated_cast<size_t>(yuv_buffer->StrideY()) * 2 * luma_rows));
      auto u_plane = UNSAFE_TODO(base::span<const uint8_t>(
          reinterpret_cast<const uint8_t*>(yuv_buffer->DataU()),
          base::saturated_cast<size_t>(yuv_buffer->StrideU()) * 2 *
              chroma_rows));
      auto v_plane = UNSAFE_TODO(base::span<const uint8_t>(
          reinterpret_cast<const uint8_t*>(yuv_buffer->DataV()),
          base::saturated_cast<size_t>(yuv_buffer->StrideV()) * 2 *
              chroma_rows));
      video_frame = media::VideoFrame::WrapExternalYuvData(
          pixel_format, size, gfx::Rect(size), size, yuv_buffer->StrideY() * 2,
          yuv_buffer->StrideU() * 2, yuv_buffer->StrideV() * 2, y_plane,
          u_plane, v_plane, timestamp);
      break;
    }
    case webrtc::VideoFrameBuffer::Type::kI410: {
      const webrtc::I410BufferInterface* yuv_buffer = buffer->GetI410();
      const media::VideoPixelFormat pixel_format =
          media::PIXEL_FORMAT_YUV444P10;
      const size_t luma_rows = size.height();
      const size_t chroma_rows =
          media::VideoFrame::PlaneSizeInSamples(
              pixel_format, media::VideoFrame::Plane::kU, size)
              .height();
      // WebRTC defines I410 data as uint16 whereas Chromium uses uint8 for all
      // video formats, so conversion and cast is needed.
      // TODO(crbug.com/338570700): webrtc::I410BufferInterface should expose
      // ArrayView instead of raw pointers.
      auto y_plane = UNSAFE_TODO(base::span<const uint8_t>(
          reinterpret_cast<const uint8_t*>(yuv_buffer->DataY()),
          base::saturated_cast<size_t>(yuv_buffer->StrideY()) * 2 * luma_rows));
      auto u_plane = UNSAFE_TODO(base::span<const uint8_t>(
          reinterpret_cast<const uint8_t*>(yuv_buffer->DataU()),
          base::saturated_cast<size_t>(yuv_buffer->StrideU()) * 2 *
              chroma_rows));
      auto v_plane = UNSAFE_TODO(base::span<const uint8_t>(
          reinterpret_cast<const uint8_t*>(yuv_buffer->DataV()),
          base::saturated_cast<size_t>(yuv_buffer->StrideV()) * 2 *
              chroma_rows));
      video_frame = media::VideoFrame::WrapExternalYuvData(
          pixel_format, size, gfx::Rect(size), size, yuv_buffer->StrideY() * 2,
          yuv_buffer->StrideU() * 2, yuv_buffer->StrideV() * 2, y_plane,
          u_plane, v_plane, timestamp);
      break;
    }
    case webrtc::VideoFrameBuffer::Type::kNV12: {
      const webrtc::NV12BufferInterface* nv12_buffer = buffer->GetNV12();
      const media::VideoPixelFormat pixel_format = media::PIXEL_FORMAT_NV12;
      const size_t luma_rows = size.height();
      const size_t chroma_rows =
          media::VideoFrame::PlaneSizeInSamples(
              pixel_format, media::VideoFrame::Plane::kUV, size)
              .height();
      // TODO(issues.webrtc.org/issues/42225170):
      // webrtc::I420BufferInterface should expose ArrayView instead of raw
      // pointers.
      auto y_plane = UNSAFE_TODO(base::span<const uint8_t>(
          nv12_buffer->DataY(),
          base::saturated_cast<size_t>(nv12_buffer->StrideY()) * luma_rows));
      auto uv_plane = UNSAFE_TODO(base::span<const uint8_t>(
          nv12_buffer->DataUV(),
          base::saturated_cast<size_t>(nv12_buffer->StrideUV()) * chroma_rows));

      video_frame = media::VideoFrame::WrapExternalYuvData(
          media::PIXEL_FORMAT_NV12, size, gfx::Rect(size), size,
          nv12_buffer->StrideY(), nv12_buffer->StrideUV(), y_plane, uv_plane,
          timestamp);
      break;
    }
    default:
      NOTREACHED();
  }
  if (!video_frame) {
    return nullptr;
  }
  // The bind ensures that we keep a reference to the underlying buffer.
  video_frame->AddDestructionObserver(
      ConvertToBaseOnceCallback(CrossThreadBindOnce(
          [](const scoped_refptr<webrtc::RefCountInterface>&) {},
          scoped_refptr<webrtc::VideoFrameBuffer>(buffer.get()))));
  return video_frame;
}

}  // namespace blink
