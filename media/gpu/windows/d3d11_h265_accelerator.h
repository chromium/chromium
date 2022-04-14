// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_D3D11_H265_ACCELERATOR_H_
#define MEDIA_GPU_WINDOWS_D3D11_H265_ACCELERATOR_H_

#include <d3d11_1.h>
#include <d3d9.h>
#include <dxva.h>
#include <wrl/client.h>

#include <vector>

#include "base/memory/raw_ptr.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "media/base/video_frame.h"
#include "media/base/win/mf_helpers.h"
#include "media/gpu/h265_decoder.h"
#include "media/gpu/h265_dpb.h"
#include "media/gpu/windows/d3d11_com_defs.h"
#include "media/gpu/windows/d3d11_status.h"
#include "media/gpu/windows/d3d11_video_context_wrapper.h"
#include "media/gpu/windows/d3d11_video_decoder_client.h"
#include "media/video/picture.h"
#include "third_party/angle/include/EGL/egl.h"
#include "third_party/angle/include/EGL/eglext.h"
#include "ui/gl/gl_image.h"

namespace media {

// Maximum of valid DXVA_PicEntry_HEVC entries in RefPicList
constexpr unsigned kMaxRefPicListSize = 15;

class D3D11H265Accelerator;
class MediaLog;

class D3D11H265Accelerator : public H265Decoder::H265Accelerator {
 public:
  D3D11H265Accelerator(D3D11VideoDecoderClient* client,
                       MediaLog* media_log,
                       ComD3D11VideoDevice video_device,
                       std::unique_ptr<VideoContextWrapper> video_context);

  D3D11H265Accelerator(const D3D11H265Accelerator&) = delete;
  D3D11H265Accelerator& operator=(const D3D11H265Accelerator&) = delete;

  ~D3D11H265Accelerator() override;

  // H265Decoder::H265Accelerator implementation.
  scoped_refptr<H265Picture> CreateH265Picture() override;
  Status SubmitFrameMetadata(const H265SPS* sps,
                             const H265PPS* pps,
                             const H265SliceHeader* slice_hdr,
                             const H265Picture::Vector& ref_pic_list,
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
  void Reset() override;
  bool OutputPicture(scoped_refptr<H265Picture> pic) override;

 private:
  bool SubmitSliceData();
  bool RetrieveBitstreamBuffer();

  // Gets a pic params struct with the constant fields set.
  void FillPicParamsWithConstants(DXVA_PicParams_HEVC* pic_param);

  // Populate the pic params with fields from the SPS structure.
  void PicParamsFromSPS(DXVA_PicParams_HEVC* pic_param, const H265SPS* sps);

  // Populate the pic params with fields from the PPS structure.
  void PicParamsFromPPS(DXVA_PicParams_HEVC* pic_param, const H265PPS* pps);

  // Populate the pic params with fields from the slice header structure.
  void PicParamsFromSliceHeader(DXVA_PicParams_HEVC* pic_param,
                                const H265SPS* sps,
                                const H265SliceHeader* slice_hdr);

  // Populate the pic params with fields from the picture passed in.
  void PicParamsFromPic(DXVA_PicParams_HEVC* pic_param, D3D11H265Picture* pic);

  // Populate the pic params with fields from ref_pic_set_lt_curr,
  // ref_pic_set_st_curr_after and ref_pic_set_st_curr_before
  bool PicParamsFromRefLists(
      DXVA_PicParams_HEVC* pic_param,
      const H265Picture::Vector& ref_pic_set_lt_curr,
      const H265Picture::Vector& ref_pic_set_st_curr_after,
      const H265Picture::Vector& ref_pic_set_st_curr_before);

  void SetVideoDecoder(ComD3D11VideoDecoder video_decoder);

  // Record a failure to DVLOG and |media_log_|.
  void RecordFailure(const std::string& reason,
                     D3D11Status::Codes code,
                     HRESULT hr = S_OK) const;
  void RecordFailure(D3D11Status error) const;

  raw_ptr<D3D11VideoDecoderClient> client_;
  raw_ptr<MediaLog> media_log_ = nullptr;

  ComD3D11VideoDecoder video_decoder_;
  ComD3D11VideoDevice video_device_;
  std::unique_ptr<VideoContextWrapper> video_context_;

  // This information set at the beginning of a frame and saved for processing
  // all the slices.
  DXVA_PicEntry_HEVC ref_frame_list_[kMaxRefPicListSize];
  int ref_frame_pocs_[kMaxRefPicListSize];
  base::flat_map<int, int> poc_index_into_ref_pic_list_;
  bool use_scaling_lists_ = false;

  // Information that's accumulated during slices and submitted at the end
  std::vector<DXVA_Slice_HEVC_Short> slice_info_;
  size_t current_offset_ = 0;
  size_t bitstream_buffer_size_ = 0;
  raw_ptr<uint8_t> bitstream_buffer_bytes_ = nullptr;

  // For HEVC this number needs to be larger than 1 and different
  // in each call to Execute().
  int current_status_report_feedback_num_ = 1;

  // This contains the subsamples (clear and encrypted) of the slice data
  // in D3D11_VIDEO_DECODER_BUFFER_BITSTREAM buffer.
  std::vector<D3D11_VIDEO_DECODER_SUB_SAMPLE_MAPPING_BLOCK> subsamples_;
  // IV for the current frame.
  std::vector<uint8_t> frame_iv_;
};

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_D3D11_H265_ACCELERATOR_H_
