// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/peerconnection/webrtc_video_track_source.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/trace_event/trace_event.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/platform/webrtc/webrtc_video_frame_adapter.h"
#include "third_party/libyuv/include/libyuv/scale.h"
#include "third_party/webrtc/rtc_base/ref_counted_object.h"

namespace {

gfx::Rect CropRectangle(const gfx::Rect& input_rect,
                        const gfx::Rect& cropping_rect) {
  gfx::Rect result(input_rect);
  result.Intersect(cropping_rect);
  result.Offset(-cropping_rect.x(), -cropping_rect.y());
  if (result.x() < 0)
    result.set_x(0);
  if (result.y() < 0)
    result.set_y(0);
  return result;
}

}  // anonymous namespace

namespace blink {

WebRtcVideoTrackSource::WebRtcVideoTrackSource(
    bool is_screencast,
    absl::optional<bool> needs_denoising)
    : AdaptedVideoTrackSource(/*required_alignment=*/1),
      is_screencast_(is_screencast),
      needs_denoising_(needs_denoising) {
  DETACH_FROM_THREAD(thread_checker_);
}

WebRtcVideoTrackSource::~WebRtcVideoTrackSource() = default;

void WebRtcVideoTrackSource::SetCustomFrameAdaptationParamsForTesting(
    const FrameAdaptationParams& params) {
  custom_frame_adaptation_params_for_testing_ = params;
}

WebRtcVideoTrackSource::SourceState WebRtcVideoTrackSource::state() const {
  // TODO(nisse): What's supposed to change this state?
  return MediaSourceInterface::SourceState::kLive;
}

bool WebRtcVideoTrackSource::remote() const {
  return false;
}

bool WebRtcVideoTrackSource::is_screencast() const {
  return is_screencast_;
}

absl::optional<bool> WebRtcVideoTrackSource::needs_denoising() const {
  return needs_denoising_;
}

void WebRtcVideoTrackSource::OnFrameCaptured(
    scoped_refptr<media::VideoFrame> frame) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  TRACE_EVENT0("media", "WebRtcVideoSource::OnFrameCaptured");
  if (!(frame->IsMappable() &&
        (frame->format() == media::PIXEL_FORMAT_I420 ||
         frame->format() == media::PIXEL_FORMAT_I420A)) &&
      !(frame->storage_type() ==
        media::VideoFrame::STORAGE_GPU_MEMORY_BUFFER) &&
      !frame->HasTextures()) {
    // Since connecting sources and sinks do not check the format, we need to
    // just ignore formats that we can not handle.
    LOG(ERROR) << "We cannot send frame with storage type: "
               << frame->AsHumanReadableString();
    NOTREACHED();
    return;
  }

  // Compute what rectangular region has changed since the last frame
  // that we successfully delivered to the base class method
  // rtc::AdaptedVideoTrackSource::OnFrame(). This region is going to be
  // relative to the coded frame data, i.e.
  // [0, 0, frame->coded_size().width(), frame->coded_size().height()].
  gfx::Rect update_rect;
  int capture_counter = 0;
  bool has_capture_counter = frame->metadata()->GetInteger(
      media::VideoFrameMetadata::CAPTURE_COUNTER, &capture_counter);
  bool has_update_rect = frame->metadata()->GetRect(
      media::VideoFrameMetadata::CAPTURE_UPDATE_RECT, &update_rect);
  const bool has_valid_update_rect =
      has_update_rect && has_capture_counter &&
      previous_capture_counter_.has_value() &&
      (capture_counter == (previous_capture_counter_.value() + 1));
  DVLOG(3) << "has_valid_update_rect = " << has_valid_update_rect;
  if (has_capture_counter)
    previous_capture_counter_ = capture_counter;
  if (has_valid_update_rect) {
    if (!accumulated_update_rect_) {
      accumulated_update_rect_ = update_rect;
    } else {
      accumulated_update_rect_->Union(update_rect);
    }
  } else {
    accumulated_update_rect_ = base::nullopt;
  }

  if (accumulated_update_rect_) {
    DVLOG(3) << "accumulated_update_rect_ = [" << accumulated_update_rect_->x()
             << ", " << accumulated_update_rect_->y() << ", "
             << accumulated_update_rect_->width() << ", "
             << accumulated_update_rect_->height() << "]";
  }

