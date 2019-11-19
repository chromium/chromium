// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/aom_video_decoder.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_log.h"
#include "media/base/video_util.h"
#include "media/filters/frame_buffer_pool.h"
#include "third_party/libyuv/include/libyuv/convert.h"

// Include libaom header files.
extern "C" {
#include "third_party/libaom/source/libaom/aom/aom_decoder.h"
#include "third_party/libaom/source/libaom/aom/aom_frame_buffer.h"
#include "third_party/libaom/source/libaom/aom/aomdx.h"
}

namespace media {

// Returns the number of threads.
static int GetAomVideoDecoderThreadCount(const VideoDecoderConfig& config) {
  // For AOM decode when using the default thread count, increase the number
  // of decode threads to equal the maximum number of tiles possible for
  // higher resolution streams.
  return VideoDecoder::GetRecommendedThreadCount(config.coded_size().width() /
                                                 256);
}

static VideoPixelFormat AomImgFmtToVideoPixelFormat(const aom_image_t* img) {
  switch (img->fmt) {
    case AOM_IMG_FMT_I420:
      return PIXEL_FORMAT_I420;
    case AOM_IMG_FMT_I422:
      return PIXEL_FORMAT_I422;
    case AOM_IMG_FMT_I444:
      return PIXEL_FORMAT_I444;

    case AOM_IMG_FMT_I42016:
      switch (img->bit_depth) {
        case 10:
          return PIXEL_FORMAT_YUV420P10;
        case 12:
          return PIXEL_FORMAT_YUV420P12;
        default:
          DLOG(ERROR) << "Unsupported bit depth: " << img->bit_depth;
          return PIXEL_FORMAT_UNKNOWN;
      }

    case AOM_IMG_FMT_I42216:
      switch (img->bit_depth) {
        case 10:
          return PIXEL_FORMAT_YUV422P10;
        case 12:
          return PIXEL_FORMAT_YUV422P12;
        default:
          DLOG(ERROR) << "Unsupported bit depth: " << img->bit_depth;
          return PIXEL_FORMAT_UNKNOWN;
      }

    case AOM_IMG_FMT_I44416:
      switch (img->bit_depth) {
        case 10:
          return PIXEL_FORMAT_YUV444P10;
        case 12:
          return PIXEL_FORMAT_YUV444P12;
        default:
          DLOG(ERROR) << "Unsupported bit depth: " << img->bit_depth;
          return PIXEL_FORMAT_UNKNOWN;
      }

    default:
      DLOG(ERROR) << "Unsupported pixel format: " << img->fmt;
      return PIXEL_FORMAT_UNKNOWN;
  }
}

static void SetColorSpaceForFrame(const aom_image_t* img,
                                  const VideoDecoderConfig& config,
                                  VideoFrame* frame) {
  gfx::ColorSpace::RangeID range = img->range == AOM_CR_FULL_RANGE
                                       ? gfx::ColorSpace::RangeID::FULL
                                       : gfx::ColorSpace::RangeID::LIMITED;

  // AOM color space defines match ISO 23001-8:2016 via ISO/IEC 23091-4/ITU-T
  // H.273.
  // http://av1-spec.argondesign.com/av1-spec/av1-spec.html#color-config-semantics
  media::VideoColorSpace color_space(img->cp, img->tc, img->mc, range);

  // If the bitstream doesn't specify a color space, use the one from the
  // container.
  if (!color_space.IsSpecified())
    color_space = config.color_space_info();

  frame->set_color_space(color_space.ToGfxColorSpace());
}

static int GetFrameBuffer(void* cb_priv,
                          size_t min_size,
                          aom_codec_frame_buffer* fb) {
  DCHECK(cb_priv);
  DCHECK(fb);
  FrameBufferPool* pool = static_cast<FrameBufferPool*>(cb_priv);
  fb->data = pool->GetFrameBuffer(min_size, &fb->priv);
  fb->size = min_size;
  return 0;
}

static int ReleaseFrameBuffer(void* cb_priv, aom_codec_frame_buffer* fb) {
  DCHECK(cb_priv);
  DCHECK(fb);
  if (!fb->priv)
    return -1;

  FrameBufferPool* pool = static_cast<FrameBufferPool*>(cb_priv);
  pool->ReleaseFrameBuffer(fb->priv);
  return 0;
}

AomVideoDecoder::AomVideoDecoder(MediaLog* media_log) : media_log_(media_log) {
  DETACH_FROM_THREAD(thread_checker_);
}

AomVideoDecoder::~AomVideoDecoder() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  CloseDecoder();
}

std::string AomVideoDecoder::GetDisplayName() const {
  return "AomVideoDecoder";
}

