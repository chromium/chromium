// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_VP9_ENCODER_H_
#define MEDIA_GPU_VAAPI_VP9_ENCODER_H_

#include <list>
#include <memory>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/sequence_checker.h"
#include "media/base/video_bitrate_allocation.h"
#include "media/filters/vp9_parser.h"
#include "media/gpu/vaapi/accelerated_video_encoder.h"
#include "media/gpu/vp9_picture.h"
#include "media/gpu/vp9_reference_frame_vector.h"

namespace media {

class VP9Encoder : public AcceleratedVideoEncoder {
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

    // Returns the VP9Picture to be used as output for |job|.
    virtual scoped_refptr<VP9Picture> GetPicture(EncodeJob* job) = 0;

    // Initializes |job| to use the provided |encode_params| as its parameters,
    // and |pic| as the target, as well as |ref_frames| as reference frames for
    // it. |ref_frames_used| is to specify whether each of |ref_frame_idx| of
    // VP9FrameHeader in |pic| is used. If |ref_frames_used[i]| is true,
    // ref_frame_idx[i] will be used as a reference frame. Returns true on
    // success.
    virtual bool SubmitFrameParameters(
        EncodeJob* job,
        const VP9Encoder::EncodeParams& encode_params,
        scoped_refptr<VP9Picture> pic,
        const Vp9ReferenceFrameVector& ref_frames,
        const std::array<bool, kVp9NumRefsPerFrame>& ref_frames_used) = 0;

    DISALLOW_COPY_AND_ASSIGN(Accelerator);
  };

  explicit VP9Encoder(std::unique_ptr<Accelerator> accelerator);
  ~VP9Encoder() override;

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
  void UpdateReferenceFrames(scoped_refptr<VP9Picture> picture);
  void Reset();

  gfx::Size visible_size_;
  gfx::Size coded_size_;  // Macroblock-aligned.

  // Frame count since last keyframe, reset to 0 every keyframe period.
  size_t frame_num_ = 0;
  size_t ref_frame_index_ = 0;

  EncodeParams current_params_;

  Vp9FrameHeader current_frame_hdr_;
  Vp9ReferenceFrameVector reference_frames_;

  const std::unique_ptr<Accelerator> accelerator_;

  SEQUENCE_CHECKER(sequence_checker_);
  DISALLOW_COPY_AND_ASSIGN(VP9Encoder);
};

}  // namespace media

#endif  // MEDIA_GPU_VAAPI_VP9_ENCODER_H_
