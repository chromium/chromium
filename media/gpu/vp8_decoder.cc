// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vp8_decoder.h"

#include "base/logging.h"
#include "base/notreached.h"
#include "media/base/limits.h"
#include "ui/gfx/hdr_metadata.h"

namespace media {

VP8Decoder::VP8Accelerator::VP8Accelerator() {}

VP8Decoder::VP8Accelerator::~VP8Accelerator() {}

VP8Decoder::VP8Decoder(std::unique_ptr<VP8Accelerator> accelerator,
                       const VideoColorSpace& container_color_space)
    : state_(kNeedStreamMetadata),
      curr_frame_start_(nullptr),
      frame_size_(0),
      accelerator_(std::move(accelerator)),
      container_color_space_(container_color_space) {
  DCHECK(accelerator_);
}

VP8Decoder::~VP8Decoder() = default;

bool VP8Decoder::Flush() {
  DVLOG(2) << "Decoder flush";
  Reset();
  return true;
}

void VP8Decoder::SetStream(int32_t id, const DecoderBuffer& decoder_buffer) {
  const uint8_t* ptr = decoder_buffer.data();
  const size_t size = decoder_buffer.size();
  const DecryptConfig* decrypt_config = decoder_buffer.decrypt_config();

  DCHECK(ptr);
  DCHECK(size);
  if (decrypt_config) {
    NOTIMPLEMENTED();
    state_ = kError;
    return;
  }

  DVLOG(4) << "New input stream id: " << id << " at: " << (void*)ptr
           << " size: " << size;
  stream_id_ = id;
  curr_frame_start_ = ptr;
  frame_size_ = size;
}

void VP8Decoder::Reset() {
  curr_frame_hdr_ = nullptr;
  curr_frame_start_ = nullptr;
  frame_size_ = 0;

  ref_frames_.Clear();

  if (state_ == kDecoding)
    state_ = kAfterReset;
}

VP8Decoder::DecodeResult VP8Decoder::Decode() {
  if (!curr_frame_start_ || frame_size_ == 0)
    return kRanOutOfStreamData;

  if (!curr_frame_hdr_) {
    curr_frame_hdr_.reset(new Vp8FrameHeader());
    if (!parser_.ParseFrame(curr_frame_start_, frame_size_,
                            curr_frame_hdr_.get())) {
      DVLOG(1) << "Error during decode";
      state_ = kError;
      return kDecodeError;
    }
  }

  // The |stream_id_|s are expected to be monotonically increasing, and we've
  // lost (at least) a frame if this condition doesn't uphold.
  const bool have_skipped_frame = last_decoded_stream_id_ + 1 != stream_id_ &&
                                  last_decoded_stream_id_ != kInvalidId;
  if (curr_frame_hdr_->IsKeyframe()) {
    const gfx::Size new_picture_size(curr_frame_hdr_->width,
                                     curr_frame_hdr_->height);
    if (new_picture_size.IsEmpty())
      return kDecodeError;

    if (new_picture_size != pic_size_) {
      DVLOG(2) << "New resolution: " << new_picture_size.ToString();
      pic_size_ = new_picture_size;

      ref_frames_.Clear();
      last_decoded_stream_id_ = stream_id_;
      size_change_failure_counter_ = 0;

      return kConfigChange;
    }

    state_ = kDecoding;
  } else if (state_ != kDecoding || have_skipped_frame) {
    // Only trust the next frame. Otherwise, new keyframe might be missed, so
    // |pic_size_| might be stale.
    // TODO(dshwang): if rtc decoder can know the size of inter frame, change
    // this condition to check if new keyframe is missed.
    // https://crbug.com/832545
    DVLOG(4) << "Drop the frame because the size maybe stale.";
    if (have_skipped_frame &&
        ++size_change_failure_counter_ > kVPxMaxNumOfSizeChangeFailures) {
      state_ = kError;
      return kDecodeError;
    }

    // Need a resume point.
    curr_frame_hdr_ = nullptr;
    return kRanOutOfStreamData;
  }

  scoped_refptr<VP8Picture> pic = accelerator_->CreateVP8Picture();
  if (!pic)
    return kRanOutOfSurfaces;

  if (!DecodeAndOutputCurrentFrame(std::move(pic))) {
    state_ = kError;
    return kDecodeError;
  }

  last_decoded_stream_id_ = stream_id_;
  size_change_failure_counter_ = 0;
  return kRanOutOfStreamData;
}

bool VP8Decoder::DecodeAndOutputCurrentFrame(scoped_refptr<VP8Picture> pic) {
  DCHECK(pic);
  DCHECK(!pic_size_.IsEmpty());
  DCHECK(curr_frame_hdr_);

  pic->set_visible_rect(gfx::Rect(pic_size_));
  pic->set_bitstream_id(stream_id_);
  if (container_color_space_.IsSpecified())
    pic->set_colorspace(container_color_space_);
  else
    pic->set_colorspace(VideoColorSpace::REC601());

  if (curr_frame_hdr_->IsKeyframe()) {
    horizontal_scale_ = curr_frame_hdr_->horizontal_scale;
    vertical_scale_ = curr_frame_hdr_->vertical_scale;
  } else {
    // Populate fields from decoder state instead.
    curr_frame_hdr_->width = pic_size_.width();
    curr_frame_hdr_->height = pic_size_.height();
    curr_frame_hdr_->horizontal_scale = horizontal_scale_;
    curr_frame_hdr_->vertical_scale = vertical_scale_;
  }

  const bool show_frame = curr_frame_hdr_->show_frame;
  pic->frame_hdr = std::move(curr_frame_hdr_);

  if (!accelerator_->SubmitDecode(pic, ref_frames_))
    return false;

  if (show_frame && !accelerator_->OutputPicture(pic))
    return false;

  ref_frames_.Refresh(pic);

  curr_frame_start_ = nullptr;
  frame_size_ = 0;
  return true;
}

gfx::Size VP8Decoder::GetPicSize() const {
  return pic_size_;
}

gfx::Rect VP8Decoder::GetVisibleRect() const {
  return gfx::Rect(pic_size_);
}

VideoCodecProfile VP8Decoder::GetProfile() const {
  return VP8PROFILE_ANY;
}

uint8_t VP8Decoder::GetBitDepth() const {
  return 8u;
}

VideoChromaSampling VP8Decoder::GetChromaSampling() const {
  // VP8 decoder currently does not rely on chroma sampling format for
  // creating/reconfiguring decoder, so return an unknown format.
  return VideoChromaSampling::kUnknown;
}

VideoColorSpace VP8Decoder::GetVideoColorSpace() const {
  // VP8 decoder currently does not store color space information and trigger
  // changes for color space.
  return VideoColorSpace();
}

std::optional<gfx::HDRMetadata> VP8Decoder::GetHDRMetadata() const {
  // VP8 doesn't support HDR metadata.
  return std::nullopt;
}

size_t VP8Decoder::GetRequiredNumOfPictures() const {
  constexpr size_t kPicsInPipeline = limits::kMaxVideoFrames + 1;
  return kNumVp8ReferenceBuffers + kPicsInPipeline;
}

size_t VP8Decoder::GetNumReferenceFrames() const {
  // Maximum number of reference frames.
  return kNumVp8ReferenceBuffers;
}

}  // namespace media
