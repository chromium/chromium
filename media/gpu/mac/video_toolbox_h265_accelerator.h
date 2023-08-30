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
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "media/gpu/h265_decoder.h"
#include "media/gpu/mac/video_toolbox_decode_metadata.h"
#include "media/gpu/media_gpu_export.h"

namespace media {

class MediaLog;

class MEDIA_GPU_EXPORT VideoToolboxH265Accelerator
    : public H265Decoder::H265Accelerator {
 public:
  using DecodeCB = base::RepeatingCallback<void(
      base::apple::ScopedCFTypeRef<CMSampleBufferRef>,
      VideoToolboxSessionMetadata,
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

 private:
  std::unique_ptr<MediaLog> media_log_;

  // Callbacks are called synchronously, which is always re-entrant.
  DecodeCB decode_cb_;
  OutputCB output_cb_;

  // Raw parameter set bytes that have been observed.
  base::flat_map<int, std::vector<uint8_t>> seen_vps_data_;  // IDs can be 0-16
  base::flat_map<int, std::vector<uint8_t>> seen_sps_data_;  // IDs can be 0-15
  base::flat_map<int, std::vector<uint8_t>> seen_pps_data_;  // IDs can be 0-63

  // Raw parameter set bytes used to produce |active_format_|, so that they
  // can be checked for changes.
  std::vector<uint8_t> active_vps_data_;
  std::vector<uint8_t> active_sps_data_;
  std::vector<uint8_t> active_pps_data_;

  base::apple::ScopedCFTypeRef<CMFormatDescriptionRef> active_format_;
  VideoToolboxSessionMetadata session_metadata_;

  // Accumulated slice data for the current frame.
  std::vector<base::span<const uint8_t>> slice_nalu_data_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace media

#endif  // MEDIA_GPU_MAC_VIDEO_TOOLBOX_H265_ACCELERATOR_H_
