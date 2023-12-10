// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_V4L2_V4L2_VIDEO_DECODER_DELEGATE_H265_H_
#define MEDIA_GPU_V4L2_V4L2_VIDEO_DECODER_DELEGATE_H265_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "media/gpu/h265_decoder.h"
#include "media/gpu/h265_dpb.h"

struct v4l2_ctrl_hevc_decode_params;

namespace media {

class V4L2Device;
class V4L2DecodeSurface;
class V4L2DecodeSurfaceHandler;

class V4L2VideoDecoderDelegateH265 : public H265Decoder::H265Accelerator {
 public:
  using Status = H265Decoder::H265Accelerator::Status;

  explicit V4L2VideoDecoderDelegateH265(
      V4L2DecodeSurfaceHandler* surface_handler,
      V4L2Device* device);

  V4L2VideoDecoderDelegateH265(const V4L2VideoDecoderDelegateH265&) = delete;
  V4L2VideoDecoderDelegateH265& operator=(const V4L2VideoDecoderDelegateH265&) =
      delete;

  ~V4L2VideoDecoderDelegateH265() override;

  // H265Decoder::H265Accelerator implementation.
  scoped_refptr<H265Picture> CreateH265Picture() override;
  scoped_refptr<H265Picture> CreateH265PictureSecure(
      uint64_t secure_handle) override;
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
  std::vector<scoped_refptr<V4L2DecodeSurface>> FillInV4L2DPB(
      struct v4l2_ctrl_hevc_decode_params* v4l2_decode_param,
      const H265Picture::Vector& ref_pic_list,
      const H265Picture::Vector& ref_pic_set_lt_curr,
      const H265Picture::Vector& ref_pic_set_st_curr_after,
      const H265Picture::Vector& ref_pic_set_st_curr_before);
  scoped_refptr<V4L2DecodeSurface> H265PictureToV4L2DecodeSurface(
      H265Picture* pic);

  raw_ptr<V4L2DecodeSurfaceHandler> const surface_handler_;
  raw_ptr<V4L2Device> const device_;
};

}  // namespace media

#endif  // MEDIA_GPU_V4L2_V4L2_VIDEO_DECODER_DELEGATE_H265_H_
