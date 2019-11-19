// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vp9_decoder.h"

#include <memory>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "media/base/limits.h"
#include "media/base/media_switches.h"
#include "media/gpu/vp9_decoder.h"

namespace media {

namespace {
std::vector<uint32_t> GetSpatialLayerFrameSize(
    const DecoderBuffer& decoder_buffer) {
#if defined(ARCH_CPU_X86_FAMILY) && defined(OS_CHROMEOS)
  const uint32_t* cue_data =
      reinterpret_cast<const uint32_t*>(decoder_buffer.side_data());
  if (!cue_data)
    return {};
  if (!base::FeatureList::IsEnabled(media::kVp9kSVCHWDecoding)) {
    DLOG(ERROR) << "Vp9Parser doesn't support parsing SVC stream";
    return {};
  }

  size_t num_of_layers = decoder_buffer.side_data_size() / sizeof(uint32_t);
  if (num_of_layers > 3u) {
    DLOG(WARNING) << "The maximum number of spatial layers in VP9 is three";
    return {};
  }
  return std::vector<uint32_t>(cue_data, cue_data + num_of_layers);
#endif  // defined(ARCH_CPU_X86_FAMILY) && defined(OS_CHROMEOS)
  return {};
}
}  // namespace

VP9Decoder::VP9Accelerator::VP9Accelerator() {}

VP9Decoder::VP9Accelerator::~VP9Accelerator() {}

VP9Decoder::VP9Decoder(std::unique_ptr<VP9Accelerator> accelerator,
                       const VideoColorSpace& container_color_space)
    : state_(kNeedStreamMetadata),
      container_color_space_(container_color_space),
      accelerator_(std::move(accelerator)),
      parser_(accelerator_->IsFrameContextRequired()) {
}

VP9Decoder::~VP9Decoder() = default;

void VP9Decoder::SetStream(int32_t id, const DecoderBuffer& decoder_buffer) {
  const uint8_t* ptr = decoder_buffer.data();
  const size_t size = decoder_buffer.data_size();
  const DecryptConfig* decrypt_config = decoder_buffer.decrypt_config();

  DCHECK(ptr);
  DCHECK(size);
  DVLOG(4) << "New input stream id: " << id << " at: " << (void*)ptr
           << " size: " << size;
  stream_id_ = id;
  if (decrypt_config) {
    parser_.SetStream(ptr, size, GetSpatialLayerFrameSize(decoder_buffer),
                      decrypt_config->Clone());
  } else {
    parser_.SetStream(ptr, size, GetSpatialLayerFrameSize(decoder_buffer),
                      nullptr);
  }
}

bool VP9Decoder::Flush() {
  DVLOG(2) << "Decoder flush";
  Reset();
  return true;
}

void VP9Decoder::Reset() {
  curr_frame_hdr_ = nullptr;

  ref_frames_.Clear();

  parser_.Reset();

  if (state_ == kDecoding)
    state_ = kAfterReset;
}

VP9Decoder::DecodeResult VP9Decoder::Decode() {
  while (1) {
    // Read a new frame header if one is not awaiting decoding already.
    std::unique_ptr<DecryptConfig> decrypt_config;
    if (!curr_frame_hdr_) {
      gfx::Size allocate_size;
      std::unique_ptr<Vp9FrameHeader> hdr(new Vp9FrameHeader());
      Vp9Parser::Result res =
          parser_.ParseNextFrame(hdr.get(), &allocate_size, &decrypt_config);
      switch (res) {
        case Vp9Parser::kOk:
          curr_frame_hdr_ = std::move(hdr);
          curr_frame_size_ = allocate_size;
          break;

        case Vp9Parser::kEOStream:
          return kRanOutOfStreamData;

        case Vp9Parser::kInvalidStream:
          DVLOG(1) << "Error parsing stream";
          SetError();
          return kDecodeError;

        case Vp9Parser::kAwaitingRefresh:
          DVLOG(4) << "Awaiting context update";
          return kNeedContextUpdate;
      }
    }

    if (state_ != kDecoding) {
      // Not kDecoding, so we need a resume point (a keyframe), as we are after
      // reset or at the beginning of the stream. Drop anything that is not
      // a keyframe in such case, and continue looking for a keyframe.
      // Only exception is when the stream/sequence starts with an Intra only
      // frame.
      if (curr_frame_hdr_->IsKeyframe() ||
          (curr_frame_hdr_->IsIntra() && pic_size_.IsEmpty())) {
        state_ = kDecoding;
      } else {
        curr_frame_hdr_.reset();
        continue;
      }
    }

    if (curr_frame_hdr_->show_existing_frame) {
      // This frame header only instructs us to display one of the
      // previously-decoded frames, but has no frame data otherwise. Display
      // and continue decoding subsequent frames.
      size_t frame_to_show = curr_frame_hdr_->frame_to_show_map_idx;
      if (frame_to_show >= kVp9NumRefFrames ||
          !ref_frames_.GetFrame(frame_to_show)) {
        DVLOG(1) << "Request to show an invalid frame";
        SetError();
        return kDecodeError;
      }

      // Duplicate the VP9Picture and set the current bitstream id to keep the
      // correct timestamp.
      scoped_refptr<VP9Picture> pic =
          ref_frames_.GetFrame(frame_to_show)->Duplicate();
      if (pic == nullptr) {
        DVLOG(1) << "Failed to duplicate the VP9Picture.";
        SetError();
        return kDecodeError;
      }
      pic->set_bitstream_id(stream_id_);
      if (!accelerator_->OutputPicture(std::move(pic))) {
        SetError();
        return kDecodeError;
      }

      curr_frame_hdr_.reset();
      continue;
    }

    gfx::Size new_pic_size = curr_frame_size_;
    gfx::Rect new_render_rect(curr_frame_hdr_->render_width,
                              curr_frame_hdr_->render_height);
    // For safety, check the validity of render size or leave it as (0, 0).
    if (!gfx::Rect(new_pic_size).Contains(new_render_rect)) {
      DVLOG(1) << "Render size exceeds picture size. render size: "
               << new_render_rect.ToString()
               << ", picture size: " << new_pic_size.ToString();
      new_render_rect = gfx::Rect();
    }

    DCHECK(!new_pic_size.IsEmpty());
    if (new_pic_size != pic_size_) {
      DVLOG(1) << "New resolution: " << new_pic_size.ToString();

      if (!curr_frame_hdr_->IsKeyframe() &&
          (curr_frame_hdr_->IsIntra() && pic_size_.IsEmpty())) {
        // TODO(posciak): This is doable, but requires a few modifications to
        // VDA implementations to allow multiple picture buffer sets in flight.
        // http://crbug.com/832264
        DVLOG(1) << "Resolution change currently supported for keyframes and "
                    "sequence begins with Intra only when there is no prior "
                    "frames in the context";
        if (++size_change_failure_counter_ > kVPxMaxNumOfSizeChangeFailures) {
          SetError();
          return kDecodeError;
        }

        curr_frame_hdr_.reset();
        return kRanOutOfStreamData;
      }

      // TODO(posciak): This requires us to be on a keyframe (see above) and is
      // required, because VDA clients expect all surfaces to be returned before
      // they can cycle surface sets after receiving kAllocateNewSurfaces.
      // This is only an implementation detail of VDAs and can be improved.
      ref_frames_.Clear();

      pic_size_ = new_pic_size;
      visible_rect_ = new_render_rect;
      size_change_failure_counter_ = 0;
      return kAllocateNewSurfaces;
    }

    scoped_refptr<VP9Picture> pic = accelerator_->CreateVP9Picture();
    if (!pic)
      return kRanOutOfSurfaces;
    DVLOG(2) << "Render resolution: " << new_render_rect.ToString();

    pic->set_visible_rect(new_render_rect);
    pic->set_bitstream_id(stream_id_);

    pic->set_decrypt_config(std::move(decrypt_config));

    // For VP9, container color spaces override video stream color spaces.
    if (container_color_space_.IsSpecified()) {
      pic->set_colorspace(container_color_space_);
    } else if (curr_frame_hdr_) {
      pic->set_colorspace(curr_frame_hdr_->GetColorSpace());
    }
    pic->frame_hdr = std::move(curr_frame_hdr_);

    if (!DecodeAndOutputPicture(pic)) {
      SetError();
      return kDecodeError;
    }
  }
}

void VP9Decoder::UpdateFrameContext(
    scoped_refptr<VP9Picture> pic,
    const base::Callback<void(const Vp9FrameContext&)>& context_refresh_cb) {
  DCHECK(context_refresh_cb);
  Vp9FrameContext frame_ctx;
  memset(&frame_ctx, 0, sizeof(frame_ctx));

  if (!accelerator_->GetFrameContext(std::move(pic), &frame_ctx)) {
    SetError();
    return;
  }

  context_refresh_cb.Run(frame_ctx);
}

bool VP9Decoder::DecodeAndOutputPicture(scoped_refptr<VP9Picture> pic) {
  DCHECK(!pic_size_.IsEmpty());
  DCHECK(pic->frame_hdr);

  base::Closure done_cb;
  const auto& context_refresh_cb =
      parser_.GetContextRefreshCb(pic->frame_hdr->frame_context_idx);
  if (context_refresh_cb)
    done_cb = base::Bind(&VP9Decoder::UpdateFrameContext,
                         base::Unretained(this), pic, context_refresh_cb);

  const Vp9Parser::Context& context = parser_.context();
  if (!accelerator_->SubmitDecode(pic, context.segmentation(),
                                  context.loop_filter(), ref_frames_, done_cb))
    return false;

  if (pic->frame_hdr->show_frame) {
    if (!accelerator_->OutputPicture(pic))
      return false;
  }

  ref_frames_.Refresh(std::move(pic));
  return true;
}

void VP9Decoder::SetError() {
  Reset();
  state_ = kError;
}

gfx::Size VP9Decoder::GetPicSize() const {
  return pic_size_;
}

gfx::Rect VP9Decoder::GetVisibleRect() const {
  return visible_rect_;
}

size_t VP9Decoder::GetRequiredNumOfPictures() const {
  constexpr size_t kPicsInPipeline = limits::kMaxVideoFrames + 1;
  return kPicsInPipeline + GetNumReferenceFrames();
}

size_t VP9Decoder::GetNumReferenceFrames() const {
  // Maximum number of reference frames
  return kVp9NumRefFrames;
}

}  // namespace media
