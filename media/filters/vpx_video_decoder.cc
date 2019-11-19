// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/vpx_video_decoder.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/sys_byteorder.h"
#include "base/trace_event/trace_event.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/decoder_buffer.h"
#include "media/base/limits.h"
#include "media/base/media_switches.h"
#include "media/base/video_util.h"
#include "media/filters/frame_buffer_pool.h"
#include "third_party/libvpx/source/libvpx/vpx/vp8dx.h"
#include "third_party/libvpx/source/libvpx/vpx/vpx_decoder.h"
#include "third_party/libvpx/source/libvpx/vpx/vpx_frame_buffer.h"

#include "third_party/libyuv/include/libyuv/convert.h"
#include "third_party/libyuv/include/libyuv/planar_functions.h"

namespace media {

// Returns the number of threads.
static int GetVpxVideoDecoderThreadCount(const VideoDecoderConfig& config) {
  // vp8a doesn't really need more threads.
  int desired_threads = limits::kMinVideoDecodeThreads;

  // For VP9 decoding increase the number of decode threads to equal the
  // maximum number of tiles possible for higher resolution streams.
  if (config.codec() == kCodecVP9) {
    const int width = config.coded_size().width();
    if (width >= 4096)
      desired_threads = 16;
    else if (width >= 2048)
      desired_threads = 8;
    else if (width >= 1024)
      desired_threads = 4;
  }

  return VideoDecoder::GetRecommendedThreadCount(desired_threads);
}

static std::unique_ptr<vpx_codec_ctx> InitializeVpxContext(
    const VideoDecoderConfig& config) {
  auto context = std::make_unique<vpx_codec_ctx>();
  vpx_codec_dec_cfg_t vpx_config = {0};
  vpx_config.w = config.coded_size().width();
  vpx_config.h = config.coded_size().height();
  vpx_config.threads = GetVpxVideoDecoderThreadCount(config);

  vpx_codec_err_t status = vpx_codec_dec_init(
      context.get(),
      config.codec() == kCodecVP9 ? vpx_codec_vp9_dx() : vpx_codec_vp8_dx(),
      &vpx_config, 0 /* flags */);
  if (status == VPX_CODEC_OK)
    return context;

  DLOG(ERROR) << "vpx_codec_dec_init() failed: "
              << vpx_codec_error(context.get());
  return nullptr;
}

static int32_t GetVP9FrameBuffer(void* user_priv,
                                 size_t min_size,
                                 vpx_codec_frame_buffer* fb) {
  DCHECK(user_priv);
  DCHECK(fb);
  FrameBufferPool* pool = static_cast<FrameBufferPool*>(user_priv);
  fb->data = pool->GetFrameBuffer(min_size, &fb->priv);
  fb->size = min_size;
  return 0;
}

static int32_t ReleaseVP9FrameBuffer(void* user_priv,
                                     vpx_codec_frame_buffer* fb) {
  DCHECK(user_priv);
  DCHECK(fb);
  if (!fb->priv)
    return -1;

  FrameBufferPool* pool = static_cast<FrameBufferPool*>(user_priv);
  pool->ReleaseFrameBuffer(fb->priv);
  return 0;
}

VpxVideoDecoder::VpxVideoDecoder(OffloadState offload_state)
    : bind_callbacks_(offload_state == OffloadState::kNormal) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

VpxVideoDecoder::~VpxVideoDecoder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CloseDecoder();
}

std::string VpxVideoDecoder::GetDisplayName() const {
  return "VpxVideoDecoder";
}

void VpxVideoDecoder::Initialize(const VideoDecoderConfig& config,
                                 bool /* low_delay */,
                                 CdmContext* /* cdm_context */,
                                 InitCB init_cb,
                                 const OutputCB& output_cb,
                                 const WaitingCB& /* waiting_cb */) {
  DVLOG(1) << __func__ << ": " << config.AsHumanReadableString();
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(config.IsValidConfig());

  CloseDecoder();

  InitCB bound_init_cb = bind_callbacks_ ? BindToCurrentLoop(std::move(init_cb))
                                         : std::move(init_cb);
  if (config.is_encrypted() || !ConfigureDecoder(config)) {
    std::move(bound_init_cb).Run(false);
    return;
  }

  // Success!
  config_ = config;
  state_ = kNormal;
  output_cb_ = output_cb;
  std::move(bound_init_cb).Run(true);
}

