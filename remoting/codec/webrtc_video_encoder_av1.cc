// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/codec/webrtc_video_encoder_av1.h"

#include "base/callback.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/system/sys_info.h"
#include "remoting/base/util.h"
#include "third_party/libyuv/include/libyuv/convert_from_argb.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_region.h"

namespace remoting {

namespace {

constexpr int kBlockAlignSize = 16;
constexpr int kBlockAlignMask = ~15;

void DestroyAomCodecContext(aom_codec_ctx_t* codec_ctx) {
  if (codec_ctx->name) {
    // Codec has been initialized so we need to destroy it.
    auto error = aom_codec_destroy(codec_ctx);
    DCHECK_EQ(error, AOM_CODEC_OK);
  }
  delete codec_ctx;
}

// Minimum target bitrate per megapixel.
// TODO(joedow): Perform some additional testing to see if this needs tweaking.
constexpr int kAv1MinimumTargetBitrateKbpsPerMegapixel = 2500;

}  // namespace

WebrtcVideoEncoderAV1::WebrtcVideoEncoderAV1()
    : codec_(nullptr, DestroyAomCodecContext),
      bitrate_filter_(kAv1MinimumTargetBitrateKbpsPerMegapixel) {
  ConfigureCodecParams();
}
WebrtcVideoEncoderAV1::~WebrtcVideoEncoderAV1() = default;

void WebrtcVideoEncoderAV1::SetLosslessEncode(bool want_lossless) {
  NOTIMPLEMENTED();
}

void WebrtcVideoEncoderAV1::SetLosslessColor(bool want_lossless) {
  NOTIMPLEMENTED();
}

bool WebrtcVideoEncoderAV1::InitializeCodec(const webrtc::DesktopSize& size) {
  // Set the config's width and height now that it's known.
  config_.g_w = size.width();
  config_.g_h = size.height();

  // Initialize an encoder instance.
  aom_codec_unique_ptr codec(new aom_codec_ctx_t, DestroyAomCodecContext);
  codec->name = nullptr;
  aom_codec_flags_t flags = 0;
  auto error =
      aom_codec_enc_init(codec.get(), aom_codec_av1_cx(), &config_, flags);
  if (error != AOM_CODEC_OK) {
    LOG(ERROR) << "Failed to initialize codec: " << error;
    return false;
  }
  DCHECK_NE(codec->name, nullptr);

  error = aom_codec_control(codec.get(), AOME_SET_CPUUSED, 10);
  DCHECK_EQ(error, AOM_CODEC_OK) << "Failed to set AOME_SET_CPUUSED";

  if (config_.g_threads > 1) {
    error = aom_codec_control(codec.get(), AV1E_SET_ROW_MT, 1);
    DCHECK_EQ(error, AOM_CODEC_OK) << "Failed to set AV1E_SET_ROW_MT";
  }

  // The param for the tile column/row controls are log2 values so 0 is ok.
  int log2_tiles = std::min(static_cast<int>(config_.g_threads >> 1), 6);

  error = aom_codec_control(codec.get(), AV1E_SET_TILE_COLUMNS, log2_tiles);
  DCHECK_EQ(error, AOM_CODEC_OK) << "Failed to set AV1E_SET_TILE_COLUMNS";

  error = aom_codec_control(codec.get(), AV1E_SET_TILE_ROWS, log2_tiles);
  DCHECK_EQ(error, AOM_CODEC_OK) << "Failed to set AV1E_SET_TILE_ROWS";

  error = aom_codec_control(codec.get(), AV1E_SET_AQ_MODE, 3);
  DCHECK_EQ(error, AOM_CODEC_OK) << "Failed to set AV1E_SET_AQ_MODE";

  codec_ = std::move(codec);
  return true;
}

void WebrtcVideoEncoderAV1::CreateImage(const webrtc::DesktopSize& size) {
  DCHECK(!size.is_empty());

  std::unique_ptr<aom_image_t> image(new aom_image_t());
  memset(image.get(), 0, sizeof(aom_image_t));

  image->d_w = size.width();
  image->w = size.width();
  image->d_h = size.height();
  image->h = size.height();
  image->fmt = AOM_IMG_FMT_I420;
  image->x_chroma_shift = 1;
  image->y_chroma_shift = 1;

  // libyuv's fast-path for SSE3 requires 16-byte aligned pointers and strides,
  // so pad the Y, U and V planes' strides to multiples of 16 bytes.
  // TODO(joedow): libyuv's fastpath for AVX2 requires 32-byte alignment. Update
  // this encoder and VPX to test for performance gains with that alignment.
  const int y_stride = ((image->w - 1) & kBlockAlignMask) + kBlockAlignSize;
  const int uv_unaligned_stride = y_stride >> image->x_chroma_shift;
  const int uv_stride =
      ((uv_unaligned_stride - 1) & kBlockAlignMask) + kBlockAlignSize;

  const int y_rows = ((image->h - 1) & kBlockAlignMask) + kBlockAlignSize;
  const int uv_rows = y_rows >> image->y_chroma_shift;

  // Allocate a YUV buffer large enough for the aligned data & padding. Init the
  // memory with 128 so we only need to fill in the y plane.
  const int buffer_size = y_stride * y_rows + 2 * uv_stride * uv_rows;
  std::unique_ptr<uint8_t[]> image_buffer(new uint8_t[buffer_size]{128});

  // Fill in the information for |image_|.
  unsigned char* uchar_buffer =
      reinterpret_cast<unsigned char*>(image_buffer.get());
  image->planes[0] = uchar_buffer;
  image->planes[1] = image->planes[0] + y_stride * y_rows;
  image->planes[2] = image->planes[1] + uv_stride * uv_rows;
  image->stride[0] = y_stride;
  image->stride[1] = uv_stride;
  image->stride[2] = uv_stride;

  image_ = std::move(image);
  image_buffer_ = std::move(image_buffer);
}

void WebrtcVideoEncoderAV1::PrepareImage(const webrtc::DesktopFrame* frame) {
  if (!frame) {
    return;
  }

  webrtc::DesktopRegion updated_region;
  if (image_) {
    DCHECK_EQ(frame->size().width(), static_cast<int>(image_->w));
    DCHECK_EQ(frame->size().height(), static_cast<int>(image_->h));
    for (webrtc::DesktopRegion::Iterator r(frame->updated_region());
         !r.IsAtEnd(); r.Advance()) {
      const webrtc::DesktopRect& rect = r.rect();
      updated_region.AddRect(AlignRect(webrtc::DesktopRect::MakeLTRB(
          rect.left(), rect.top(), rect.right(), rect.bottom())));
    }

    // Clip back to the screen dimensions.
    updated_region.IntersectWith(
        webrtc::DesktopRect::MakeWH(image_->w, image_->h));
  } else {
    CreateImage(frame->size());
    updated_region.AddRect(webrtc::DesktopRect::MakeWH(image_->w, image_->h));
  }

  // Convert the updated region to YUV ready for encoding.
  const uint8_t* rgb_data = frame->data();
  const int rgb_stride = frame->stride();
  const int y_stride = image_->stride[0];
  DCHECK_EQ(image_->stride[1], image_->stride[2]);
  const int uv_stride = image_->stride[1];
  uint8_t* y_data = image_->planes[0];
  uint8_t* u_data = image_->planes[1];
  uint8_t* v_data = image_->planes[2];

  CHECK_EQ(image_->fmt, AOM_IMG_FMT_I420);
  for (webrtc::DesktopRegion::Iterator r(updated_region); !r.IsAtEnd();
       r.Advance()) {
    const webrtc::DesktopRect& rect = r.rect();
    int rgb_offset = rgb_stride * rect.top() +
                     rect.left() * webrtc::DesktopFrame::kBytesPerPixel;
    int y_offset = y_stride * rect.top() + rect.left();
    int uv_offset = uv_stride * rect.top() / 2 + rect.left() / 2;
    libyuv::ARGBToI420(rgb_data + rgb_offset, rgb_stride, y_data + y_offset,
                       y_stride, u_data + uv_offset, uv_stride,
                       v_data + uv_offset, uv_stride, rect.width(),
                       rect.height());
  }
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
  config_.g_profile = 0;
  // Width and height are reset once the frame size is known.
  config_.g_w = 0;
  config_.g_h = 0;
  config_.g_input_bit_depth = 8;
  config_.g_pass = AOM_RC_ONE_PASS;
  config_.g_lag_in_frames = 0;
  config_.g_error_resilient = 0;
  config_.g_timebase.num = 1;
  config_.g_timebase.den = base::Time::kMicrosecondsPerSecond;
  config_.g_threads = (base::SysInfo::NumberOfProcessors() + 1) / 2;

  config_.kf_mode = AOM_KF_AUTO;
  config_.kf_min_dist = 10000;
  config_.kf_max_dist = 10000;

  config_.rc_max_quantizer = 0;
  config_.rc_min_quantizer = 0;
  config_.rc_dropframe_thresh = 0;
  config_.rc_undershoot_pct = 100;
  config_.rc_overshoot_pct = 15;
  config_.rc_end_usage = AOM_CBR;
  config_.rc_target_bitrate = 1000000;  // 1Mbps
}

void WebrtcVideoEncoderAV1::UpdateConfig(const FrameParams& params) {
  bool changed = false;

  if (params.bitrate_kbps >= 0) {
    bitrate_filter_.SetBandwidthEstimateKbps(params.bitrate_kbps);
    if (config_.rc_target_bitrate !=
        static_cast<unsigned int>(bitrate_filter_.GetTargetBitrateKbps())) {
      config_.rc_target_bitrate = bitrate_filter_.GetTargetBitrateKbps();
      changed = true;
    }
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

  if (!changed)
    return;

  // Update encoder context.
  if (aom_codec_enc_config_set(codec_.get(), &config_)) {
    NOTREACHED() << "Unable to set encoder config";
  }
}

void WebrtcVideoEncoderAV1::FreeImageMembers() {
  image_.reset();
  image_buffer_.reset();
}

void WebrtcVideoEncoderAV1::Encode(std::unique_ptr<webrtc::DesktopFrame> frame,
                                   const FrameParams& params,
                                   EncodeCallback done) {
  if (frame) {
    bitrate_filter_.SetFrameSize(frame->size().width(), frame->size().height());
  }

  webrtc::DesktopSize previous_frame_size =
      image_ ? webrtc::DesktopSize(image_->w, image_->h)
             : webrtc::DesktopSize();

  webrtc::DesktopSize frame_size = frame ? frame->size() : previous_frame_size;

  if (frame_size.is_empty()) {
    std::move(done).Run(EncodeResult::SUCCEEDED, nullptr);
    return;
  }

  DCHECK_GE(frame_size.width(), 32);
  DCHECK_GE(frame_size.height(), 32);

  // Create or reconfigure the codec to match the size of |frame|.
  if (!codec_ || !frame_size.equals(previous_frame_size)) {
    FreeImageMembers();
    if (!InitializeCodec(frame_size)) {
      std::move(done).Run(EncodeResult::UNKNOWN_ERROR, nullptr);
      return;
    }
  }

  UpdateConfig(params);

  // Transfer the frame data to the image buffer for encoding.
  // TODO(joedow): Look into using aom_img_wrap instead of our own buffer.
  PrepareImage(frame.get());

  auto duration_us = params.duration.InMicroseconds();
  DCHECK_GT(duration_us, 0);
  aom_codec_err_t ret = aom_codec_encode(
      codec_.get(), image_.get(), artificial_timestamp_us_, duration_us,
      (params.key_frame) ? AOM_EFLAG_FORCE_KF : 0);
  artificial_timestamp_us_ += duration_us;
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
  encoded_frame->size = frame_size;
  encoded_frame->codec = webrtc::kVideoCodecAV1;

  while (!got_data) {
    const aom_codec_cx_pkt_t* aom_packet =
        aom_codec_get_cx_data(codec_.get(), &iter);
    if (!aom_packet)
      continue;

    switch (aom_packet->kind) {
      case AOM_CODEC_CX_FRAME_PKT: {
        got_data = true;
        encoded_frame->data.assign(
            reinterpret_cast<const char*>(aom_packet->data.frame.buf),
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
