// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_VPX_VIDEO_ENCODER_H_
#define MEDIA_VIDEO_VPX_VIDEO_ENCODER_H_

#include <memory>

#include "base/callback_forward.h"
#include "media/base/media_export.h"
#include "media/base/video_encoder.h"
#include "third_party/libvpx/source/libvpx/vpx/vpx_encoder.h"
#include "ui/gfx/geometry/size.h"

namespace media {

class MEDIA_EXPORT VpxVideoEncoder : public VideoEncoder {
 public:
  VpxVideoEncoder();
  ~VpxVideoEncoder() override;

  // VideoDecoder implementation.
  void Initialize(VideoCodecProfile profile,
                  const Options& options,
                  OutputCB output_cb,
                  StatusCB done_cb) override;
  void Encode(scoped_refptr<VideoFrame> frame,
              bool key_frame,
              StatusCB done_cb) override;
  void ChangeOptions(const Options& options, StatusCB done_cb) override;
  void Flush(StatusCB done_cb) override;

 private:
  uint64_t GetFrameDuration(const VideoFrame& frame);
  void DrainOutputs();

  using vpx_codec_unique_ptr =
      std::unique_ptr<vpx_codec_ctx_t, void (*)(vpx_codec_ctx_t*)>;

  vpx_codec_unique_ptr codec_;
  bool is_vp9_ = false;
  vpx_codec_enc_cfg_t codec_config_ = {};
  vpx_image_t vpx_image_ = {};
  VideoCodecProfile profile_ = VIDEO_CODEC_PROFILE_UNKNOWN;
  Options options_;
  OutputCB output_cb_;
};

}  // namespace media
#endif  // MEDIA_VIDEO_VPX_VIDEO_ENCODER_H_