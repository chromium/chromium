// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CODEC_VIDEO_DECODER_VPX_H_
#define REMOTING_CODEC_VIDEO_DECODER_VPX_H_

#include <memory>

#include "base/compiler_specific.h"
#include "remoting/codec/scoped_vpx_codec.h"
#include "remoting/codec/video_decoder.h"

typedef const struct vpx_codec_iface vpx_codec_iface_t;
typedef struct vpx_image vpx_image_t;

namespace remoting {

class VideoDecoderVpx : public VideoDecoder {
 public:
  // Create decoders for the specified protocol.
  static std::unique_ptr<VideoDecoderVpx> CreateForVP8();
  static std::unique_ptr<VideoDecoderVpx> CreateForVP9();

  VideoDecoderVpx(const VideoDecoderVpx&) = delete;
  VideoDecoderVpx& operator=(const VideoDecoderVpx&) = delete;

  ~VideoDecoderVpx() override;

  // VideoDecoder interface.
  void SetPixelFormat(PixelFormat pixel_format) override;
  bool DecodePacket(const VideoPacket& packet,
                    webrtc::DesktopFrame* frame) override;

 private:
  explicit VideoDecoderVpx(vpx_codec_iface_t* codec);

  ScopedVpxCodec codec_;
  PixelFormat pixel_format_ = PixelFormat::BGRA;
};

}  // namespace remoting

#endif  // REMOTING_CODEC_VIDEO_DECODER_VPX_H_