void VpxVideoDecoder::Decode(scoped_refptr<DecoderBuffer> buffer,
                             DecodeCB decode_cb) {
  DVLOG(3) << __func__ << ": " << buffer->AsHumanReadableString();
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(buffer);
  DCHECK(decode_cb);
  DCHECK_NE(state_, kUninitialized)
      << "Called Decode() before successful Initialize()";

  DecodeCB bound_decode_cb = bind_callbacks_
                                 ? BindToCurrentLoop(std::move(decode_cb))
                                 : std::move(decode_cb);

  if (state_ == kError) {
    std::move(bound_decode_cb).Run(DecodeStatus::DECODE_ERROR);
    return;
  }

  if (state_ == kDecodeFinished) {
    std::move(bound_decode_cb).Run(DecodeStatus::OK);
    return;
  }

  if (state_ == kNormal && buffer->end_of_stream()) {
    state_ = kDecodeFinished;
    std::move(bound_decode_cb).Run(DecodeStatus::OK);
    return;
  }

  scoped_refptr<VideoFrame> video_frame;
  if (!VpxDecode(buffer.get(), &video_frame)) {
    state_ = kError;
    std::move(bound_decode_cb).Run(DecodeStatus::DECODE_ERROR);
    return;
  }

  // We might get a successful VpxDecode but not a frame if only a partial
  // decode happened.
  if (video_frame) {
    video_frame->metadata()->SetBoolean(VideoFrameMetadata::POWER_EFFICIENT,
                                        false);
    output_cb_.Run(video_frame);
  }

  // VideoDecoderShim expects |decode_cb| call after |output_cb_|.
  std::move(bound_decode_cb).Run(DecodeStatus::OK);
}

void VpxVideoDecoder::Reset(base::OnceClosure reset_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  state_ = kNormal;

  if (bind_callbacks_)
    BindToCurrentLoop(std::move(reset_cb)).Run();
  else
    std::move(reset_cb).Run();

  // Allow Initialize() to be called on another thread now.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

bool VpxVideoDecoder::ConfigureDecoder(const VideoDecoderConfig& config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (config.codec() != kCodecVP8 && config.codec() != kCodecVP9)
    return false;

#if BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)
  // When enabled, ffmpeg handles VP8 that doesn't have alpha, and
  // VpxVideoDecoder will handle VP8 with alpha. FFvp8 is being deprecated.
  // See http://crbug.com/992235.
  if (base::FeatureList::IsEnabled(kFFmpegDecodeOpaqueVP8) &&
      config.codec() == kCodecVP8 &&
      config.alpha_mode() == VideoDecoderConfig::AlphaMode::kIsOpaque) {
    return false;
  }
#endif

  DCHECK(!vpx_codec_);
  vpx_codec_ = InitializeVpxContext(config);
  if (!vpx_codec_)
    return false;

  // Configure VP9 to decode on our buffers to skip a data copy on
  // decoding. For YV12A-VP9, we use our buffers for the Y, U and V planes and
  // copy the A plane.
  if (config.codec() == kCodecVP9) {
    DCHECK(vpx_codec_get_caps(vpx_codec_->iface) &
           VPX_CODEC_CAP_EXTERNAL_FRAME_BUFFER);

    DCHECK(!memory_pool_);
    memory_pool_ = new FrameBufferPool();

    if (vpx_codec_set_frame_buffer_functions(
            vpx_codec_.get(), &GetVP9FrameBuffer, &ReleaseVP9FrameBuffer,
            memory_pool_.get())) {
      DLOG(ERROR) << "Failed to configure external buffers. "
                  << vpx_codec_error(vpx_codec_.get());
      return false;
    }
  }

  if (config.alpha_mode() == VideoDecoderConfig::AlphaMode::kIsOpaque)
    return true;

  DCHECK(!vpx_codec_alpha_);
  vpx_codec_alpha_ = InitializeVpxContext(config);
  return !!vpx_codec_alpha_;
}

void VpxVideoDecoder::CloseDecoder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Note: The vpx_codec_destroy() calls below don't release the memory
  // allocated for vpx_codec_ctx, they just release internal allocations, so we
  // still need std::unique_ptr to release the structure memory.
  if (vpx_codec_)
    vpx_codec_destroy(vpx_codec_.get());

  if (vpx_codec_alpha_)
    vpx_codec_destroy(vpx_codec_alpha_.get());

  vpx_codec_.reset();
  vpx_codec_alpha_.reset();

  if (memory_pool_) {
    memory_pool_->Shutdown();
    memory_pool_ = nullptr;
  }
}

