// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_V4L2_V4L2_VIDEO_DECODER_DELEGATE_H264_H_
#define MEDIA_GPU_V4L2_V4L2_VIDEO_DECODER_DELEGATE_H264_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "media/base/cdm_context.h"
#include "media/gpu/h264_decoder.h"
#include "media/gpu/h264_dpb.h"
#include "media/parsers/h264_parser.h"

namespace media {

class V4L2Device;
class V4L2DecodeSurface;
class V4L2DecodeSurfaceHandler;
struct V4L2VideoDecoderDelegateH264Private;

// This is used by the secure world when parsing the stream to create
// CencV1SliceParameterBufferH264 (defined in the .cc file). This contains all
// the required SPS/PPS fields used in slice header parsing so that we don't
// need to re-parse the SPS/PPS in the secure world.
struct CencV1StreamDataForSliceHeader {
  int32_t log2_max_frame_num_minus4;
  int32_t log2_max_pic_order_cnt_lsb_minus4;
  int32_t pic_order_cnt_type;
  int32_t num_ref_idx_l0_default_active_minus1;
  int32_t num_ref_idx_l1_default_active_minus1;
  int32_t weighted_bipred_idc;
  int32_t chroma_array_type;
  uint8_t frame_mbs_only_flag;
  uint8_t bottom_field_pic_order_in_frame_present_flag;
  uint8_t delta_pic_order_always_zero_flag;
  uint8_t redundant_pic_cnt_present_flag;
  uint8_t weighted_pred_flag;
  uint8_t padding[3];
};

class V4L2VideoDecoderDelegateH264 : public H264Decoder::H264Accelerator {
 public:
  using Status = H264Decoder::H264Accelerator::Status;

  explicit V4L2VideoDecoderDelegateH264(
      V4L2DecodeSurfaceHandler* surface_handler,
      V4L2Device* device,
      CdmContext* cdm_context);

  V4L2VideoDecoderDelegateH264(const V4L2VideoDecoderDelegateH264&) = delete;
  V4L2VideoDecoderDelegateH264& operator=(const V4L2VideoDecoderDelegateH264&) =
      delete;

  ~V4L2VideoDecoderDelegateH264() override;

  // H264Decoder::H264Accelerator implementation.
  scoped_refptr<H264Picture> CreateH264Picture() override;
  scoped_refptr<H264Picture> CreateH264PictureSecure(
      uint64_t secure_handle) override;
  void ProcessSPS(const H264SPS* sps,
                  base::span<const uint8_t> sps_nalu_data) override;
  void ProcessPPS(const H264PPS* pps,
                  base::span<const uint8_t> pps_nalu_data) override;
  Status SubmitFrameMetadata(const H264SPS* sps,
                             const H264PPS* pps,
                             const H264DPB& dpb,
                             const H264Picture::Vector& ref_pic_listp0,
                             const H264Picture::Vector& ref_pic_listb0,
                             const H264Picture::Vector& ref_pic_listb1,
                             scoped_refptr<H264Picture> pic) override;
  Status ParseEncryptedSliceHeader(
      const std::vector<base::span<const uint8_t>>& data,
      const std::vector<SubsampleEntry>& subsamples,
      uint64_t secure_handle,
      H264SliceHeader* slice_header_out) override;
  Status SubmitSlice(const H264PPS* pps,
                     const H264SliceHeader* slice_hdr,
                     const H264Picture::Vector& ref_pic_list0,
                     const H264Picture::Vector& ref_pic_list1,
                     scoped_refptr<H264Picture> pic,
                     const uint8_t* data,
                     size_t size,
                     const std::vector<SubsampleEntry>& subsamples) override;
  Status SubmitDecode(scoped_refptr<H264Picture> pic) override;
  bool OutputPicture(scoped_refptr<H264Picture> pic) override;
  void Reset() override;

 private:
  std::vector<scoped_refptr<V4L2DecodeSurface>> H264DPBToV4L2DPB(
      const H264DPB& dpb);
  scoped_refptr<V4L2DecodeSurface> H264PictureToV4L2DecodeSurface(
      H264Picture* pic);
  void OnEncryptedSliceHeaderParsed(bool status,
                                    const std::vector<uint8_t>& parsed_headers);

  raw_ptr<V4L2DecodeSurfaceHandler> const surface_handler_;
  raw_ptr<V4L2Device> const device_;
  raw_ptr<CdmContext> cdm_context_;

  // The last returned data for async encrypted slice header parsing, we hold
  // onto this so when we get invoked a second time to parse it we know the
  // result and can immediately return it.
  std::vector<uint8_t> last_parsed_encrypted_slice_header_;
  bool encrypted_slice_header_parsing_failed_ = false;

  // For multi-slice CENCv1 H264 content we will need to track the size of the
  // headers already processed so we know where the subsequent headers are at in
  // the secure buffer.
  size_t encrypted_slice_header_offset_ = 0;

  // Tracking of the last SPS/PPS data so we can use the values for encrypted
  // slice header parsing.
  struct CencV1StreamDataForSliceHeader cencv1_stream_data_;

  // Contains the kernel-specific structures that we don't want to expose
  // outside of the compilation unit.
  const std::unique_ptr<V4L2VideoDecoderDelegateH264Private> priv_;

  base::WeakPtrFactory<V4L2VideoDecoderDelegateH264> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_GPU_V4L2_V4L2_VIDEO_DECODER_DELEGATE_H264_H_
