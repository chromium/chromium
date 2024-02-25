// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_OPENH264_VIDEO_ENCODER_H_
#define MEDIA_VIDEO_OPENH264_VIDEO_ENCODER_H_

#include <memory>
#include <vector>

#include "media/base/media_export.h"
#include "media/base/video_encoder.h"
#include "media/base/video_frame_converter.h"
#include "media/base/video_frame_pool.h"
#include "media/formats/mp4/h264_annex_b_to_avc_bitstream_converter.h"
#include "third_party/openh264/src/codec/api/wels/codec_api.h"
#include "ui/gfx/geometry/size.h"

namespace media {

class MEDIA_EXPORT OpenH264VideoEncoder : public VideoEncoder {
 public:
  OpenH264VideoEncoder();
  ~OpenH264VideoEncoder() override;

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
  EncoderStatus DrainOutputs(const SFrameBSInfo& frame_info,
                             base::TimeDelta timestamp,
                             gfx::ColorSpace color_space);
  void UpdateEncoderColorSpace();

  class ISVCEncoderDeleter {
   public:
    ISVCEncoderDeleter();
    ISVCEncoderDeleter(const ISVCEncoderDeleter&);
    ISVCEncoderDeleter& operator=(const ISVCEncoderDeleter&);
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
  VideoFramePool frame_pool_;
  VideoFrameConverter frame_converter_;
  gfx::ColorSpace last_frame_color_space_;

  // If `h264_converter_` is null, we output in annexb format. Otherwise, we
  // output in avc format and `conversion_buffer_` is used for temporary space.
  std::vector<uint8_t> conversion_buffer_;
  std::unique_ptr<H264AnnexBToAvcBitstreamConverter> h264_converter_;
};

}  // namespace media
#endif  // MEDIA_VIDEO_OPENH264_VIDEO_ENCODER_H_
