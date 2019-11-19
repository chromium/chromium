// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_H264_ENCODER_H_
#define MEDIA_GPU_VAAPI_H264_ENCODER_H_

#include <stddef.h>
#include <list>

#include "base/macros.h"
#include "base/sequence_checker.h"
#include "media/filters/h264_bitstream_buffer.h"
#include "media/gpu/h264_dpb.h"
#include "media/gpu/vaapi/accelerated_video_encoder.h"

namespace media {

// This class provides an H264 encoder functionality, generating stream headers,
// managing encoder state, reference frames, and other codec parameters, while
// requiring support from an Accelerator to encode frame data based on these
// parameters.
//
// This class must be created, called and destroyed on a single sequence.
//
// Names used in documentation of this class refer directly to naming used
// in the H.264 specification (http://www.itu.int/rec/T-REC-H.264).
class H264Encoder : public AcceleratedVideoEncoder {
 public:
  struct EncodeParams {
    EncodeParams();

    // Produce an IDR at least once per this many frames.
    // Must be >= 16 (per spec).
    size_t idr_period_frames;

    // Produce an I frame at least once per this many frames.
    size_t i_period_frames;

    // How often do we need to have either an I or a P frame in the stream.
    // A period of 1 implies no B frames.
    size_t ip_period_frames;

    // Bitrate in bps.
    uint32_t bitrate_bps;

    // Framerate in FPS.
    uint32_t framerate;

    // Bitrate window size in ms.
    unsigned int cpb_window_size_ms;

    // Bitrate window size in bits.
    unsigned int cpb_size_bits;

    // Quantization parameter.
    int qp;

    // Maxium Number of Reference frames.
    size_t max_num_ref_frames;

    // Maximum size of reference picture list 0.
    size_t max_ref_pic_list0_size;

    // Maximum size of reference picture list 1.
    size_t max_ref_pic_list1_size;
  };

  // An accelerator interface. The client must provide an appropriate
  // implementation on creation.
  class Accelerator {
   public:
    Accelerator() = default;
    virtual ~Accelerator();

    // Returns the H264Picture to be used as output for |job|.
    virtual scoped_refptr<H264Picture> GetPicture(EncodeJob* job) = 0;

    // Initializes |job| to insert the provided |packed_sps| and |packed_pps|
    // before the frame produced by |job| into the output video stream.
    virtual bool SubmitPackedHeaders(
        EncodeJob* job,
        scoped_refptr<H264BitstreamBuffer> packed_sps,
        scoped_refptr<H264BitstreamBuffer> packed_pps) = 0;

    // Initializes |job| to use the provided |sps|, |pps|, |encode_params|, and
    // encoded picture parameters in |pic|, as well as |ref_pic_list0| and
    // |ref_pic_list1| as the corresponding H264 reference frame lists
    // (RefPicList0 and RefPicList1 per spec) for the frame to be produced.
    virtual bool SubmitFrameParameters(
        EncodeJob* job,
        const H264Encoder::EncodeParams& encode_params,
        const H264SPS& sps,
        const H264PPS& pps,
        scoped_refptr<H264Picture> pic,
        const std::list<scoped_refptr<H264Picture>>& ref_pic_list0,
        const std::list<scoped_refptr<H264Picture>>& ref_pic_list1) = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(Accelerator);
  };

  explicit H264Encoder(std::unique_ptr<Accelerator> accelerator);
  ~H264Encoder() override;

  // AcceleratedVideoEncoder implementation.
  bool Initialize(const VideoEncodeAccelerator::Config& config,
                  const AcceleratedVideoEncoder::Config& ave_config) override;
  bool UpdateRates(const VideoBitrateAllocation& bitrate_allocation,
                   uint32_t framerate) override;
  gfx::Size GetCodedSize() const override;
  size_t GetMaxNumOfRefFrames() const override;
  bool PrepareEncodeJob(EncodeJob* encode_job) override;

 private:
  friend class H264EncoderTest;

  // Fill current_sps_ and current_pps_ with current encoding state parameters.
  void UpdateSPS();
  void UpdatePPS();

  // Generate packed SPS and PPS in packed_sps_ and packed_pps_, using values
  // in current_sps_ and current_pps_.
  void GeneratePackedSPS();
  void GeneratePackedPPS();

  // Check if |bitrate| and |framerate| and current coded size are supported by
  // current profile and level.
  bool CheckConfigValidity(uint32_t bitrate, uint32_t framerate);

  // Current SPS, PPS and their packed versions. Packed versions are NALUs
  // in AnnexB format *without* emulation prevention three-byte sequences
  // (those are expected to be added by the client as needed).
  H264SPS current_sps_;
  scoped_refptr<H264BitstreamBuffer> packed_sps_;
  H264PPS current_pps_;
  scoped_refptr<H264BitstreamBuffer> packed_pps_;

  // Current encoding parameters being used.
  EncodeParams curr_params_;

  // H264 profile currently used.
  VideoCodecProfile profile_ = VIDEO_CODEC_PROFILE_UNKNOWN;

  // H264 level currently used.
  uint8_t level_ = 0;

  // Current visible and coded sizes in pixels.
  gfx::Size visible_size_;
  gfx::Size coded_size_;

  // Width/height in macroblocks.
  unsigned int mb_width_ = 0;
  unsigned int mb_height_ = 0;

  // frame_num (spec section 7.4.3) to be used for the next frame.
  unsigned int frame_num_ = 0;

  // idr_pic_id (spec section 7.4.3) to be used for the next frame.
  unsigned int idr_pic_id_ = 0;

  // True if encoding parameters have changed that affect decoder process, then
  // we need to submit a keyframe with updated parameters.
  bool encoding_parameters_changed_ = false;

  // Currently active reference frames.
  // RefPicList0 per spec (spec section 8.2.4.2).
  std::list<scoped_refptr<H264Picture>> ref_pic_list0_;

  // Accelerator instance used to prepare encode jobs.
  const std::unique_ptr<Accelerator> accelerator_;

  SEQUENCE_CHECKER(sequence_checker_);
  DISALLOW_COPY_AND_ASSIGN(H264Encoder);
};

}  // namespace media

#endif  // MEDIA_GPU_VAAPI_H264_ENCODER_H_
