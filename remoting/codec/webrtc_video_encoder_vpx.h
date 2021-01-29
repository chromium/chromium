// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CODEC_WEBRTC_VIDEO_ENCODER_VPX_H_
#define REMOTING_CODEC_WEBRTC_VIDEO_ENCODER_VPX_H_

#include <stdint.h>

#include "base/callback.h"
#include "base/macros.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "remoting/codec/encoder_bitrate_filter.h"
#include "remoting/codec/scoped_vpx_codec.h"
#include "remoting/codec/webrtc_video_encoder.h"
#include "remoting/codec/webrtc_video_encoder_selector.h"
#include "third_party/libvpx/source/libvpx/vpx/vpx_encoder.h"

typedef struct vpx_image vpx_image_t;

namespace webrtc {
class DesktopRegion;
class DesktopSize;
}  // namespace webrtc

namespace remoting {

// This is a copy of VideoEncoderVpx with enhancements to encoder for use
// over WebRTC as transport. The original VideoEncoderVpx should be deleted
// once the old implementation is no longer in use.
class WebrtcVideoEncoderVpx : public WebrtcVideoEncoder {
 public:
  // Creates encoder for the specified protocol.
  static std::unique_ptr<WebrtcVideoEncoder> CreateForVP8();
  static std::unique_ptr<WebrtcVideoEncoder> CreateForVP9();

  // Checks the support for the specified protocol.
  static bool IsSupportedByVP8(
      const WebrtcVideoEncoderSelector::Profile& profile);
  static bool IsSupportedByVP9(
      const WebrtcVideoEncoderSelector::Profile& profile);

  ~WebrtcVideoEncoderVpx() override;

  void SetTickClockForTests(const base::TickClock* tick_clock);

  // WebrtcVideoEncoder interface.
  void SetLosslessEncode(bool want_lossless) override;
  void SetLosslessColor(bool want_lossless) override;
  void Encode(std::unique_ptr<webrtc::DesktopFrame> frame,
              const FrameParams& params,
              EncodeCallback done) override;

 private:
  explicit WebrtcVideoEncoderVpx(bool use_vp9);

  // (Re)Configures this instance to encode frames of the specified |size|,
  // with the configured lossless color & encoding modes.
  void Configure(const webrtc::DesktopSize& size);

  // Updates codec configuration.
  void UpdateConfig(const FrameParams& params);

  // Prepares |image_| for encoding. Writes updated rectangles into
  // |updated_region|.
  void PrepareImage(const webrtc::DesktopFrame* frame,
                    webrtc::DesktopRegion* updated_region);

  // Clears active map.
  void ClearActiveMap();

  // Updates the active map according to |updated_region|. Active map is then
  // given to the encoder to speed up encoding.
  void SetActiveMapFromRegion(const webrtc::DesktopRegion& updated_region);

  // Adds areas changed in the most recent frame to |updated_region|. This
  // includes both content changes and areas enhanced by cyclic refresh.
  void UpdateRegionFromActiveMap(webrtc::DesktopRegion* updated_region);

  // True if the encoder is for VP9, false for VP8.
  const bool use_vp9_;

  // Options controlling VP9 encode quantization and color space.
  // These are always off (false) for VP8.
  bool lossless_encode_ = false;
  bool lossless_color_ = false;

  // Holds the initialized & configured codec.
  ScopedVpxCodec codec_;

  vpx_codec_enc_cfg_t config_;

  // Used to generate zero-based frame timestamps.
  base::TimeTicks timestamp_base_;

  // VPX image and buffer to hold the actual YUV planes.
  std::unique_ptr<vpx_image_t> image_;
  std::unique_ptr<uint8_t[]> image_buffer_;

  // Active map used to optimize out processing of un-changed macroblocks.
  std::unique_ptr<uint8_t[]> active_map_;
  webrtc::DesktopSize active_map_size_;

  const base::TickClock* clock_;

  EncoderBitrateFilter bitrate_filter_;

  DISALLOW_COPY_AND_ASSIGN(WebrtcVideoEncoderVpx);
};

}  // namespace remoting

#endif  // REMOTING_CODEC_WEBRTC_VIDEO_ENCODER_VPX_H_