void AomVideoDecoder::Initialize(const VideoDecoderConfig& config,
                                 bool /* low_delay */,
                                 CdmContext* /* cdm_context */,
                                 InitCB init_cb,
                                 const OutputCB& output_cb,
                                 const WaitingCB& /* waiting_cb */) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(config.IsValidConfig());

  InitCB bound_init_cb = BindToCurrentLoop(std::move(init_cb));
  if (config.is_encrypted() || config.codec() != kCodecAV1) {
    std::move(bound_init_cb).Run(false);
    return;
  }

  // Clear any previously initialized decoder.
  CloseDecoder();

  aom_codec_dec_cfg_t aom_config = {0};
  aom_config.w = config.coded_size().width();
  aom_config.h = config.coded_size().height();
  aom_config.threads = GetAomVideoDecoderThreadCount(config);

  // Misleading name. Required to ensure libaom doesn't output 8-bit samples
  // in uint16_t containers. Without this we have to manually pack the values
  // into uint8_t samples.
  aom_config.allow_lowbitdepth = 1;

  // TODO(dalecurtis, tguilbert): Move decoding off the media thread to the
  // offload thread via OffloadingVideoDecoder. https://crbug.com/867613

  std::unique_ptr<aom_codec_ctx> context = std::make_unique<aom_codec_ctx>();
  if (aom_codec_dec_init(context.get(), aom_codec_av1_dx(), &aom_config,
                         0 /* flags */) != AOM_CODEC_OK) {
    MEDIA_LOG(ERROR, media_log_) << "aom_codec_dec_init() failed: "
                                 << aom_codec_error(aom_decoder_.get());
    std::move(bound_init_cb).Run(false);
    return;
  }

  // Setup codec for zero copy frames.
  if (!memory_pool_)
    memory_pool_ = new FrameBufferPool();
  if (aom_codec_set_frame_buffer_functions(
          context.get(), &GetFrameBuffer, &ReleaseFrameBuffer,
          memory_pool_.get()) != AOM_CODEC_OK) {
    DLOG(ERROR) << "Failed to configure external buffers. "
                << aom_codec_error(context.get());
    std::move(bound_init_cb).Run(false);
    return;
  }

  config_ = config;
  state_ = DecoderState::kNormal;
  output_cb_ = BindToCurrentLoop(output_cb);
  aom_decoder_ = std::move(context);
  std::move(bound_init_cb).Run(true);
}

void AomVideoDecoder::Decode(scoped_refptr<DecoderBuffer> buffer,
                             DecodeCB decode_cb) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(buffer);
  DCHECK(decode_cb);
  DCHECK_NE(state_, DecoderState::kUninitialized)
      << "Called Decode() before successful Initialize()";

  DecodeCB bound_decode_cb = BindToCurrentLoop(std::move(decode_cb));

  if (state_ == DecoderState::kError) {
    std::move(bound_decode_cb).Run(DecodeStatus::DECODE_ERROR);
    return;
  }

  // No need to flush since we retrieve all available frames after a packet is
  // provided.
  if (buffer->end_of_stream()) {
    DCHECK_EQ(state_, DecoderState::kNormal);
    state_ = DecoderState::kDecodeFinished;
    std::move(bound_decode_cb).Run(DecodeStatus::OK);
    return;
  }

  if (!DecodeBuffer(buffer.get())) {
    state_ = DecoderState::kError;
    std::move(bound_decode_cb).Run(DecodeStatus::DECODE_ERROR);
    return;
  }

  // VideoDecoderShim expects |decode_cb| call after |output_cb_|.
  std::move(bound_decode_cb).Run(DecodeStatus::OK);
}

void AomVideoDecoder::Reset(const base::Closure& reset_cb) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  state_ = DecoderState::kNormal;
  timestamps_.clear();
  base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE, reset_cb);
}

void AomVideoDecoder::CloseDecoder() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!aom_decoder_)
    return;
  aom_codec_destroy(aom_decoder_.get());
  aom_decoder_.reset();

  if (memory_pool_) {
    memory_pool_->Shutdown();
    memory_pool_ = nullptr;
  }
}

bool AomVideoDecoder::DecodeBuffer(const DecoderBuffer* buffer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!buffer->end_of_stream());

  timestamps_.push_back(buffer->timestamp());
  if (aom_codec_decode(aom_decoder_.get(), buffer->data(), buffer->data_size(),
                       nullptr) != AOM_CODEC_OK) {
    const char* detail = aom_codec_error_detail(aom_decoder_.get());
    MEDIA_LOG(ERROR, media_log_)
        << "aom_codec_decode() failed: " << aom_codec_error(aom_decoder_.get())
        << (detail ? ", " : "") << (detail ? detail : "")
        << ", input: " << buffer->AsHumanReadableString();
    return false;
  }

  aom_codec_iter_t iter = nullptr;
  while (aom_image_t* img = aom_codec_get_frame(aom_decoder_.get(), &iter)) {
    auto frame = CopyImageToVideoFrame(img);
    if (!frame) {
      MEDIA_LOG(DEBUG, media_log_)
          << "Failed to produce video frame from aom_image_t.";
      return false;
    }

    // TODO(dalecurtis): Is this true even for low resolutions?
    frame->metadata()->SetBoolean(VideoFrameMetadata::POWER_EFFICIENT, false);

    // Ensure the frame memory is returned to the MemoryPool upon discard.
    frame->AddDestructionObserver(
        memory_pool_->CreateFrameCallback(img->fb_priv));

    SetColorSpaceForFrame(img, config_, frame.get());
    output_cb_.Run(std::move(frame));
  }

  return true;
}

scoped_refptr<VideoFrame> AomVideoDecoder::CopyImageToVideoFrame(
    const struct aom_image* img) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  VideoPixelFormat pixel_format = AomImgFmtToVideoPixelFormat(img);
  if (pixel_format == PIXEL_FORMAT_UNKNOWN)
    return nullptr;

  // Pull the expected timestamp from the front of the queue.
  DCHECK(!timestamps_.empty());
  const base::TimeDelta timestamp = timestamps_.front();
  timestamps_.pop_front();

  const gfx::Rect visible_rect(img->d_w, img->d_h);
  return VideoFrame::WrapExternalYuvData(
      pixel_format, visible_rect.size(), visible_rect,
      GetNaturalSize(visible_rect, config_.GetPixelAspectRatio()),
      img->stride[AOM_PLANE_Y], img->stride[AOM_PLANE_U],
      img->stride[AOM_PLANE_V], img->planes[AOM_PLANE_Y],
      img->planes[AOM_PLANE_U], img->planes[AOM_PLANE_V], timestamp);
}

}  // namespace media
