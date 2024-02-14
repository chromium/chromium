// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_AV1_VIDEO_ENCODER_H_
#define MEDIA_VIDEO_AV1_VIDEO_ENCODER_H_

#include <memory>
#include <vector>

#include "base/time/time.h"
#include "media/base/media_export.h"
#include "media/base/video_encoder.h"
#include "media/base/video_frame_converter.h"
#include "media/base/video_frame_pool.h"
#include "third_party/libaom/source/libaom/aom/aom_encoder.h"
#include "third_party/libaom/source/libaom/aom/aomcx.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"

namespace media {

class MEDIA_EXPORT Av1VideoEncoder : public VideoEncoder {
 public:
  Av1VideoEncoder();
  ~Av1VideoEncoder() override;

  // VideoDecoder implementation.
  void Initialize(VideoCodecProfile profile,
                  const Options& options,
                  EncoderInfoCB info_cb,
                  OutputCB output_cb,
                  EncoderStatusCB done_cb) override;
  void Encode(scoped_refptr<VideoFrame> frame,
              const EncodeOptions& encode_options,
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
  EncoderStatus::Or<int> AssignNextTemporalId(bool key_frame);
  void UpdateEncoderColorSpace();

  using aom_codec_unique_ptr =
      std::unique_ptr<aom_codec_ctx_t, void (*)(aom_codec_ctx_t*)>;

  aom_codec_unique_ptr codec_;
  aom_codec_enc_cfg_t config_ = {};
  aom_svc_params_t svc_params_ = {};
  aom_image_t image_ = {};

  // This is a timestamp that is always increasing by frame's duration.
  // It's used only for rate control and has nothing to do with timestamps
  // coming from real frames.
  aom_codec_pts_t artificial_timestamp_ = 0;

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
#endif  // MEDIA_VIDEO_AV1_VIDEO_ENCODER_H_
