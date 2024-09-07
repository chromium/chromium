// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "remoting/codec/video_decoder_vpx.h"

#include <math.h>
#include <stdint.h>

#include "base/check_op.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "remoting/base/util.h"
#include "remoting/proto/video.pb.h"
#include "third_party/libvpx/source/libvpx/vpx/vp8dx.h"
#include "third_party/libvpx/source/libvpx/vpx/vpx_decoder.h"
#include "third_party/libyuv/include/libyuv/convert_argb.h"
#include "third_party/libyuv/include/libyuv/convert_from.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_region.h"

namespace remoting {

namespace {

void RenderRect(vpx_image_t* image,
                webrtc::DesktopRect rect,
                VideoDecoder::PixelFormat pixel_format,
                webrtc::DesktopFrame* frame) {
  auto yuv_to_rgb_function = libyuv::I420ToARGB;
  int u_offset;
  int v_offset;

  switch (image->fmt) {
    case VPX_IMG_FMT_I420: {
      // Align position of the top left corner so that its coordinates are
      // always even.
      rect = webrtc::DesktopRect::MakeLTRB(rect.left() & ~1, rect.top() & ~1,
                                           rect.right(), rect.bottom());
      u_offset = rect.top() / 2 * image->stride[1] + rect.left() / 2;
      v_offset = rect.top() / 2 * image->stride[2] + rect.left() / 2;
      yuv_to_rgb_function = (pixel_format == VideoDecoder::PixelFormat::BGRA)
                                ? libyuv::I420ToARGB
                                : libyuv::I420ToABGR;
      break;
    }
    // VP8 only outputs I420 frames, but VP9 can also produce I444.
    case VPX_IMG_FMT_I444: {
      u_offset = rect.top() * image->stride[1] + rect.left();
      v_offset = rect.top() * image->stride[2] + rect.left();
      yuv_to_rgb_function = (pixel_format == VideoDecoder::PixelFormat::BGRA)
                                ? libyuv::I444ToARGB
                                : libyuv::I444ToABGR;
      break;
    }
    default: {
      LOG(ERROR) << "Unsupported image format:" << image->fmt;
      return;
    }
  }

  int y_offset = rect.top() * image->stride[0] + rect.left();
  uint8_t* image_data_ptr = frame->GetFrameDataAtPos(rect.top_left());
  yuv_to_rgb_function(image->planes[0] + y_offset, image->stride[0],
                      image->planes[1] + u_offset, image->stride[1],
                      image->planes[2] + v_offset, image->stride[2],
                      image_data_ptr, frame->stride(), rect.width(),
                      rect.height());
}

}  // namespace

// static
std::unique_ptr<VideoDecoderVpx> VideoDecoderVpx::CreateForVP8() {
  return base::WrapUnique(new VideoDecoderVpx(vpx_codec_vp8_dx()));
}

// static
std::unique_ptr<VideoDecoderVpx> VideoDecoderVpx::CreateForVP9() {
  return base::WrapUnique(new VideoDecoderVpx(vpx_codec_vp9_dx()));
}

VideoDecoderVpx::~VideoDecoderVpx() = default;

void VideoDecoderVpx::SetPixelFormat(PixelFormat pixel_format) {
  pixel_format_ = pixel_format;
}

bool VideoDecoderVpx::DecodePacket(const VideoPacket& packet,
                                   webrtc::DesktopFrame* frame) {
  // Pass the packet to the codec to process.
  vpx_codec_err_t ret = vpx_codec_decode(
      codec_.get(), reinterpret_cast<const uint8_t*>(packet.data().data()),
      packet.data().size(), nullptr, 0);
  if (ret != VPX_CODEC_OK) {
    const char* error = vpx_codec_error(codec_.get());
    const char* error_detail = vpx_codec_error_detail(codec_.get());
    LOG(ERROR) << "Decoding failed:" << (error ? error : "(NULL)") << "\n"
               << "Details: " << (error_detail ? error_detail : "(NULL)");
    return false;
  }

  // Fetch the decoded video frame.
  vpx_codec_iter_t iter = nullptr;
  vpx_image_t* image = vpx_codec_get_frame(codec_.get(), &iter);
  if (!image) {
    LOG(ERROR) << "No video frame decoded.";
    return false;
  }
  if (!webrtc::DesktopSize(image->d_w, image->d_h).equals(frame->size())) {
    LOG(ERROR) << "Size of the encoded frame doesn't match size in the header.";
    return false;
  }

  // Determine which areas have been updated.
  webrtc::DesktopRegion* region = frame->mutable_updated_region();
  region->Clear();
  for (int i = 0; i < packet.dirty_rects_size(); ++i) {
    Rect proto_rect = packet.dirty_rects(i);
    webrtc::DesktopRect rect =
        webrtc::DesktopRect::MakeXYWH(proto_rect.x(), proto_rect.y(),
                                      proto_rect.width(), proto_rect.height());
    region->AddRect(rect);
    RenderRect(image, rect, pixel_format_, frame);
  }

  return true;
}

VideoDecoderVpx::VideoDecoderVpx(vpx_codec_iface_t* codec) {
  codec_.reset(new vpx_codec_ctx_t);

  vpx_codec_dec_cfg config;
  config.w = 0;
  config.h = 0;
  config.threads = 2;
  vpx_codec_err_t ret = vpx_codec_dec_init(codec_.get(), codec, &config, 0);
  CHECK_EQ(VPX_CODEC_OK, ret);
}

}  // namespace remoting
