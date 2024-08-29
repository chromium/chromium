// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CODEC_VIDEO_ENCODER_VPX_H_
#define REMOTING_CODEC_VIDEO_ENCODER_VPX_H_

#include <stdint.h>

#include "base/containers/heap_array.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "remoting/codec/scoped_vpx_codec.h"
#include "remoting/codec/video_encoder.h"
#include "remoting/codec/video_encoder_helper.h"

typedef struct vpx_image vpx_image_t;

namespace webrtc {
class DesktopRegion;
class DesktopSize;
}  // namespace webrtc

namespace remoting {

class VideoEncoderVpx : public VideoEncoder {
 public:
  // Create encoder for the specified protocol.
  static std::unique_ptr<VideoEncoderVpx> CreateForVP8();
  static std::unique_ptr<VideoEncoderVpx> CreateForVP9();

  VideoEncoderVpx(const VideoEncoderVpx&) = delete;
  VideoEncoderVpx& operator=(const VideoEncoderVpx&) = delete;

  ~VideoEncoderVpx() override;

  void SetTickClockForTests(const base::TickClock* tick_clock);

  // VideoEncoder interface.
  void SetLosslessColor(bool want_lossless) override;
  std::unique_ptr<VideoPacket> Encode(
      const webrtc::DesktopFrame& frame) override;

 private:
  explicit VideoEncoderVpx(bool use_vp9);

  // (Re)Configures this instance to encode frames of the specified |size|,
  // with the configured lossless color mode.
  void Configure(const webrtc::DesktopSize& size);

  // Prepares |image_| for encoding. Writes updated rectangles into
  // |updated_region|.
  void PrepareImage(const webrtc::DesktopFrame& frame,
                    webrtc::DesktopRegion* updated_region);

  // Updates the active map according to |updated_region|. Active map is then
  // given to the encoder to speed up encoding.
  void SetActiveMapFromRegion(const webrtc::DesktopRegion& updated_region);

  // Adds areas changed in the most recent frame to |updated_region|. This
  // includes both content changes and areas enhanced by cyclic refresh.
  void UpdateRegionFromActiveMap(webrtc::DesktopRegion* updated_region);

  // True if the encoder is for VP9, false for VP8.
  const bool use_vp9_;

  // Option controlling VP9 color space, this is always off (false) for VP8.
  bool lossless_color_ = false;

  // Holds the initialized & configured codec.
  ScopedVpxCodec codec_;

  // Used to generate zero-based frame timestamps.
  base::TimeTicks timestamp_base_;

  // VPX image and buffer to hold the actual YUV planes.
  std::unique_ptr<vpx_image_t> image_;
  base::HeapArray<uint8_t> image_buffer_;

  // Active map used to optimize out processing of un-changed macroblocks.
  std::unique_ptr<uint8_t[]> active_map_;
  webrtc::DesktopSize active_map_size_;

  // True if the codec wants unchanged frames to finish topping-off with.
  bool encode_unchanged_frame_;

  // Used to help initialize VideoPackets from DesktopFrames.
  VideoEncoderHelper helper_;

  raw_ptr<const base::TickClock> clock_;
};

}  // namespace remoting

#endif  // REMOTING_CODEC_VIDEO_ENCODER_VPX_H_