void VpxVideoDecoder::Detach() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!bind_callbacks_);

  CloseDecoder();
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

bool VpxVideoDecoder::VpxDecode(const DecoderBuffer* buffer,
                                scoped_refptr<VideoFrame>* video_frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(video_frame);
  DCHECK(!buffer->end_of_stream());

  {
    TRACE_EVENT1("media", "vpx_codec_decode", "buffer",
                 buffer->AsHumanReadableString());
    vpx_codec_err_t status =
        vpx_codec_decode(vpx_codec_.get(), buffer->data(), buffer->data_size(),
                         nullptr /* user_priv */, 0 /* deadline */);
    if (status != VPX_CODEC_OK) {
      DLOG(ERROR) << "vpx_codec_decode() error: "
                  << vpx_codec_err_to_string(status);
      return false;
    }
  }

  // Gets pointer to decoded data.
  vpx_codec_iter_t iter = NULL;
  const vpx_image_t* vpx_image = vpx_codec_get_frame(vpx_codec_.get(), &iter);
  if (!vpx_image) {
    *video_frame = nullptr;
    return true;
  }

  const vpx_image_t* vpx_image_alpha = nullptr;
  const auto alpha_decode_status =
      DecodeAlphaPlane(vpx_image, &vpx_image_alpha, buffer);
  if (alpha_decode_status == kAlphaPlaneError) {
    return false;
  } else if (alpha_decode_status == kNoAlphaPlaneData) {
    *video_frame = nullptr;
    return true;
  }

  if (!CopyVpxImageToVideoFrame(vpx_image, vpx_image_alpha, video_frame))
    return false;

  if (vpx_image_alpha && config_.codec() == kCodecVP8) {
    libyuv::CopyPlane(vpx_image_alpha->planes[VPX_PLANE_Y],
                      vpx_image_alpha->stride[VPX_PLANE_Y],
                      (*video_frame)->visible_data(VideoFrame::kAPlane),
                      (*video_frame)->stride(VideoFrame::kAPlane),
                      (*video_frame)->visible_rect().width(),
                      (*video_frame)->visible_rect().height());
  }

  (*video_frame)->set_timestamp(buffer->timestamp());

  // Prefer the color space from the config if available. It generally comes
  // from the color tag which is more expressive than the vp8 and vp9 bitstream.
  if (config_.color_space_info().IsSpecified()) {
    (*video_frame)
        ->set_color_space(config_.color_space_info().ToGfxColorSpace());
    return true;
  }

  auto primaries = gfx::ColorSpace::PrimaryID::INVALID;
  auto transfer = gfx::ColorSpace::TransferID::INVALID;
  auto matrix = gfx::ColorSpace::MatrixID::INVALID;
  auto range = vpx_image->range == VPX_CR_FULL_RANGE
                   ? gfx::ColorSpace::RangeID::FULL
                   : gfx::ColorSpace::RangeID::LIMITED;

  switch (vpx_image->cs) {
    case VPX_CS_BT_601:
    case VPX_CS_SMPTE_170:
      primaries = gfx::ColorSpace::PrimaryID::SMPTE170M;
      transfer = gfx::ColorSpace::TransferID::SMPTE170M;
      matrix = gfx::ColorSpace::MatrixID::SMPTE170M;
      break;
    case VPX_CS_SMPTE_240:
      primaries = gfx::ColorSpace::PrimaryID::SMPTE240M;
      transfer = gfx::ColorSpace::TransferID::SMPTE240M;
      matrix = gfx::ColorSpace::MatrixID::SMPTE240M;
      break;
    case VPX_CS_BT_709:
      primaries = gfx::ColorSpace::PrimaryID::BT709;
      transfer = gfx::ColorSpace::TransferID::BT709;
      matrix = gfx::ColorSpace::MatrixID::BT709;
      break;
    case VPX_CS_BT_2020:
      primaries = gfx::ColorSpace::PrimaryID::BT2020;
      if (vpx_image->bit_depth >= 12)
        transfer = gfx::ColorSpace::TransferID::BT2020_12;
      else if (vpx_image->bit_depth >= 10)
        transfer = gfx::ColorSpace::TransferID::BT2020_10;
      else
        transfer = gfx::ColorSpace::TransferID::BT709;
      matrix = gfx::ColorSpace::MatrixID::BT2020_NCL;  // is this right?
      break;
    case VPX_CS_SRGB:
      primaries = gfx::ColorSpace::PrimaryID::BT709;
      transfer = gfx::ColorSpace::TransferID::IEC61966_2_1;
      matrix = gfx::ColorSpace::MatrixID::BT709;
      break;
    default:
      break;
  }

  // TODO(ccameron): Set a color space even for unspecified values.
  if (primaries != gfx::ColorSpace::PrimaryID::INVALID) {
    (*video_frame)
        ->set_color_space(gfx::ColorSpace(primaries, transfer, matrix, range));
  }

  return true;
}

