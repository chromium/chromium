// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/alpha_video_encoder_wrapper.h"

#include <algorithm>
#include <cmath>

#include "base/logging.h"
#include "base/numerics/checked_math.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "media/base/svc_scalability_mode.h"
#include "media/base/timestamp_constants.h"
#include "media/base/video_color_space.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "media/video/video_encoder_info.h"

namespace media {

AlphaVideoEncoderWrapper::AlphaVideoEncoderWrapper(
    std::unique_ptr<VideoEncoder> yuv_encoder,
    std::unique_ptr<VideoEncoder> alpha_encoder)
    : yuv_encoder_(std::move(yuv_encoder)),
      alpha_encoder_(std::move(alpha_encoder)) {
  CHECK(yuv_encoder_);
  CHECK(alpha_encoder_);
}

void AlphaVideoEncoderWrapper::Initialize(VideoCodecProfile profile,
                                          const Options& options,
                                          EncoderInfoCB info_cb,
                                          OutputCB output_cb,
                                          EncoderStatusCB done_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  done_cb = BindCallbackToCurrentLoopIfNeeded(std::move(done_cb));
  output_cb_ = BindCallbackToCurrentLoopIfNeeded(std::move(output_cb));

  if (options.scalability_mode.has_value()) {
    std::move(done_cb).Run(EncoderStatus::Codes::kEncoderUnsupportedConfig);
    return;
  }

  Options yuv_options = options;
  Options alpha_options = options;
  // Split bitrate 3 quarters to color planes and 1 quarter to alpha.
  if (options.bitrate.has_value()) {
    auto& bitrate = options.bitrate.value();
    switch (bitrate.mode()) {
      case Bitrate::Mode::kConstant:
        yuv_options.bitrate =
            Bitrate::ConstantBitrate(bitrate.target_bps() / 4 * 3);
        alpha_options.bitrate =
            Bitrate::ConstantBitrate(bitrate.target_bps() / 4);
        break;
      case Bitrate::Mode::kVariable:
        yuv_options.bitrate = Bitrate::VariableBitrate(
            bitrate.target_bps() / 4 * 3, bitrate.peak_bps() / 4 * 3);
        alpha_options.bitrate = Bitrate::VariableBitrate(
            bitrate.target_bps() / 4, bitrate.peak_bps() / 4);
        break;
      case Bitrate::Mode::kExternal:
        break;
    }
  }

  auto done_callback = [](base::WeakPtr<AlphaVideoEncoderWrapper> self,
                          EncoderStatus status) {
    if (!self) {
      NOTREACHED_IN_MIGRATION() << "Underlying encoder must be synchronous";
      return;
    }
    DCHECK_CALLED_ON_VALID_SEQUENCE(self->sequence_checker_);
    if (!self->init_status_.has_value() || self->init_status_->is_ok()) {
      self->init_status_ = std::move(status);
    }
  };

  yuv_encoder_->DisablePostedCallbacks();
  alpha_encoder_->DisablePostedCallbacks();
  init_status_.reset();

  yuv_encoder_->Initialize(
      profile, yuv_options, std::move(info_cb),
      base::BindRepeating(&AlphaVideoEncoderWrapper::YuvOutputCallback,
                          weak_factory_.GetWeakPtr()),
      base::BindOnce(done_callback, weak_factory_.GetWeakPtr()));

  alpha_encoder_->Initialize(
      profile, alpha_options, EncoderInfoCB(),
      base::BindRepeating(&AlphaVideoEncoderWrapper::AlphaOutputCallback,
                          weak_factory_.GetWeakPtr()),
      base::BindOnce(done_callback, weak_factory_.GetWeakPtr()));

  CHECK(init_status_.has_value());
  std::move(done_cb).Run(std::move(init_status_).value());
}

void AlphaVideoEncoderWrapper::Encode(scoped_refptr<VideoFrame> frame,
                                      const EncodeOptions& encode_options,
                                      EncoderStatusCB done_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  done_cb = BindCallbackToCurrentLoopIfNeeded(std::move(done_cb));
  if (frame->format() != PIXEL_FORMAT_I420A) {
    std::move(done_cb).Run(EncoderStatus::Codes::kUnsupportedFrameFormat);
    return;
  }

  const gfx::Size frame_size = frame->coded_size();
  auto dummy_plane_size =
      VideoFrame::PlaneSize(frame->format(), VideoFrame::Plane::kV, frame_size)
          .Area64();

  if (dummy_plane_size != dummy_uv_planes_.size()) {
    // It is more expensive to encode 0x00, so use 0x80 instead.
    dummy_uv_planes_.resize(dummy_plane_size, 0x80);
  }

  yuv_output_.reset();
  alpha_output_.reset();
  encode_status_.reset();
  auto uv_stride = VideoFrame::RowBytes(VideoFrame::Plane::kU, frame->format(),
                                        frame_size.width());

  auto yuv_frame = WrapAsI420VideoFrame(frame);
  auto alpha_frame = VideoFrame::WrapExternalYuvData(
      PIXEL_FORMAT_I420, frame->visible_rect().size(), frame->visible_rect(),
      frame->natural_size(), frame->stride(VideoFrame::Plane::kA), uv_stride,
      uv_stride, frame->visible_data(VideoFrame::Plane::kA),
      dummy_uv_planes_.data(), dummy_uv_planes_.data(), frame->timestamp());
  alpha_frame->metadata().MergeMetadataFrom(frame->metadata());

  auto done_callback = [](base::WeakPtr<AlphaVideoEncoderWrapper> self,
                          EncoderStatus status) {
    if (!self) {
      NOTREACHED_IN_MIGRATION() << "Underlying encoder must be synchronous";
      return;
    }
    DCHECK_CALLED_ON_VALID_SEQUENCE(self->sequence_checker_);
    if (!self->encode_status_.has_value() || self->encode_status_->is_ok()) {
      self->encode_status_ = std::move(status);
    }
  };

  yuv_encoder_->Encode(
      yuv_frame, encode_options,
      base::BindOnce(done_callback, weak_factory_.GetWeakPtr()));
  alpha_encoder_->Encode(
      alpha_frame, encode_options,
      base::BindOnce(done_callback, weak_factory_.GetWeakPtr()));

  if (!yuv_output_.has_value() || !alpha_output_.has_value()) {
    // This wrapper can only work with synchronous encoders that are completely
    // done encoding by the time Encode() completed.
    // So if we don't have the status and outputs it's time to give up.
    CHECK(encode_status_.has_value());
    std::move(done_cb).Run(*encode_status_);
    return;
  }

  if (encode_status_->is_ok()) {
    if (yuv_output_->key_frame && !alpha_output_->key_frame) {
      // Alpha keyframe must always go with YUV keyframe.
      std::move(done_cb).Run(EncoderStatus::Codes::kEncoderIllegalState);
      return;
    }

    VideoEncoderOutput output = std::move(yuv_output_).value();
    output.alpha_data = std::move(alpha_output_->data);
    output_cb_.Run(std::move(output), {});
  }

  std::move(done_cb).Run(std::move(encode_status_).value());
}

void AlphaVideoEncoderWrapper::ChangeOptions(const Options& options,
                                             OutputCB output_cb,
                                             EncoderStatusCB done_cb) {
  done_cb = BindCallbackToCurrentLoopIfNeeded(std::move(done_cb));
  NOTREACHED_IN_MIGRATION() << "Not implemented. Implement when needed.";
  std::move(done_cb).Run(EncoderStatus::Codes::kEncoderUnsupportedConfig);
}

AlphaVideoEncoderWrapper::~AlphaVideoEncoderWrapper() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void AlphaVideoEncoderWrapper::Flush(EncoderStatusCB done_cb) {
  done_cb = BindCallbackToCurrentLoopIfNeeded(std::move(done_cb));
  std::move(done_cb).Run(EncoderStatus::Codes::kOk);
}

void AlphaVideoEncoderWrapper::YuvOutputCallback(
    VideoEncoderOutput output,
    std::optional<CodecDescription> desc) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (desc.has_value()) {
    NOTREACHED_IN_MIGRATION()
        << "AlphaVideoEncoderWrapper doesn't support codecs with extra data";
    return;
  }
  yuv_output_.emplace(std::move(output));
}
void AlphaVideoEncoderWrapper::AlphaOutputCallback(
    VideoEncoderOutput output,
    std::optional<CodecDescription> desc) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (desc.has_value()) {
    NOTREACHED_IN_MIGRATION()
        << "AlphaVideoEncoderWrapper doesn't support codecs with extra data";
    return;
  }
  alpha_output_.emplace(std::move(output));
}

}  // namespace media
