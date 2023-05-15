// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_D3D11_H264_ACCELERATOR_H_
#define MEDIA_GPU_WINDOWS_D3D11_H264_ACCELERATOR_H_

#include <d3d11_1.h>
#include <d3d9.h>
#include <dxva.h>
#include <wrl/client.h>

#include <vector>

#include "base/memory/raw_ptr.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "media/base/video_frame.h"
#include "media/base/win/mf_helpers.h"
#include "media/gpu/h264_decoder.h"
#include "media/gpu/h264_dpb.h"
#include "media/gpu/windows/d3d11_com_defs.h"
#include "media/gpu/windows/d3d11_status.h"
#include "media/gpu/windows/d3d11_video_context_wrapper.h"
#include "media/gpu/windows/d3d11_video_decoder_client.h"
#include "media/gpu/windows/d3d_accelerator.h"
#include "media/video/picture.h"
#include "third_party/angle/include/EGL/egl.h"
#include "third_party/angle/include/EGL/eglext.h"

namespace media {

constexpr int kRefFrameMaxCount = 16;

class MediaLog;

class D3D11H264Accelerator : public D3DAccelerator,
                             public H264Decoder::H264Accelerator {
 public:
  D3D11H264Accelerator(D3D11VideoDecoderClient* client,
                       MediaLog* media_log,
                       ComD3D11VideoDevice video_device,
                       std::unique_ptr<VideoContextWrapper> video_context);

  D3D11H264Accelerator(const D3D11H264Accelerator&) = delete;
  D3D11H264Accelerator& operator=(const D3D11H264Accelerator&) = delete;

  ~D3D11H264Accelerator() override;

  // H264Decoder::H264Accelerator implementation.
  scoped_refptr<H264Picture> CreateH264Picture() override;
  Status SubmitFrameMetadata(const H264SPS* sps,
                             const H264PPS* pps,
                             const H264DPB& dpb,
                             const H264Picture::Vector& ref_pic_listp0,
                             const H264Picture::Vector& ref_pic_listb0,
                             const H264Picture::Vector& ref_pic_listb1,
                             scoped_refptr<H264Picture> pic) override;
  Status SubmitSlice(const H264PPS* pps,
                     const H264SliceHeader* slice_hdr,
                     const H264Picture::Vector& ref_pic_list0,
                     const H264Picture::Vector& ref_pic_list1,
                     scoped_refptr<H264Picture> pic,
                     const uint8_t* data,
                     size_t size,
                     const std::vector<SubsampleEntry>& subsamples) override;
  Status SubmitDecode(scoped_refptr<H264Picture> pic) override;
  void Reset() override;
  bool OutputPicture(scoped_refptr<H264Picture> pic) override;

 private:
  bool SubmitSliceData();
  bool RetrieveBitstreamBuffer();

  // Gets a pic params struct with the constant fields set.
  void FillPicParamsWithConstants(DXVA_PicParams_H264* pic_param);

  // Populate the pic params with fields from the SPS structure.
  void PicParamsFromSPS(DXVA_PicParams_H264* pic_param,
                        const H264SPS* sps,
                        bool field_pic);

  // Populate the pic params with fields from the PPS structure.
  bool PicParamsFromPPS(DXVA_PicParams_H264* pic_param, const H264PPS* pps);

  // Populate the pic params with fields from the slice header structure.
  void PicParamsFromSliceHeader(DXVA_PicParams_H264* pic_param,
                                const H264SliceHeader* slice_hdr);

  void PicParamsFromPic(DXVA_PicParams_H264* pic_param, D3D11H264Picture* pic);

  // This information set at the beginning of a frame and saved for processing
  // all the slices.
  DXVA_PicEntry_H264 ref_frame_list_[kRefFrameMaxCount];
  H264SPS sps_;
  INT field_order_cnt_list_[kRefFrameMaxCount][2];
  USHORT frame_num_list_[kRefFrameMaxCount];
  UINT used_for_reference_flags_;
  USHORT non_existing_frame_flags_;

  // Information that's accumulated during slices and submitted at the end
  std::vector<DXVA_Slice_H264_Short> slice_info_;
  size_t current_offset_ = 0;
  size_t bitstream_buffer_size_ = 0;
  raw_ptr<uint8_t, AllowPtrArithmetic> bitstream_buffer_bytes_ = nullptr;
};

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_D3D11_H264_ACCELERATOR_H_
