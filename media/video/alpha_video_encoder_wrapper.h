// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_ALPHA_VIDEO_ENCODER_WRAPPER_H_
#define MEDIA_VIDEO_ALPHA_VIDEO_ENCODER_WRAPPER_H_

#include <memory>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "media/base/media_export.h"
#include "media/base/video_encoder.h"
#include "media/base/video_frame_pool.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"

namespace media {

// A wrapper around video encoder that splits frames with alpha channel between
// two underlying encoders - one for regular pixel data and the other for
// transparency channel.
// Encoded data from both encoders is merged and returned via `output_cb`.
class MEDIA_EXPORT AlphaVideoEncoderWrapper : public VideoEncoder {
 public:
  AlphaVideoEncoderWrapper(std::unique_ptr<VideoEncoder> yuv_encoder,
                           std::unique_ptr<VideoEncoder> alpha_encoder);
  ~AlphaVideoEncoderWrapper() override;

  // VideoEncoder implementation.
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
  void YuvOutputCallback(VideoEncoderOutput output,
                         std::optional<CodecDescription> desc);
  void AlphaOutputCallback(VideoEncoderOutput output,
                           std::optional<CodecDescription> desc);

  std::unique_ptr<VideoEncoder> yuv_encoder_;
  std::unique_ptr<VideoEncoder> alpha_encoder_;
  std::optional<VideoEncoderOutput> yuv_output_;
  std::optional<VideoEncoderOutput> alpha_output_;
  std::optional<EncoderStatus> init_status_;
  std::optional<EncoderStatus> encode_status_;

  std::vector<uint8_t> dummy_uv_planes_;

  OutputCB output_cb_;
  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<AlphaVideoEncoderWrapper> weak_factory_{this};
};

}  // namespace media
#endif  // MEDIA_VIDEO_ALPHA_VIDEO_ENCODER_WRAPPER_H_
