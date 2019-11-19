// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/sender/size_adaptable_video_encoder_base.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "media/base/video_frame.h"

namespace media {
namespace cast {

SizeAdaptableVideoEncoderBase::SizeAdaptableVideoEncoderBase(
    const scoped_refptr<CastEnvironment>& cast_environment,
    const FrameSenderConfig& video_config,
    const StatusChangeCallback& status_change_cb)
    : cast_environment_(cast_environment),
      video_config_(video_config),
      status_change_cb_(status_change_cb),
      frames_in_encoder_(0),
      next_frame_id_(FrameId::first()) {
  cast_environment_->PostTask(
      CastEnvironment::MAIN,
      FROM_HERE,
      base::Bind(status_change_cb_, STATUS_INITIALIZED));
}

SizeAdaptableVideoEncoderBase::~SizeAdaptableVideoEncoderBase() {
  DestroyEncoder();
}

bool SizeAdaptableVideoEncoderBase::EncodeVideoFrame(
    scoped_refptr<media::VideoFrame> video_frame,
    const base::TimeTicks& reference_time,
    const FrameEncodedCallback& frame_encoded_callback) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));

  const gfx::Size frame_size = video_frame->visible_rect().size();
  if (frame_size.IsEmpty()) {
    DVLOG(1) << "Rejecting empty video frame.";
    return false;
  }
  if (frames_in_encoder_ == kEncoderIsInitializing) {
    VLOG(1) << "Dropping frame since encoder initialization is in-progress.";
    return false;
  }
  if (frame_size != frame_size_ || !encoder_) {
    VLOG(1) << "Dropping this frame, and future frames until a replacement "
               "encoder is spun-up to handle size " << frame_size.ToString();
    TrySpawningReplacementEncoder(frame_size);
    return false;
  }

  const bool is_frame_accepted = encoder_->EncodeVideoFrame(
      std::move(video_frame), reference_time,
      base::Bind(&SizeAdaptableVideoEncoderBase::OnEncodedVideoFrame,
                 weak_factory_.GetWeakPtr(), frame_encoded_callback));
  if (is_frame_accepted)
    ++frames_in_encoder_;
  return is_frame_accepted;
}

void SizeAdaptableVideoEncoderBase::SetBitRate(int new_bit_rate) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  video_config_.start_bitrate = new_bit_rate;
  if (encoder_)
    encoder_->SetBitRate(new_bit_rate);
}

void SizeAdaptableVideoEncoderBase::GenerateKeyFrame() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  if (encoder_)
    encoder_->GenerateKeyFrame();
}

std::unique_ptr<VideoFrameFactory>
SizeAdaptableVideoEncoderBase::CreateVideoFrameFactory() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  return nullptr;
}

void SizeAdaptableVideoEncoderBase::EmitFrames() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  if (encoder_)
    encoder_->EmitFrames();
}

StatusChangeCallback
    SizeAdaptableVideoEncoderBase::CreateEncoderStatusChangeCallback() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  return base::Bind(&SizeAdaptableVideoEncoderBase::OnEncoderStatusChange,
                    weak_factory_.GetWeakPtr());
}

void SizeAdaptableVideoEncoderBase::OnEncoderReplaced(
    VideoEncoder* replacement_encoder) {}

void SizeAdaptableVideoEncoderBase::DestroyEncoder() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  // The weak pointers are invalidated to prevent future calls back to |this|.
  // This effectively cancels any of |encoder_|'s posted tasks that have not yet
  // run.
  weak_factory_.InvalidateWeakPtrs();
  encoder_.reset();
}

void SizeAdaptableVideoEncoderBase::TrySpawningReplacementEncoder(
    const gfx::Size& size_needed) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));

  // If prior frames are still encoding in the current encoder, let them finish
  // first.
  if (frames_in_encoder_ > 0) {
    encoder_->EmitFrames();
    // Check again, since EmitFrames() is a synchronous operation for some
    // encoders.
    if (frames_in_encoder_ > 0)
      return;
  }

  if (frames_in_encoder_ == kEncoderIsInitializing)
    return;  // Already spawned.

  DestroyEncoder();
  frames_in_encoder_ = kEncoderIsInitializing;
  OnEncoderStatusChange(STATUS_CODEC_REINIT_PENDING);
  VLOG(1) << "Creating replacement video encoder (for frame size change from "
          << frame_size_.ToString() << " to "
          << size_needed.ToString() << ").";
  frame_size_ = size_needed;
  encoder_ = CreateEncoder();
  DCHECK(encoder_);
}

void SizeAdaptableVideoEncoderBase::OnEncoderStatusChange(
    OperationalStatus status) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  if (frames_in_encoder_ == kEncoderIsInitializing &&
      status == STATUS_INITIALIZED) {
    // Begin using the replacement encoder.
    frames_in_encoder_ = 0;
    OnEncoderReplaced(encoder_.get());
  }
  status_change_cb_.Run(status);
}

void SizeAdaptableVideoEncoderBase::OnEncodedVideoFrame(
    const FrameEncodedCallback& frame_encoded_callback,
    std::unique_ptr<SenderEncodedFrame> encoded_frame) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  --frames_in_encoder_;
  DCHECK_GE(frames_in_encoder_, 0);

  if (encoded_frame)
    next_frame_id_ = encoded_frame->frame_id + 1;
  frame_encoded_callback.Run(std::move(encoded_frame));
}

}  // namespace cast
}  // namespace media