  // Calculate desired target cropping and scaling of the received frame. Note,
  // that the frame may already have some cropping and scaling soft-applied via
  // |frame->visible_rect()| and |frame->natural_size()|. The target cropping
  // and scaling computed by AdaptFrame() below is going to be applied on top
  // of the existing one.
  const int orig_width = frame->natural_size().width();
  const int orig_height = frame->natural_size().height();
  const int64_t now_us = rtc::TimeMicros();
  FrameAdaptationParams frame_adaptation_params =
      ComputeAdaptationParams(orig_width, orig_height, now_us);
  if (frame_adaptation_params.should_drop_frame)
    return;

  const int64_t translated_camera_time_us =
      timestamp_aligner_.TranslateTimestamp(frame->timestamp().InMicroseconds(),
                                            now_us);

  // Return |frame| directly if it is texture not backed up by GPU memory,
  // because there is no cropping support for texture yet. See
  // http://crbug/503653.
  if (frame->HasTextures() &&
      frame->storage_type() != media::VideoFrame::STORAGE_GPU_MEMORY_BUFFER) {
    // The webrtc::VideoFrame::UpdateRect expected by WebRTC must
    // be relative to the |visible_rect()|. We need to translate.
    base::Optional<gfx::Rect> cropped_rect;
    if (accumulated_update_rect_) {
      cropped_rect =
          CropRectangle(*accumulated_update_rect_, frame->visible_rect());
    }

    DeliverFrame(std::move(frame), OptionalOrNullptr(cropped_rect),
                 translated_camera_time_us);
    return;
  }

  // Translate the |crop_*| values output by AdaptFrame() from natural size to
  // visible size. This is needed to apply the new cropping on top of any
  // existing soft-applied cropping and scaling when using
  // media::VideoFrame::WrapVideoFrame().
  gfx::Rect cropped_visible_rect(
      frame->visible_rect().x() + frame_adaptation_params.crop_x *
                                      frame->visible_rect().width() /
                                      orig_width,
      frame->visible_rect().y() + frame_adaptation_params.crop_y *
                                      frame->visible_rect().height() /
                                      orig_height,
      frame_adaptation_params.crop_width * frame->visible_rect().width() /
          orig_width,
      frame_adaptation_params.crop_height * frame->visible_rect().height() /
          orig_height);

  DVLOG(3) << "cropped_visible_rect = "
           << "[" << cropped_visible_rect.x() << ", "
           << cropped_visible_rect.y() << ", " << cropped_visible_rect.width()
           << ", " << cropped_visible_rect.height() << "]";

  const gfx::Size adapted_size(frame_adaptation_params.scale_to_width,
                               frame_adaptation_params.scale_to_height);
  // Soft-apply the new (combined) cropping and scaling.
  scoped_refptr<media::VideoFrame> video_frame =
      media::VideoFrame::WrapVideoFrame(frame, frame->format(),
                                        cropped_visible_rect, adapted_size);
  if (!video_frame)
    return;

  // If no scaling is needed, return a wrapped version of |frame| directly.
  // The soft-applied cropping will be taken into account by the remainder
  // of the pipeline.
  if (video_frame->natural_size() == video_frame->visible_rect().size()) {
    // The webrtc::VideoFrame::UpdateRect expected by WebRTC must be
    // relative to the |visible_rect()|. We need to translate.
    base::Optional<gfx::Rect> cropped_rect;
    if (accumulated_update_rect_) {
      cropped_rect =
          CropRectangle(*accumulated_update_rect_, frame->visible_rect());
    }
    DeliverFrame(std::move(video_frame), OptionalOrNullptr(cropped_rect),
                 translated_camera_time_us);
    return;
  }

  // Delay scaling if |video_frame| is backed by GpuMemoryBuffer.
  if (video_frame->storage_type() ==
      media::VideoFrame::STORAGE_GPU_MEMORY_BUFFER) {
    // When scaling is applied and any part of the frame has changed, we don't
    // have reliable changed rect information.
    if (accumulated_update_rect_.has_value() &&
        !accumulated_update_rect_->IsEmpty()) {
      accumulated_update_rect_ = base::nullopt;
    }

    DeliverFrame(std::move(video_frame),
                 OptionalOrNullptr(accumulated_update_rect_),
                 translated_camera_time_us);
    return;
  }

