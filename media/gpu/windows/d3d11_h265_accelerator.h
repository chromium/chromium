// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_D3D11_H265_ACCELERATOR_H_
#define MEDIA_GPU_WINDOWS_D3D11_H265_ACCELERATOR_H_

#include <d3d11_1.h>
#include <d3d9.h>
#include <dxva.h>

#include "base/memory/raw_ptr.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "media/base/video_frame.h"
#include "media/base/win/mf_helpers.h"
#include "media/gpu/h265_decoder.h"
#include "media/gpu/h265_dpb.h"
#include "media/gpu/windows/d3d11_video_decoder_client.h"
#include "third_party/angle/include/EGL/egl.h"
#include "third_party/angle/include/EGL/eglext.h"

namespace media {

// Maximum of valid DXVA_PicEntry_HEVC entries in RefPicList
constexpr unsigned kMaxRefPicListSize = 15;

class MediaLog;

// Picture Parameters DXVA buffer struct for Rext/Scc is not specified in DXVA
// spec. The below structures come from Intel platform DDI definition, so they
// are currently Intel specific.
// For NVidia and AMD platforms supporting HEVC Rext & Scc, it is expected
// the picture param information included in below structures is sufficient
// for underlying drivers supporting range extension/Scc.
#pragma pack(push, 1)
typedef struct {
  DXVA_PicParams_HEVC main;

  // HEVC Range Extension. Fields are named the same as in HEVC spec.
  union {
    struct {
      UINT32 transform_skip_rotation_enabled_flag : 1;
      UINT32 transform_skip_context_enabled_flag : 1;
      UINT32 implicit_rdpcm_enabled_flag : 1;
      UINT32 explicit_rdpcm_enabled_flag : 1;
      UINT32 extended_precision_processing_flag : 1;
      UINT32 intra_smoothing_disabled_flag : 1;
      UINT32 high_precision_offsets_enabled_flag : 1;
      UINT32 persistent_rice_adaptation_enabled_flag : 1;
      UINT32 cabac_bypass_alignment_enabled_flag : 1;
      UINT32 cross_component_prediction_enabled_flag : 1;
      UINT32 chroma_qp_offset_list_enabled_flag : 1;
      // Indicates if luma bit depth equals to 16. If its value is 1, the
      // corresponding bit_depth_luma_minus8 must be set to 0.
      UINT32 BitDepthLuma16 : 1;
      // Indicates if chroma bit depth equals to 16. If its value is 1, the
      // corresponding bit_depth_chroma_minus8 must be set to 0.
      UINT32 BitDepthChroma16 : 1;
      UINT32 ReservedBits8 : 19;
    };
    UINT32 dwRangeExtensionFlags;
  };

  UCHAR diff_cu_chroma_qp_offset_depth;    // [0..3]
  UCHAR chroma_qp_offset_list_len_minus1;  // [0..5]
  UCHAR log2_sao_offset_scale_luma;        // [0..6]
  UCHAR log2_sao_offset_scale_chroma;      // [0..6]
  UCHAR log2_max_transform_skip_block_size_minus2;
  CHAR cb_qp_offset_list[6];  // [-12..12]
  CHAR cr_qp_offset_list[6];  // [-12..12]
} DXVA_PicParams_HEVC_Rext;

typedef struct {
  DXVA_PicParams_HEVC_Rext main_rext;

  // HEVC Screen Content Coding. Fields are named the same as in HEVC spec.
  union {
    struct {
      UINT32 pps_curr_pic_ref_enabled_flag : 1;
      UINT32 palette_mode_enabled_flag : 1;
      UINT32 motion_vector_resolution_control_idc : 2;
      UINT32 intra_boundary_filtering_disabled_flag : 1;
      UINT32 residual_adaptive_coloour_transform_enabled_flag : 1;
      UINT32 pps_slice_act_qp_offsets_present_flag : 1;
      UINT32 ReservedBits9 : 25;
    };
    UINT dwSccExtensionFlags;
  };

  UCHAR palette_max_size;                  // [0..64]
  UCHAR delta_palette_max_predictor_size;  // [0..128]
  UCHAR PredictorPaletteSize;              // [0..127]
  USHORT PredictorPaletteEntries[3][128];
  CHAR pps_act_y_qp_offset_plus5;   // [-7..17]
  CHAR pps_act_cb_qp_offset_plus5;  // [-7..17]
  CHAR pps_act_cr_qp_offset_plus3;  // [-9..15]
} DXVA_PicParams_HEVC_SCC;
#pragma pack(pop)

class D3D11H265Accelerator : public H265Decoder::H265Accelerator {
 public:
  D3D11H265Accelerator(D3D11VideoDecoderClient* client, MediaLog* media_log);

  D3D11H265Accelerator(const D3D11H265Accelerator&) = delete;
  D3D11H265Accelerator& operator=(const D3D11H265Accelerator&) = delete;

  ~D3D11H265Accelerator() override;

  // H265Decoder::H265Accelerator implementation.
  scoped_refptr<H265Picture> CreateH265Picture() override;
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
  void Reset() override;
  bool OutputPicture(scoped_refptr<H265Picture> pic) override;
  Status SetStream(base::span<const uint8_t> stream,
                   const DecryptConfig* decrypt_config) override;
  bool IsChromaSamplingSupported(VideoChromaSampling chroma_sampling) override;

 private:
  // Gets a pic params struct with the constant fields set.
  void FillPicParamsWithConstants(DXVA_PicParams_HEVC_Rext* pic_param);

  // Populate the pic params with fields from the SPS structure.
  void PicParamsFromSPS(DXVA_PicParams_HEVC_Rext* pic_param,
                        const H265SPS* sps);

  // Populate the pic params with fields from the PPS structure.
  void PicParamsFromPPS(DXVA_PicParams_HEVC_Rext* pic_param,
                        const H265PPS* pps);

  // Populate the pic params with fields from the slice header structure.
  void PicParamsFromSliceHeader(DXVA_PicParams_HEVC_Rext* pic_param,
                                const H265SPS* sps,
                                const H265SliceHeader* slice_hdr);

  // Populate the pic params with fields from the picture passed in.
  void PicParamsFromPic(DXVA_PicParams_HEVC_Rext* pic_param,
                        D3D11H265Picture* pic);

  // Populate the pic params with fields from ref_pic_set_lt_curr,
  // ref_pic_set_st_curr_after and ref_pic_set_st_curr_before
  bool PicParamsFromRefLists(
      DXVA_PicParams_HEVC_Rext* pic_param,
      const H265Picture::Vector& ref_pic_set_lt_curr,
      const H265Picture::Vector& ref_pic_set_st_curr_after,
      const H265Picture::Vector& ref_pic_set_st_curr_before);

  std::unique_ptr<MediaLog> media_log_;
  raw_ptr<D3D11VideoDecoderClient> client_;

  // This information set at the beginning of a frame and saved for processing
  // all the slices.
  DXVA_PicEntry_HEVC ref_frame_list_[kMaxRefPicListSize];
  int ref_frame_pocs_[kMaxRefPicListSize];
  base::flat_map<int, int> poc_index_into_ref_pic_list_;
  bool use_scaling_lists_ = false;
  // If current stream is encoded with range extension profile.
  bool is_rext_ = false;

  // For HEVC this number needs to be larger than 1 and different
  // in each call to Execute().
  int current_status_report_feedback_num_ = 1;

  uint32_t current_frame_size_ = 0;
};

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_D3D11_H265_ACCELERATOR_H_
