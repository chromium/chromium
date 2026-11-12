// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/codec/webrtc_video_encoder_av1.h"

#include <algorithm>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/system/sys_info.h"
#include "remoting/base/cpu_utils.h"
#include "remoting/base/util.h"
#include "remoting/codec/utils.h"
#include "third_party/libaom/source/libaom/aom/aom_image.h"
#include "third_party/libaom/source/libaom/aom/aomcx.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_region.h"

namespace remoting {

namespace {

void DestroyAomCodecContext(aom_codec_ctx_t* codec_ctx) {
  if (codec_ctx->name) {
    // Codec has been initialized so we need to destroy it.
    auto error = aom_codec_destroy(codec_ctx);
    DCHECK_EQ(error, AOM_CODEC_OK);
  }
  delete codec_ctx;
}

}  // namespace

WebrtcVideoEncoderAV1::WebrtcVideoEncoderAV1()
    : codec_(nullptr, DestroyAomCodecContext),
      image_(nullptr, aom_img_free),
      av1_encoder_speed_(kDefaultEncoderSpeed),
      bitrate_filter_(
          EncoderBitrateFilter::kDefaultMinimumBitrateKbpsPerMegapixel) {
  ConfigureCodecParams();
}
WebrtcVideoEncoderAV1::~WebrtcVideoEncoderAV1() = default;

void WebrtcVideoEncoderAV1::SetLosslessColor(bool want_lossless) {
  if (want_lossless != lossless_color_) {
    lossless_color_ = want_lossless;
    codec_.reset();
  }
}

void WebrtcVideoEncoderAV1::SetEncoderSpeed(int encoder_speed) {
  // Clamp values are based on the lowest and highest values available when
  // realtime encoding with AV1.  This allows for client-driven experimentation,
  // however in practice, a value of 10 or 11 should be chosen as they will give
  // the best performance.
  int clamped_speed =
      std::clamp<int>(encoder_speed, kMinEncoderSpeed, kMaxEncoderSpeed);
  if (av1_encoder_speed_ != clamped_speed) {
    VLOG(0) << "Setting AV1 encoder speed to " << clamped_speed;
    av1_encoder_speed_ = clamped_speed;
    codec_.reset();
  }
}

void WebrtcVideoEncoderAV1::SetUseActiveMap(bool use_active_map) {
  if (use_active_map != use_active_map_) {
    use_active_map_ = use_active_map;
    codec_.reset();
  }
}

bool WebrtcVideoEncoderAV1::InitializeCodec(const webrtc::DesktopSize& size) {
  // Set the width, height, and thread count now that the frame size is known.
  config_.g_w = size.width();
  config_.g_h = size.height();
  // Determine the number of threads to use for encoding by choosing the larger
  // of the two dimensions. If we only checked the width, the performance for
  // displays in portrait mode will be degraded due to a lower thread count.
  // Since AV1 supports dividing the image into both rows and cols, we set the
  // maximum number of threads here and then set the tile_row and tile_column
  // values based on the frame dimensions later on.
  config_.g_threads = GetEncoderThreadCount(std::max(config_.g_w, config_.g_h));

  // Choose a profile based on whether we should provide frames in I420 or I444.
  config_.g_profile = lossless_color_ ? 1 : 0;

  // Initialize an encoder instance.
  scoped_aom_codec codec(new aom_codec_ctx_t, DestroyAomCodecContext);
  codec->name = nullptr;
  aom_codec_flags_t flags = 0;
  auto error =
      aom_codec_enc_init(codec.get(), aom_codec_av1_cx(), &config_, flags);
  if (error != AOM_CODEC_OK) {
    LOG(ERROR) << "Failed to initialize codec: " << error;
    return false;
  }
  DCHECK_NE(codec->name, nullptr);
  last_init_target_bitrate_kbps_ = config_.rc_target_bitrate;

  codec_active_map_enabled_ = false;
  if (use_active_map_) {
    active_map_data_.Initialize(size);
    active_map_ = {
        .active_map = active_map_data_.data(),
        .rows = active_map_data_.height(),
        .cols = active_map_data_.width(),
    };
  }

  error = aom_codec_control(codec.get(), AOME_SET_CPUUSED, av1_encoder_speed_);
  DCHECK_EQ(error, AOM_CODEC_OK) << "Failed to set AOME_SET_CPUUSED";

  error = aom_codec_control(codec.get(), AV1E_SET_AQ_MODE, 3);
  DCHECK_EQ(error, AOM_CODEC_OK) << "Failed to set AV1E_SET_AQ_MODE";

  if (config_.g_threads > 1) {
    error = aom_codec_control(codec.get(), AV1E_SET_ROW_MT, 1);
    DCHECK_EQ(error, AOM_CODEC_OK) << "Failed to set AV1E_SET_ROW_MT";

    // Use a smaller superblock size for 5k, 4k, and 2k displays.
    if (config_.g_threads > 4) {
      // The default value for AV1E_SET_SUPERBLOCK_SIZE is
      // AOM_SUPERBLOCK_SIZE_DYNAMIC which uses 64x64 blocks for resolutions of
      // 720px or lower and 128x128 for resolutions above that. When testing
      // with display resolutions higher than 3840x2160 (4K), we found that
      // increasing the number of threads from 16 to 32 increased performance
      // for VP9 but performance decreased with AV1. The codec team investigated
      // and suggested adjusting the superblock size down to 64x64 to increase
      // the amount of parallelism.  This solution increased performance for 5K
      // displays and put it around the level of what we see for a 4K display.
      // Additional testing with 4K and 2K displays showed that a smaller
      // superblock size increased performance there as well.
      // See https://crbug.com/aomedia/3363 for more info.
      error = aom_codec_control(codec.get(), AV1E_SET_SUPERBLOCK_SIZE,
                                AOM_SUPERBLOCK_SIZE_64X64);
      DCHECK_EQ(error, AOM_CODEC_OK)
          << "Failed to set AV1E_SET_SUPERBLOCK_SIZE";
    }
  }

  // The param used to set the tile columns and tile rows is in log2 so 0 is ok.
  int log2_threads = static_cast<int>(std::log2(config_.g_threads));
  // Split the cols/rows as evenly as possible, favoring columns for widescreen.
  int log2_cols = (log2_threads + 1) / 2;
  int log2_rows = log2_threads - log2_cols;
  if (size.height() > size.width()) {
    // Swap for portrait mode since we want more rows than columns.
    std::swap(log2_cols, log2_rows);
  }

  error = aom_codec_control(codec.get(), AV1E_SET_TILE_COLUMNS, log2_cols);
  DCHECK_EQ(error, AOM_CODEC_OK) << "Failed to set AV1E_SET_TILE_COLUMNS";

  error = aom_codec_control(codec.get(), AV1E_SET_TILE_ROWS, log2_rows);
  DCHECK_EQ(error, AOM_CODEC_OK) << "Failed to set AV1E_SET_TILE_ROWS";

  // These make realtime encoding faster.
  error =
      aom_codec_control(codec.get(), AV1E_SET_TUNE_CONTENT, AOM_CONTENT_SCREEN);
  DCHECK_EQ(error, AOM_CODEC_OK) << "Failed to set AOM_CONTENT_SCREEN";
  error = aom_codec_control(codec.get(), AV1E_SET_ENABLE_PALETTE, 1);
  DCHECK_EQ(error, AOM_CODEC_OK) << "Failed to set AV1E_SET_ENABLE_PALETTE";
  error = aom_codec_control(codec.get(), AV1E_SET_ENABLE_INTRABC, 0);
  DCHECK_EQ(error, AOM_CODEC_OK) << "Failed to set AV1E_SET_ENABLE_INTRABC";

  // These default to on but should not be used for real-time encoding.
  error = aom_codec_control(codec.get(), AV1E_SET_ENABLE_TPL_MODEL, 0);
  DCHECK_EQ(error, AOM_CODEC_OK) << "Failed to set AV1E_SET_ENABLE_TPL_MODEL";
  error = aom_codec_control(codec.get(), AV1E_SET_DELTAQ_MODE, 0);
  DCHECK_EQ(error, AOM_CODEC_OK) << "Failed to set AV1E_SET_DELTAQ_MODE";
  error = aom_codec_control(codec.get(), AV1E_SET_ENABLE_ORDER_HINT, 0);
  DCHECK_EQ(error, AOM_CODEC_OK) << "Failed to set AV1E_SET_ENABLE_ORDER_HINT";
  error = aom_codec_control(codec.get(), AV1E_SET_ENABLE_OBMC, 0);
  DCHECK_EQ(error, AOM_CODEC_OK) << "Failed to set AV1E_SET_ENABLE_OBMC";
  error = aom_codec_control(codec.get(), AV1E_SET_ENABLE_WARPED_MOTION, 0);
  DCHECK_EQ(error, AOM_CODEC_OK)
      << "Failed to set AV1E_SET_ENABLE_WARPED_MOTION";
  error = aom_codec_control(codec.get(), AV1E_SET_ENABLE_GLOBAL_MOTION, 0);
  DCHECK_EQ(error, AOM_CODEC_OK)
      << "Failed to set AV1E_SET_ENABLE_GLOBAL_MOTION";
  error = aom_codec_control(codec.get(), AV1E_SET_ENABLE_REF_FRAME_MVS, 0);
  DCHECK_EQ(error, AOM_CODEC_OK)
      << "Failed to set AV1E_SET_ENABLE_REF_FRAME_MVS";

  // These should significantly speed up key frame encoding.
  error = aom_codec_control(codec.get(), AV1E_SET_ENABLE_CFL_INTRA, 0);
  DCHECK_EQ(error, AOM_CODEC_OK) << "Failed to set AV1E_SET_ENABLE_CFL_INTRA";
  error = aom_codec_control(codec.get(), AV1E_SET_ENABLE_SMOOTH_INTRA, 0);
  DCHECK_EQ(error, AOM_CODEC_OK)
      << "Failed to set AV1E_SET_ENABLE_SMOOTH_INTRA";
  error = aom_codec_control(codec.get(), AV1E_SET_ENABLE_ANGLE_DELTA, 0);
  DCHECK_EQ(error, AOM_CODEC_OK) << "Failed to set AV1E_SET_ENABLE_ANGLE_DELTA";
  error = aom_codec_control(codec.get(), AV1E_SET_ENABLE_FILTER_INTRA, 0);
  DCHECK_EQ(error, AOM_CODEC_OK)
      << "Failed to set AV1E_SET_ENABLE_FILTER_INTRA";
  error = aom_codec_control(codec.get(), AV1E_SET_INTRA_DEFAULT_TX_ONLY, 1);
  DCHECK_EQ(error, AOM_CODEC_OK)
      << "Failed to set AV1E_SET_INTRA_DEFAULT_TX_ONLY";

  // Misc. quality settings.
  error = aom_codec_control(codec.get(), AV1E_SET_ENABLE_CDEF, 1);
  DCHECK_EQ(error, AOM_CODEC_OK) << "Failed to set AV1E_SET_ENABLE_CDEF";
  error = aom_codec_control(codec.get(), AV1E_SET_NOISE_SENSITIVITY, 0);
  DCHECK_EQ(error, AOM_CODEC_OK) << "Failed to set AV1E_SET_NOISE_SENSITIVITY";
  error = aom_codec_control(codec.get(), AV1E_SET_ENABLE_PAETH_INTRA, 0);
  DCHECK_EQ(error, AOM_CODEC_OK) << "Failed to set AV1E_SET_ENABLE_PAETH_INTRA";
  error = aom_codec_control(codec.get(), AV1E_SET_ENABLE_QM, 0);
  DCHECK_EQ(error, AOM_CODEC_OK) << "Failed to set AV1E_SET_ENABLE_QM";
  error = aom_codec_control(codec.get(), AV1E_SET_ENABLE_RECT_PARTITIONS, 0);
  DCHECK_EQ(error, AOM_CODEC_OK)
      << "Failed to set AV1E_SET_ENABLE_RECT_PARTITIONS";
  error = aom_codec_control(codec.get(), AV1E_SET_ENABLE_RESTORATION, 0);
  DCHECK_EQ(error, AOM_CODEC_OK) << "Failed to set AV1E_SET_ENABLE_RESTORATION";
  error = aom_codec_control(codec.get(), AV1E_SET_ENABLE_SMOOTH_INTERINTRA, 0);
  DCHECK_EQ(error, AOM_CODEC_OK)
      << "Failed to set AV1E_SET_ENABLE_SMOOTH_INTERINTRA";
  error = aom_codec_control(codec.get(), AV1E_SET_ENABLE_TX64, 0);
  DCHECK_EQ(error, AOM_CODEC_OK) << "Failed to set AV1E_SET_ENABLE_TX64";
  error = aom_codec_control(codec.get(), AV1E_SET_MAX_REFERENCE_FRAMES, 3);
  DCHECK_EQ(error, AOM_CODEC_OK)
      << "Failed to set AV1E_SET_MAX_REFERENCE_FRAMES";

  // Suggested by the AOM team to decrease encode time per frame. Setting this
  // value to 2 decreased per-frame encode time by ~1ms at 3840x2160 with no
  // noticeable loss of quality.
  error = aom_codec_control(codec.get(), AV1E_SET_CDF_UPDATE_MODE, 2);
  DCHECK_EQ(error, AOM_CODEC_OK) << "Failed to set AV1E_SET_CDF_UPDATE_MODE";

  codec_ = std::move(codec);
  return true;
}

void WebrtcVideoEncoderAV1::PrepareImage(
    const webrtc::DesktopFrame* frame,
    webrtc::DesktopRegion& updated_region) {
  updated_region.Clear();
  if (!frame) {
    return;
  }

  if (image_) {
    DCHECK_EQ(frame->size().width(), static_cast<int>(image_->d_w));
    DCHECK_EQ(frame->size().height(), static_cast<int>(image_->d_h));
    for (webrtc::DesktopRegion::Iterator r(frame->updated_region());
         !r.IsAtEnd(); r.Advance()) {
      const webrtc::DesktopRect& rect = r.rect();
      updated_region.AddRect(AlignRect(webrtc::DesktopRect::MakeLTRB(
          rect.left(), rect.top(), rect.right(), rect.bottom())));
    }

    // Clip back to the screen dimensions.
    updated_region.IntersectWith(
        webrtc::DesktopRect::MakeWH(image_->d_w, image_->d_h));
  } else {
    aom_img_fmt_t fmt = lossless_color_ ? AOM_IMG_FMT_I444 : AOM_IMG_FMT_I420;
    image_.reset(aom_img_alloc(nullptr, fmt, frame->size().width(),
                               frame->size().height(),
                               GetSimdMemoryAlignment()));
    updated_region.AddRect(
        webrtc::DesktopRect::MakeWH(image_->d_w, image_->d_h));
  }

  // Convert the updated region to YUV ready for encoding.
  DCHECK(image_->fmt == AOM_IMG_FMT_I420 || image_->fmt == AOM_IMG_FMT_I444);
  const bool is_i444 = image_->fmt == AOM_IMG_FMT_I444;
  const size_t y_rows = image_->d_h;
  const size_t uv_rows = is_i444 ? image_->d_h : (image_->d_h + 1) / 2;
  // SAFETY: `image_->planes[i]` is allocated by `aom_img_alloc` with at least
  // `image_->stride[i] * rows` bytes for each plane.
  ConvertDesktopFrameRegionToYuv(
      *frame, updated_region, is_i444,
      UNSAFE_TODO(base::span(image_->planes[0],
                             static_cast<size_t>(image_->stride[0]) * y_rows)),
      image_->stride[0],
      UNSAFE_TODO(base::span(image_->planes[1],
                             static_cast<size_t>(image_->stride[1]) * uv_rows)),
      image_->stride[1],
      UNSAFE_TODO(base::span(image_->planes[2],
                             static_cast<size_t>(image_->stride[2]) * uv_rows)),
      image_->stride[2]);
}

void WebrtcVideoEncoderAV1::ConfigureCodecParams() {
  auto error = aom_codec_enc_config_default(aom_codec_av1_cx(), &config_,
                                            AOM_USAGE_REALTIME);
  if (error != AOM_CODEC_OK) {
    LOG(ERROR) << "Failed to configure default codec params: " << error;
    return;
  }

  // Encoder config values are defined in:
  // //third_party/libaom/source/libaom/aom/aom_encoder.h

  // Default to profile 0 and update later if lossless color is requested.
  config_.g_profile = 0;
  // Width, height, and thread count are set once the frame size is known.
  config_.g_w = 0;
  config_.g_h = 0;
  config_.g_threads = 0;

  config_.g_input_bit_depth = 8;
  config_.g_pass = AOM_RC_ONE_PASS;
  config_.g_lag_in_frames = 0;
  config_.g_error_resilient = 0;
  config_.g_timebase.num = 1;
  config_.g_timebase.den = base::Time::kMillisecondsPerSecond;

  config_.kf_mode = AOM_KF_DISABLED;

  // Default max/min quantizer bounds match WebrtcVideoEncoderWrapper and
  // WebrtcVideoEncoderVpx. Capping max quantizer at 50 (rather than the general
  // WebRTC default of 52) avoids a ~16% jump in AC quantization step size at
  // the ceiling, preserving text legibility during keyframes or large screen
  // updates and reducing the number of top-off frames needed to converge to the
  // minimum quantizer (10), below which additional bandwidth yields negligible
  // visual improvement.
  config_.rc_max_quantizer = kMaxQuantizer;
  config_.rc_min_quantizer = kMinQuantizer;
  config_.rc_dropframe_thresh = 0;
  config_.rc_undershoot_pct = 50;
  config_.rc_overshoot_pct = 50;
  config_.rc_buf_initial_sz = 600;
  config_.rc_buf_optimal_sz = 600;
  config_.rc_buf_sz = 1000;
  config_.rc_end_usage = AOM_CBR;
  config_.rc_target_bitrate = EncoderBitrateFilter::kDefaultMinimumBitrateKbps;
}

void WebrtcVideoEncoderAV1::UpdateConfig(const FrameParams& params) {
  bool changed = false;

  if (params.bitrate_kbps >= 0) {
    bitrate_filter_.SetBandwidthEstimateKbps(params.bitrate_kbps);
  }
  const unsigned int target_bitrate =
      static_cast<unsigned int>(bitrate_filter_.GetTargetBitrateKbps());
  if (config_.rc_target_bitrate != target_bitrate) {
    config_.rc_target_bitrate = target_bitrate;
    changed = true;
  }

  if (params.vpx_min_quantizer >= 0 &&
      config_.rc_min_quantizer !=
          static_cast<unsigned int>(params.vpx_min_quantizer)) {
    config_.rc_min_quantizer = params.vpx_min_quantizer;
    changed = true;
  }

  if (params.vpx_max_quantizer >= 0 &&
      config_.rc_max_quantizer !=
          static_cast<unsigned int>(params.vpx_max_quantizer)) {
    config_.rc_max_quantizer = params.vpx_max_quantizer;
    changed = true;
  }

  if (!changed || !codec_) {
    return;
  }

  // Update encoder context.
  if (aom_codec_enc_config_set(codec_.get(), &config_)) {
    NOTREACHED() << "Unable to set encoder config";
  }
}

void WebrtcVideoEncoderAV1::Encode(std::unique_ptr<webrtc::DesktopFrame> frame,
                                   const FrameParams& params,
                                   EncodeCallback done) {
  if (frame) {
    bitrate_filter_.SetFrameSize(frame->size().width(), frame->size().height());
  }

  webrtc::DesktopSize previous_frame_size =
      image_ ? webrtc::DesktopSize(image_->d_w, image_->d_h)
             : webrtc::DesktopSize();

  webrtc::DesktopSize frame_size = frame ? frame->size() : previous_frame_size;

  if (frame_size.is_empty()) {
    std::move(done).Run(EncodeResult::SUCCEEDED, nullptr);
    return;
  }

  DCHECK_GE(frame_size.width(), kMinDimension);
  DCHECK_GE(frame_size.height(), kMinDimension);

  // Create or reconfigure the codec to match the size of |frame|.
  if (!codec_ || !frame_size.equals(previous_frame_size)) {
    image_.reset();
    codec_.reset();
  }

  // Populate target bitrate and quantizer bounds in |config_| before
  // InitializeCodec() so aom_codec_enc_init() initializes its rate-control
  // buffer level with the actual target bitrate rather than the 1000 kbps
  // default (EncoderBitrateFilter::kDefaultMinimumBitrateKbps), since
  // av1_change_config() only clamps buffer_level down and never scales it up.
  UpdateConfig(params);

  if (!codec_ && !InitializeCodec(frame_size)) {
    std::move(done).Run(EncodeResult::UNKNOWN_ERROR, nullptr);
    return;
  }

  // Transfer the frame data to the image buffer for encoding.
  // TODO(joedow): Look into using aom_img_wrap instead of our own buffer.
  webrtc::DesktopRegion updated_region;
  PrepareImage(frame.get(), updated_region);

  if (use_active_map_) {
    auto disable_codec_active_map = [&]() {
      if (codec_active_map_enabled_) {
        // Passing a null `active_map` buffer makes libaom's
        // `av1_set_active_map()` reset `rc.percent_blocks_inactive = 0` without
        // scanning the 4x4 MI grid; on the next encode,
        // `av1_apply_active_map()` sees `percent_blocks_inactive == 0` and sets
        // `active_map.enabled = 0, update = 1`.
        aom_active_map_t empty_map = {nullptr, active_map_.rows,
                                      active_map_.cols};
        if (aom_codec_control(codec_.get(), AOME_SET_ACTIVEMAP, &empty_map)) {
          LOG(ERROR) << "Unable to disable active map";
        } else {
          codec_active_map_enabled_ = false;
        }
      }
    };

    if (params.key_frame) {
      // AV1 does not use an active map for keyframes, but we still populate
      // |active_map_data_| with the full frame so subsequent top-off frames
      // (with clear_active_map = false) know the entire frame is active.
      active_map_data_.SetFull();
      disable_codec_active_map();
    } else {
      if (params.clear_active_map) {
        active_map_data_.Clear();
      }
      active_map_data_.Update(updated_region);

      const size_t active_blocks = active_map_data_.active_blocks();
      const size_t total_blocks = active_map_data_.total_blocks();
      // Only enable libaom's active map when [2.5%, 25%] of 16x16 macroblocks
      // are active (chosen via `webrtc_video_encoder_benchmark`). Below 2.5%,
      // full-frame 4x4 segmentation-map coding outweighs superblock skip
      // savings on tiny updates. Above 25%, active-map overhead and libaom
      // disabling `set_zeromv_skip_based_on_source_sad` outweigh skipping the
      // remaining inactive blocks.
      if (active_blocks * 4 > total_blocks ||
          active_blocks * 40 < total_blocks) {
        disable_codec_active_map();
      } else {
        if (aom_codec_control(codec_.get(), AOME_SET_ACTIVEMAP, &active_map_)) {
          LOG(ERROR) << "Unable to apply active map";
        } else {
          codec_active_map_enabled_ = true;
        }
      }
    }
  }

  auto duration_ms = params.duration.InMilliseconds();
  DCHECK_GT(duration_ms, 0);
  aom_codec_err_t ret = aom_codec_encode(
      codec_.get(), image_.get(), artificial_timestamp_ms_, duration_ms,
      (params.key_frame) ? AOM_EFLAG_FORCE_KF : 0);
  artificial_timestamp_ms_ += duration_ms;
  if (ret != AOM_CODEC_OK) {
    const char* error_detail = aom_codec_error_detail(codec_.get());
    LOG(ERROR) << "Encoding error: " << aom_codec_err_to_string(ret) << "\n  "
               << aom_codec_error(codec_.get()) << "\n  "
               << (error_detail ? error_detail : "No error details");
    std::move(done).Run(EncodeResult::UNKNOWN_ERROR, nullptr);
    return;
  }

  // Read the encoded data.
  aom_codec_iter_t iter = nullptr;
  bool got_data = false;

  auto encoded_frame = std::make_unique<EncodedFrame>();
  encoded_frame->dimensions = frame_size;
  encoded_frame->codec = webrtc::kVideoCodecAV1;
  encoded_frame->profile = config_.g_profile;
  if (params.key_frame) {
    encoded_frame->encoded_rect_width = frame_size.width();
    encoded_frame->encoded_rect_height = frame_size.height();
  } else {
    const webrtc::DesktopRect bounding_rectangle =
        GetBoundingRect(updated_region);
    encoded_frame->encoded_rect_width = bounding_rectangle.width();
    encoded_frame->encoded_rect_height = bounding_rectangle.height();
  }

  while (!got_data) {
    const aom_codec_cx_pkt_t* aom_packet =
        aom_codec_get_cx_data(codec_.get(), &iter);
    if (!aom_packet) {
      continue;
    }

    switch (aom_packet->kind) {
      case AOM_CODEC_CX_FRAME_PKT: {
        got_data = true;
        encoded_frame->data = webrtc::EncodedImageBuffer::Create(
            reinterpret_cast<const uint8_t*>(aom_packet->data.frame.buf),
            aom_packet->data.frame.sz);
        encoded_frame->key_frame =
            aom_packet->data.frame.flags & AOM_FRAME_IS_KEY;
        CHECK_EQ(aom_codec_control(codec_.get(), AOME_GET_LAST_QUANTIZER_64,
                                   &(encoded_frame->quantizer)),
                 AOM_CODEC_OK);
        break;
      }
      default:
        break;
    }
  }

  std::move(done).Run(EncodeResult::SUCCEEDED, std::move(encoded_frame));
}

}  // namespace remoting
