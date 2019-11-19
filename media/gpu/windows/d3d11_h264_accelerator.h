// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_D3D11_H264_ACCELERATOR_H_
#define MEDIA_GPU_WINDOWS_D3D11_H264_ACCELERATOR_H_

#include <d3d11_1.h>
#include <d3d9.h>
#include <dxva.h>
#include <wrl/client.h>

#include <vector>

#include "gpu/command_buffer/service/texture_manager.h"
#include "media/base/video_frame.h"
#include "media/gpu/h264_decoder.h"
#include "media/gpu/h264_dpb.h"
#include "media/gpu/windows/d3d11_com_defs.h"
#include "media/gpu/windows/d3d11_video_context_wrapper.h"
#include "media/gpu/windows/d3d11_video_decoder_client.h"
#include "media/gpu/windows/return_on_failure.h"
#include "media/video/picture.h"
#include "third_party/angle/include/EGL/egl.h"
#include "third_party/angle/include/EGL/eglext.h"
#include "ui/gl/gl_image.h"

namespace media {
class CdmProxyContext;
class D3D11H264Accelerator;
class MediaLog;


class D3D11H264Accelerator : public H264Decoder::H264Accelerator {
 public:
  // |cdm_proxy_context| may be null for clear content.
  D3D11H264Accelerator(D3D11VideoDecoderClient* client,
                       MediaLog* media_log,
                       CdmProxyContext* cdm_proxy_context,
                       ComD3D11VideoDecoder video_decoder,
                       ComD3D11VideoDevice video_device,
                       std::unique_ptr<VideoContextWrapper> video_context);
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
                                const H264SliceHeader* pps);

  void PicParamsFromPic(DXVA_PicParams_H264* pic_param,
                        scoped_refptr<H264Picture> pic);

 private:
  bool SubmitSliceData();
  bool RetrieveBitstreamBuffer();

  // Record a failure to DVLOG and |media_log_|.
  void RecordFailure(const std::string& reason, HRESULT hr = S_OK) const;

  D3D11VideoDecoderClient* client_;
  MediaLog* media_log_ = nullptr;
  CdmProxyContext* const cdm_proxy_context_;

  ComD3D11VideoDecoder video_decoder_;
  ComD3D11VideoDevice video_device_;
  std::unique_ptr<VideoContextWrapper> video_context_;

  // This information set at the beginning of a frame and saved for processing
  // all the slices.
  DXVA_PicEntry_H264 ref_frame_list_[16];
  H264SPS sps_;
  INT field_order_cnt_list_[16][2];
  USHORT frame_num_list_[16];
  UINT used_for_reference_flags_;
  USHORT non_existing_frame_flags_;

  // Information that's accumulated during slices and submitted at the end
  std::vector<DXVA_Slice_H264_Short> slice_info_;
  size_t current_offset_ = 0;
  size_t bitstream_buffer_size_ = 0;
  uint8_t* bitstream_buffer_bytes_ = nullptr;

  // This contains the subsamples (clear and encrypted) of the slice data
  // in D3D11_VIDEO_DECODER_BUFFER_BITSTREAM buffer.
  std::vector<D3D11_VIDEO_DECODER_SUB_SAMPLE_MAPPING_BLOCK> subsamples_;
  // IV for the current frame.
  std::vector<uint8_t> frame_iv_;

  DISALLOW_COPY_AND_ASSIGN(D3D11H264Accelerator);
};

}  // namespace media

#endif  // MEDIA_GPU_D3D11_WINDOWS_H264_ACCELERATOR_H_
