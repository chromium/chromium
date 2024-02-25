// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_VPX_VIDEO_ENCODER_H_
#define MEDIA_VIDEO_VPX_VIDEO_ENCODER_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/time/time.h"
#include "media/base/media_export.h"
#include "media/base/video_encoder.h"
#include "media/base/video_frame_converter.h"
#include "media/base/video_frame_pool.h"
#include "third_party/libvpx/source/libvpx/vpx/vpx_encoder.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"

namespace media {

class MEDIA_EXPORT VpxVideoEncoder : public VideoEncoder {
 public:
  VpxVideoEncoder();
  ~VpxVideoEncoder() override;

  // VideoDecoder implementation.
  void Initialize(VideoCodecProfile profile,
                  const Options& options,
                  EncoderInfoCB info_cb,
                  OutputCB output_cb,
                  EncoderStatusCB done_cb) override;
  void Encode(scoped_refptr<VideoFrame> frame,
              const EncodeOptions& options,
              EncoderStatusCB done_cb) override;
  void ChangeOptions(const Options& options,
                     OutputCB output_cb,
                     EncoderStatusCB done_cb) override;
  void Flush(EncoderStatusCB done_cb) override;

 private:
  base::TimeDelta GetFrameDuration(const VideoFrame& frame);
  VideoEncoderOutput GetEncoderOutput(int temporal_id,
                                      base::TimeDelta timestamp,
                                      gfx::ColorSpace color_space) const;
  void RecreateVpxImageIfNeeded(vpx_img_fmt fmt, bool needs_memory);
  void UpdateEncoderColorSpace();

  using vpx_codec_unique_ptr =
      std::unique_ptr<vpx_codec_ctx_t, void (*)(vpx_codec_ctx_t*)>;

  vpx_codec_unique_ptr codec_;
  vpx_codec_enc_cfg_t codec_config_ = {};
  vpx_image_t vpx_image_ = {};
  gfx::Size originally_configured_size_;
  base::TimeDelta last_frame_timestamp_;
  gfx::ColorSpace last_frame_color_space_;
  unsigned int temporal_svc_frame_index_ = 0;
  VideoCodecProfile profile_ = VIDEO_CODEC_PROFILE_UNKNOWN;
  VideoFramePool frame_pool_;
  VideoFrameConverter frame_converter_;
  Options options_;
  OutputCB output_cb_;
};

}  // namespace media
#endif  // MEDIA_VIDEO_VPX_VIDEO_ENCODER_H_