  // Since scaling is required, hard-apply both the cropping and scaling before
  // we hand the frame over to WebRTC.
  const bool has_alpha = video_frame->format() == media::PIXEL_FORMAT_I420A;
  scoped_refptr<media::VideoFrame> scaled_frame =
      scaled_frame_pool_.CreateFrame(
          has_alpha ? media::PIXEL_FORMAT_I420A : media::PIXEL_FORMAT_I420,
          adapted_size, gfx::Rect(adapted_size), adapted_size,
          video_frame->timestamp());
  libyuv::I420Scale(
      video_frame->visible_data(media::VideoFrame::kYPlane),
      video_frame->stride(media::VideoFrame::kYPlane),
      video_frame->visible_data(media::VideoFrame::kUPlane),
      video_frame->stride(media::VideoFrame::kUPlane),
      video_frame->visible_data(media::VideoFrame::kVPlane),
      video_frame->stride(media::VideoFrame::kVPlane),
      video_frame->visible_rect().width(), video_frame->visible_rect().height(),
      scaled_frame->data(media::VideoFrame::kYPlane),
      scaled_frame->stride(media::VideoFrame::kYPlane),
      scaled_frame->data(media::VideoFrame::kUPlane),
      scaled_frame->stride(media::VideoFrame::kUPlane),
      scaled_frame->data(media::VideoFrame::kVPlane),
      scaled_frame->stride(media::VideoFrame::kVPlane), adapted_size.width(),
      adapted_size.height(), libyuv::kFilterBilinear);
  if (has_alpha) {
    libyuv::ScalePlane(video_frame->visible_data(media::VideoFrame::kAPlane),
                       video_frame->stride(media::VideoFrame::kAPlane),
                       video_frame->visible_rect().width(),
                       video_frame->visible_rect().height(),
                       scaled_frame->data(media::VideoFrame::kAPlane),
                       scaled_frame->stride(media::VideoFrame::kAPlane),
                       adapted_size.width(), adapted_size.height(),
                       libyuv::kFilterBilinear);
  }
  // When scaling is applied and any part of the frame has changed, we don't
  // have a reliable update rect information.
  if (accumulated_update_rect_.has_value() &&
      !accumulated_update_rect_->IsEmpty()) {
    accumulated_update_rect_ = base::nullopt;
  }
  DeliverFrame(std::move(scaled_frame),
               OptionalOrNullptr(accumulated_update_rect_),
               translated_camera_time_us);
}

WebRtcVideoTrackSource::FrameAdaptationParams
WebRtcVideoTrackSource::ComputeAdaptationParams(int width,
                                                int height,
                                                int64_t time_us) {
  if (custom_frame_adaptation_params_for_testing_.has_value())
    return custom_frame_adaptation_params_for_testing_.value();

  FrameAdaptationParams result{false, 0, 0, 0, 0, 0, 0};
  result.should_drop_frame = !AdaptFrame(
      width, height, time_us, &result.scale_to_width, &result.scale_to_height,
      &result.crop_width, &result.crop_height, &result.crop_x, &result.crop_y);
  return result;
}

void WebRtcVideoTrackSource::DeliverFrame(
    scoped_refptr<media::VideoFrame> frame,
    gfx::Rect* update_rect,
    int64_t timestamp_us) {
  if (update_rect) {
    DVLOG(3) << "update_rect = "
             << "[" << update_rect->x() << ", " << update_rect->y() << ", "
             << update_rect->width() << ", " << update_rect->height() << "]";
  }

  // If the cropping or the size have changed since the previous
  // frame, even if nothing in the incoming coded frame content has changed, we
  // have to assume that every pixel in the outgoing frame has changed.
  if (frame->visible_rect() != cropping_rect_of_previous_delivered_frame_ ||
      frame->natural_size() != natural_size_of_previous_delivered_frame_) {
    cropping_rect_of_previous_delivered_frame_ = frame->visible_rect();
    natural_size_of_previous_delivered_frame_ = frame->natural_size();
    update_rect = nullptr;
  }

  // Clear accumulated_update_rect_.
  accumulated_update_rect_ = gfx::Rect();

  webrtc::VideoFrame::Builder frame_builder =
      webrtc::VideoFrame::Builder()
          .set_video_frame_buffer(
              new rtc::RefCountedObject<WebRtcVideoFrameAdapter>(frame))
          .set_rotation(webrtc::kVideoRotation_0)
          .set_timestamp_us(timestamp_us);
  if (update_rect) {
    frame_builder.set_update_rect(webrtc::VideoFrame::UpdateRect{
        update_rect->x(), update_rect->y(), update_rect->width(),
        update_rect->height()});
  }
  OnFrame(frame_builder.build());
}

}  // namespace blink
