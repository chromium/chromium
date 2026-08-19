// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_MAC_VIDEO_TOOLBOX_H265_ACCELERATOR_H_
#define MEDIA_GPU_MAC_VIDEO_TOOLBOX_H265_ACCELERATOR_H_

#include <CoreMedia/CoreMedia.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/apple/scoped_cftyperef.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/sequence_checker.h"
#include "media/base/video_types.h"
#include "media/gpu/h265_decoder.h"
#include "media/gpu/mac/video_toolbox_decompression_metadata.h"
#include "media/gpu/mac/video_toolbox_nalu_util.h"
#include "media/gpu/media_gpu_export.h"

namespace media {

class MediaLog;

class MEDIA_GPU_EXPORT VideoToolboxH265Accelerator
    : public H265Decoder::H265Accelerator {
 public:
  using DecodeCB = base::RepeatingCallback<void(
      base::apple::ScopedCFTypeRef<CMSampleBufferRef>,
      VideoToolboxDecompressionSessionMetadata,
      scoped_refptr<CodecPicture>)>;
  using OutputCB = base::RepeatingCallback<void(scoped_refptr<CodecPicture>)>;

  VideoToolboxH265Accelerator(std::unique_ptr<MediaLog> media_log,
                              DecodeCB decode_cb,
                              OutputCB output_cb);
  ~VideoToolboxH265Accelerator() override;

  // H265Accelerator implementation.
  scoped_refptr<H265Picture> CreateH265Picture() override;
  void ProcessVPS(const H265VPS* vps,
                  base::span<const uint8_t> vps_nalu_data) override;
  void ProcessSPS(const H265SPS* sps,
                  base::span<const uint8_t> sps_nalu_data) override;
  void ProcessPPS(const H265PPS* pps,
                  base::span<const uint8_t> pps_nalu_data) override;
  Status SubmitFrameMetadata(
      const H265SPS* sps,
      const H265PPS* pps,
      const H265SliceHeader* slice_hdr,
      const H265Picture::Vector& ref_pic_list,
      const H265Picture::Vector& ref_pic_set_lt_curr,
      const H265Picture::Vector& ref_pic_set_st_curr_after,
      const H265Picture::Vector& ref_pic_set_st_curr_before,
      scoped_refptr<H265Picture> pic) override;
  Status SubmitSlice(const H265SPS* sps,
                     const H265PPS* pps,
                     const H265SliceHeader* slice_hdr,
                     const H265Picture::Vector& ref_pic_list0,
                     const H265Picture::Vector& ref_pic_list1,
                     const H265Picture::Vector& ref_pic_set_lt_curr,
                     const H265Picture::Vector& ref_pic_set_st_curr_after,
                     const H265Picture::Vector& ref_pic_set_st_curr_before,
                     scoped_refptr<H265Picture> pic,
                     const uint8_t* data,
                     size_t size,
                     const std::vector<SubsampleEntry>& subsamples) override;
  Status SubmitDecode(scoped_refptr<H265Picture> pic) override;
  bool OutputPicture(scoped_refptr<H265Picture> pic) override;
  void Reset() override;
  bool IsChromaSamplingSupported(VideoChromaSampling format) override;
  bool IsAlphaLayerSupported() override;

 private:
  [[nodiscard]] bool CreateFormat(scoped_refptr<H265Picture> pic);
  void ResetFrameData();

  std::unique_ptr<MediaLog> media_log_;

  // Callbacks are called synchronously, which is always re-entrant.
  DecodeCB decode_cb_;
  OutputCB output_cb_;

  // Trackers for observed, active, and per-frame referenced parameter sets.
  VideoToolboxParameterSetTracker vps_tracker_;
  VideoToolboxParameterSetTracker sps_tracker_;
  VideoToolboxParameterSetTracker pps_tracker_;

  // Cached parameter values.
  base::flat_set<int> alpha_vps_ids_;

  // Format description and session metadata created from active parameter
  // sets and frame configurations.
  base::apple::ScopedCFTypeRef<CMFormatDescriptionRef> active_format_;
  VideoToolboxDecompressionSessionMetadata active_session_metadata_;

  // Accumulated slice data and format properties for the current frame.
  // TODO(367764863) Rewrite to base::raw_span.
  RAW_PTR_EXCLUSION std::vector<base::span<const uint8_t>> frame_slice_data_;
  uint8_t frame_bit_depth_ = 8;
  VideoChromaSampling frame_chroma_sampling_ = VideoChromaSampling::k420;
  bool frame_is_keyframe_ = false;
  bool frame_has_alpha_ = false;
  bool drop_frame_ = false;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace media

#endif  // MEDIA_GPU_MAC_VIDEO_TOOLBOX_H265_ACCELERATOR_H_