VpxVideoDecoder::AlphaDecodeStatus VpxVideoDecoder::DecodeAlphaPlane(
    const struct vpx_image* vpx_image,
    const struct vpx_image** vpx_image_alpha,
    const DecoderBuffer* buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!vpx_codec_alpha_ || buffer->side_data_size() < 8) {
    return kAlphaPlaneProcessed;
  }

  // First 8 bytes of side data is |side_data_id| in big endian.
  const uint64_t side_data_id = base::NetToHost64(
      *(reinterpret_cast<const uint64_t*>(buffer->side_data())));
  if (side_data_id != 1) {
    return kAlphaPlaneProcessed;
  }

  // Try and decode buffer->side_data() minus the first 8 bytes as a full
  // frame.
  {
    TRACE_EVENT1("media", "vpx_codec_decode_alpha", "buffer",
                 buffer->AsHumanReadableString());
    vpx_codec_err_t status =
        vpx_codec_decode(vpx_codec_alpha_.get(), buffer->side_data() + 8,
                         buffer->side_data_size() - 8, nullptr /* user_priv */,
                         0 /* deadline */);
    if (status != VPX_CODEC_OK) {
      DLOG(ERROR) << "vpx_codec_decode() failed for the alpha: "
                  << vpx_codec_error(vpx_codec_.get());
      return kAlphaPlaneError;
    }
  }

  vpx_codec_iter_t iter_alpha = NULL;
  *vpx_image_alpha = vpx_codec_get_frame(vpx_codec_alpha_.get(), &iter_alpha);
  if (!(*vpx_image_alpha)) {
    return kNoAlphaPlaneData;
  }

  if ((*vpx_image_alpha)->d_h != vpx_image->d_h ||
      (*vpx_image_alpha)->d_w != vpx_image->d_w) {
    DLOG(ERROR) << "The alpha plane dimensions are not the same as the "
                   "image dimensions.";
    return kAlphaPlaneError;
  }

  return kAlphaPlaneProcessed;
}

