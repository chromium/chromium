// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CODEC_WEBRTC_VIDEO_ENCODER_VPX_H_
#define REMOTING_CODEC_WEBRTC_VIDEO_ENCODER_VPX_H_

#include <stdint.h>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "remoting/codec/encoder_bitrate_filter.h"
#include "remoting/codec/scoped_vpx_codec.h"
#include "remoting/codec/video_encoder_active_map.h"
#include "remoting/codec/webrtc_video_encoder.h"
#include "third_party/libvpx/source/libvpx/vpx/vp8cx.h"
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

  WebrtcVideoEncoderVpx(const WebrtcVideoEncoderVpx&) = delete;
  WebrtcVideoEncoderVpx& operator=(const WebrtcVideoEncoderVpx&) = delete;

  ~WebrtcVideoEncoderVpx() override;

  void SetTickClockForTests(const base::TickClock* tick_clock);

  // WebrtcVideoEncoder interface.
  void SetLosslessColor(bool want_lossless) override;
  void SetEncoderSpeed(int encoder_speed) override;
  void Encode(std::unique_ptr<webrtc::DesktopFrame> frame,
              const FrameParams& params,
              EncodeCallback done) override;

 private:
  explicit WebrtcVideoEncoderVpx(bool use_vp9);

  // (Re)Configures this instance to encode frames of the specified |size|,
  // with the configured lossless color mode.
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

  // Controls VP9 color space and encode speed. Not used when configuring VP8.
  bool lossless_color_ = false;
  int vp9_encoder_speed_ = -1;

  // Holds the initialized & configured codec.
  ScopedVpxCodec codec_;

  vpx_codec_enc_cfg_t config_;

  // Used to generate zero-based frame timestamps.
  base::TimeTicks timestamp_base_;

  // vpx_image_t has a custom deallocator which needs to be called before
  // deletion.
  using scoped_vpx_image = std::unique_ptr<vpx_image_t, void (*)(vpx_image_t*)>;

  // VPX image descriptor and pixel buffer.
  scoped_vpx_image image_;

  // An active map is used to skip processing of unchanged macroblocks.
  VideoEncoderActiveMap active_map_data_;
  vpx_active_map_t active_map_;
  // TODO(joedow): Remove this flag after we're done with performance tuning.
  const bool use_active_map_ = true;

  raw_ptr<const base::TickClock> clock_;

  EncoderBitrateFilter bitrate_filter_;
};

}  // namespace remoting

#endif  // REMOTING_CODEC_WEBRTC_VIDEO_ENCODER_VPX_H_
