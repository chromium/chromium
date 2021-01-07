// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_OPENH264_VIDEO_ENCODER_H_
#define MEDIA_VIDEO_OPENH264_VIDEO_ENCODER_H_

#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "media/base/media_export.h"
#include "media/base/video_encoder.h"
#include "media/base/video_frame_pool.h"
#include "media/formats/mp4/h264_annex_b_to_avc_bitstream_converter.h"
#include "third_party/openh264/src/codec/api/svc/codec_api.h"
#include "ui/gfx/geometry/size.h"

namespace media {

class MEDIA_EXPORT OpenH264VideoEncoder : public VideoEncoder {
 public:
  OpenH264VideoEncoder();
  ~OpenH264VideoEncoder() override;

  // VideoDecoder implementation.
  void Initialize(VideoCodecProfile profile,
                  const Options& options,
                  OutputCB output_cb,
                  StatusCB done_cb) override;
  void Encode(scoped_refptr<VideoFrame> frame,
              bool key_frame,
              StatusCB done_cb) override;
  void ChangeOptions(const Options& options,
                     OutputCB output_cb,
                     StatusCB done_cb) override;
  void Flush(StatusCB done_cb) override;

 private:
  void DrainOutputs();

  class ISVCEncoderDeleter {
   public:
    ISVCEncoderDeleter();
    ISVCEncoderDeleter(const ISVCEncoderDeleter&);
    void operator()(ISVCEncoder* coder);
    void MarkInitialized();

   private:
    bool initialized_ = false;
  };

  using svc_encoder_unique_ptr =
      std::unique_ptr<ISVCEncoder, ISVCEncoderDeleter>;

  svc_encoder_unique_ptr codec_;
  VideoCodecProfile profile_ = VIDEO_CODEC_PROFILE_UNKNOWN;
  Options options_;
  OutputCB output_cb_;
  std::vector<uint8_t> conversion_buffer_;
  VideoFramePool frame_pool_;

  // If |h264_converter_| is null, we output in annexb format. Otherwise, we
  // output in avc format.
  std::unique_ptr<H264AnnexBToAvcBitstreamConverter> h264_converter_;
};

}  // namespace media
#endif  // MEDIA_VIDEO_OPENH264_VIDEO_ENCODER_H_