bool VpxVideoDecoder::CopyVpxImageToVideoFrame(
    const struct vpx_image* vpx_image,
    const struct vpx_image* vpx_image_alpha,
    scoped_refptr<VideoFrame>* video_frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(vpx_image);

  VideoPixelFormat codec_format;
  switch (vpx_image->fmt) {
    case VPX_IMG_FMT_I420:
      codec_format = vpx_image_alpha ? PIXEL_FORMAT_I420A : PIXEL_FORMAT_I420;
      break;

    case VPX_IMG_FMT_I422:
      codec_format = PIXEL_FORMAT_I422;
      break;

    case VPX_IMG_FMT_I444:
      codec_format = PIXEL_FORMAT_I444;
      break;

    case VPX_IMG_FMT_I42016:
      switch (vpx_image->bit_depth) {
        case 10:
          codec_format = PIXEL_FORMAT_YUV420P10;
          break;
        case 12:
          codec_format = PIXEL_FORMAT_YUV420P12;
          break;
        default:
          DLOG(ERROR) << "Unsupported bit depth: " << vpx_image->bit_depth;
          return false;
      }
      break;

    case VPX_IMG_FMT_I42216:
      switch (vpx_image->bit_depth) {
        case 10:
          codec_format = PIXEL_FORMAT_YUV422P10;
          break;
        case 12:
          codec_format = PIXEL_FORMAT_YUV422P12;
          break;
        default:
          DLOG(ERROR) << "Unsupported bit depth: " << vpx_image->bit_depth;
          return false;
      }
      break;

    case VPX_IMG_FMT_I44416:
      switch (vpx_image->bit_depth) {
        case 10:
          codec_format = PIXEL_FORMAT_YUV444P10;
          break;
        case 12:
          codec_format = PIXEL_FORMAT_YUV444P12;
          break;
        default:
          DLOG(ERROR) << "Unsupported bit depth: " << vpx_image->bit_depth;
          return false;
      }
      break;

    default:
      DLOG(ERROR) << "Unsupported pixel format: " << vpx_image->fmt;
      return false;
  }

  // The mixed |w|/|d_h| in |coded_size| is intentional. Setting the correct
  // coded width is necessary to allow coalesced memory access, which may avoid
  // frame copies. Setting the correct coded height however does not have any
  // benefit, and only risk copying too much data.
  const gfx::Size coded_size(vpx_image->w, vpx_image->d_h);
  const gfx::Size visible_size(vpx_image->d_w, vpx_image->d_h);
  // Compute natural size by scaling visible size by *pixel* aspect ratio. Note
  // that we could instead use vpx_image r_w and r_h, but doing so would allow
  // pixel aspect ratio to change on a per-frame basis which would make
  // vpx_video_decoder inconsistent with decoders where changes to
  // pixel aspect ratio are not surfaced (e.g. Android MediaCodec).
  const gfx::Size natural_size =
      GetNaturalSize(gfx::Rect(visible_size), config_.GetPixelAspectRatio());

  if (memory_pool_) {
    DCHECK_EQ(kCodecVP9, config_.codec());
    if (vpx_image_alpha) {
      size_t alpha_plane_size =
          vpx_image_alpha->stride[VPX_PLANE_Y] * vpx_image_alpha->d_h;
      uint8_t* alpha_plane = memory_pool_->AllocateAlphaPlaneForFrameBuffer(
          alpha_plane_size, vpx_image->fb_priv);
      libyuv::CopyPlane(vpx_image_alpha->planes[VPX_PLANE_Y],
                        vpx_image_alpha->stride[VPX_PLANE_Y], alpha_plane,
                        vpx_image_alpha->stride[VPX_PLANE_Y],
                        vpx_image_alpha->d_w, vpx_image_alpha->d_h);
      *video_frame = VideoFrame::WrapExternalYuvaData(
          codec_format, coded_size, gfx::Rect(visible_size), natural_size,
          vpx_image->stride[VPX_PLANE_Y], vpx_image->stride[VPX_PLANE_U],
          vpx_image->stride[VPX_PLANE_V], vpx_image_alpha->stride[VPX_PLANE_Y],
          vpx_image->planes[VPX_PLANE_Y], vpx_image->planes[VPX_PLANE_U],
          vpx_image->planes[VPX_PLANE_V], alpha_plane, kNoTimestamp);
    } else {
      *video_frame = VideoFrame::WrapExternalYuvData(
          codec_format, coded_size, gfx::Rect(visible_size), natural_size,
          vpx_image->stride[VPX_PLANE_Y], vpx_image->stride[VPX_PLANE_U],
          vpx_image->stride[VPX_PLANE_V], vpx_image->planes[VPX_PLANE_Y],
          vpx_image->planes[VPX_PLANE_U], vpx_image->planes[VPX_PLANE_V],
          kNoTimestamp);
    }
    if (!(*video_frame))
      return false;

    video_frame->get()->AddDestructionObserver(
        memory_pool_->CreateFrameCallback(vpx_image->fb_priv));
    return true;
  }

  *video_frame = frame_pool_.CreateFrame(codec_format, visible_size,
                                         gfx::Rect(visible_size), natural_size,
                                         kNoTimestamp);
  if (!(*video_frame))
    return false;

  for (int plane = 0; plane < 3; plane++) {
    libyuv::CopyPlane(
        vpx_image->planes[plane], vpx_image->stride[plane],
        (*video_frame)->visible_data(plane), (*video_frame)->stride(plane),
        (*video_frame)->row_bytes(plane), (*video_frame)->rows(plane));
  }

  return true;
}

}  // namespace media
