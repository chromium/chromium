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
#include "base/sequence_checker.h"
#include "media/base/video_types.h"
#include "media/gpu/h265_decoder.h"
#include "media/gpu/mac/video_toolbox_decompression_metadata.h"
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
  bool ExtractParameterSetData(
      const char* parameter_set_name,
      const base::flat_set<int>& parameter_set_ids,
      const base::flat_map<int, std::vector<uint8_t>>& seen_parameter_set_data,
      base::flat_map<int, std::vector<uint8_t>>* active_parameter_set_data_out,
      std::vector<const uint8_t*>* parameter_set_data_out,
      std::vector<size_t>* parameter_set_size_out);
  bool CreateFormat(scoped_refptr<H265Picture> pic);
  bool ExtractChangedParameterSetData(
      const char* parameter_set_name,
      const base::flat_set<int>& parameter_set_ids,
      const base::flat_map<int, std::vector<uint8_t>>& seen_parameter_set_data,
      base::flat_map<int, std::vector<uint8_t>>* active_parameter_set_data_out,
      std::vector<base::span<const uint8_t>>* parameter_set_data_out);
  void ResetFrameData();

  std::unique_ptr<MediaLog> media_log_;

  // Callbacks are called synchronously, which is always re-entrant.
  DecodeCB decode_cb_;
  OutputCB output_cb_;

  // Raw parameter set bytes that have been observed.
  base::flat_map<int, std::vector<uint8_t>> seen_vps_data_;  // IDs can be 0-16
  base::flat_map<int, std::vector<uint8_t>> seen_sps_data_;  // IDs can be 0-15
  base::flat_map<int, std::vector<uint8_t>> seen_pps_data_;  // IDs can be 0-63

  // Cached parameter values.
  base::flat_set<int> alpha_vps_ids_;

  // Raw parameter set bytes that have been sent to the decoder, to compare for
  // changes.
  base::flat_map<int, std::vector<uint8_t>> active_vps_data_;
  base::flat_map<int, std::vector<uint8_t>> active_sps_data_;
  base::flat_map<int, std::vector<uint8_t>> active_pps_data_;

  base::apple::ScopedCFTypeRef<CMFormatDescriptionRef> active_format_;
  VideoToolboxDecompressionSessionMetadata active_session_metadata_;

  // Accumulated data for the current frame.
  base::flat_set<int> frame_vps_ids_;  // Note: there should be exactly one VPS.
  base::flat_set<int> frame_sps_ids_;
  base::flat_set<int> frame_pps_ids_;
  std::vector<base::span<const uint8_t>> frame_slice_data_;
  uint8_t frame_bit_depth_ = 8;
  VideoChromaSampling frame_chroma_sampling_ = VideoChromaSampling::k420;
  bool frame_is_keyframe_ = false;
  bool frame_has_alpha_ = false;
  bool drop_frame_ = false;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace media

#endif  // MEDIA_GPU_MAC_VIDEO_TOOLBOX_H265_ACCELERATOR_H_
