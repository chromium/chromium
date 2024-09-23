// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CODEC_WEBRTC_VIDEO_ENCODER_AV1_H_
#define REMOTING_CODEC_WEBRTC_VIDEO_ENCODER_AV1_H_

#include "base/functional/callback.h"
#include "remoting/codec/encoder_bitrate_filter.h"
#include "remoting/codec/video_encoder_active_map.h"
#include "remoting/codec/webrtc_video_encoder.h"
#include "third_party/libaom/source/libaom/aom/aom_encoder.h"
#include "third_party/libaom/source/libaom/aom/aom_image.h"
#include "third_party/libaom/source/libaom/aom/aomcx.h"

namespace webrtc {
class DesktopFrame;
class DesktopRegion;
class DesktopSize;
}  // namespace webrtc

namespace remoting {

// AV1 encoder implementation for WebRTC transport, params are optimized for
// real-time screen sharing.
class WebrtcVideoEncoderAV1 : public WebrtcVideoEncoder {
 public:
  WebrtcVideoEncoderAV1();
  WebrtcVideoEncoderAV1(const WebrtcVideoEncoderAV1&) = delete;
  WebrtcVideoEncoderAV1& operator=(const WebrtcVideoEncoderAV1&) = delete;
  ~WebrtcVideoEncoderAV1() override;

  // WebrtcVideoEncoder interface.
  void SetLosslessColor(bool want_lossless) override;
  void SetUseActiveMap(bool use_active_map) override;
  void SetEncoderSpeed(int encoder_speed) override;
  void Encode(std::unique_ptr<webrtc::DesktopFrame> frame,
              const FrameParams& params,
              EncodeCallback done) override;

 private:
  void ConfigureCodecParams();
  bool InitializeCodec(const webrtc::DesktopSize& size);
  void UpdateConfig(const FrameParams& params);
  void PrepareImage(const webrtc::DesktopFrame* frame,
                    webrtc::DesktopRegion& updated_region);

  using scoped_aom_codec =
      std::unique_ptr<aom_codec_ctx_t, void (*)(aom_codec_ctx_t*)>;
  scoped_aom_codec codec_;

  aom_codec_enc_cfg_t config_ = {};

  using scoped_aom_image = std::unique_ptr<aom_image_t, void (*)(aom_image_t*)>;
  scoped_aom_image image_;

  // Indicates whether the frames provided to the encoder will use I420 (lossy)
  // or I444 (lossless) format.
  bool lossless_color_ = false;
  int av1_encoder_speed_ = -1;

  // An active map is used to skip processing of unchanged macroblocks.
  VideoEncoderActiveMap active_map_data_;
  aom_active_map_t active_map_;
  // Disable |active_map_| by default until we've tuned it.
  bool use_active_map_ = false;

  // This timestamp is monotonically increased using the current frame duration.
  // It's only used for rate control and is not related to the timestamps on the
  // incoming frames to encode.
  aom_codec_pts_t artificial_timestamp_ms_ = 0;

  EncoderBitrateFilter bitrate_filter_;
};

}  // namespace remoting

#endif  // REMOTING_CODEC_WEBRTC_VIDEO_ENCODER_AV1_H_
