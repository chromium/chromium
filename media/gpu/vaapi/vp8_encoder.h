// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_VP8_ENCODER_H_
#define MEDIA_GPU_VAAPI_VP8_ENCODER_H_

#include <list>
#include <vector>

#include "base/macros.h"
#include "base/sequence_checker.h"
#include "media/base/video_bitrate_allocation.h"
#include "media/gpu/vaapi/accelerated_video_encoder.h"
#include "media/gpu/vp8_picture.h"
#include "media/gpu/vp8_reference_frame_vector.h"
#include "media/parsers/vp8_parser.h"

namespace media {

class VP8Encoder : public AcceleratedVideoEncoder {
 public:
  struct EncodeParams {
    EncodeParams();

    // Produce a keyframe at least once per this many frames.
    size_t kf_period_frames;

    // Bitrate allocation in bps.
    VideoBitrateAllocation bitrate_allocation;

    // Framerate in FPS.
    uint32_t framerate;

    // Bitrate window size in ms.
    unsigned int cpb_window_size_ms;

    // Coded picture buffer size in bits.
    unsigned int cpb_size_bits;

    int initial_qp;
    int min_qp;
    int max_qp;

    bool error_resilient_mode;
  };

  // An accelerator interface. The client must provide an appropriate
  // implementation on creation.
  class Accelerator {
   public:
    Accelerator() = default;
    virtual ~Accelerator() = default;

    // Returns the VP8Picture to be used as output for |job|.
    virtual scoped_refptr<VP8Picture> GetPicture(EncodeJob* job) = 0;

    // Initializes |job| to use the provided |encode_params| as its parameters,
    // and |pic| as the target, as well as |ref_frames| as reference frames for
    // it. |ref_frames_used| specifies which frames in |ref_frames| will be
    // actually used as reference frames on encoding. Returns true on success.
    virtual bool SubmitFrameParameters(
        EncodeJob* job,
        const VP8Encoder::EncodeParams& encode_params,
        scoped_refptr<VP8Picture> pic,
        const Vp8ReferenceFrameVector& ref_frames,
        const std::array<bool, kNumVp8ReferenceBuffers>& ref_frames_used) = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(Accelerator);
  };

  explicit VP8Encoder(std::unique_ptr<Accelerator> accelerator);
  ~VP8Encoder() override;

  // AcceleratedVideoEncoder implementation.
  bool Initialize(const VideoEncodeAccelerator::Config& config,
                  const AcceleratedVideoEncoder::Config& ave_config) override;
  bool UpdateRates(const VideoBitrateAllocation& bitrate_allocation,
                   uint32_t framerate) override;
  gfx::Size GetCodedSize() const override;
  size_t GetMaxNumOfRefFrames() const override;
  bool PrepareEncodeJob(EncodeJob* encode_job) override;

 private:
  void InitializeFrameHeader();
  void UpdateFrameHeader(bool keyframe);
  void UpdateReferenceFrames(scoped_refptr<VP8Picture> picture);
  void Reset();

  gfx::Size visible_size_;
  gfx::Size coded_size_;  // Macroblock-aligned.

  // Frame count since last keyframe, reset to 0 every keyframe period.
  size_t frame_num_ = 0;

  EncodeParams current_params_;

  Vp8FrameHeader current_frame_hdr_;
  Vp8ReferenceFrameVector reference_frames_;

  const std::unique_ptr<Accelerator> accelerator_;

  SEQUENCE_CHECKER(sequence_checker_);
  DISALLOW_COPY_AND_ASSIGN(VP8Encoder);
};

}  // namespace media

#endif  // MEDIA_GPU_VAAPI_VP8_ENCODER_H_
