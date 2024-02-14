// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_V4L2_STATELESS_H264_DELEGATE_H_
#define MEDIA_GPU_V4L2_STATELESS_H264_DELEGATE_H_

#include <memory>
#include <vector>

#include <linux/v4l2-controls.h>
#include <linux/videodev2.h>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "media/gpu/h264_decoder.h"
#include "media/gpu/h264_dpb.h"

namespace media {

class StatelessDecodeSurface;
class StatelessDecodeSurfaceHandler;

class H264Delegate : public H264Decoder::H264Accelerator {
 public:
  explicit H264Delegate(StatelessDecodeSurfaceHandler* surface_handler);

  H264Delegate(const H264Delegate&) = delete;
  H264Delegate& operator=(const H264Delegate&) = delete;

  ~H264Delegate() override;

  // H264Decoder::H264Accelerator implementation
  scoped_refptr<H264Picture> CreateH264Picture() override;
  H264Delegate::Status SubmitFrameMetadata(
      const H264SPS* sps,
      const H264PPS* pps,
      const H264DPB& dpb,
      const H264Picture::Vector& ref_pic_listp0,
      const H264Picture::Vector& ref_pic_listb0,
      const H264Picture::Vector& ref_pic_listb1,
      scoped_refptr<H264Picture> pic) override;
  H264Delegate::Status SubmitSlice(
      const H264PPS* pps,
      const H264SliceHeader* slice_hdr,
      const H264Picture::Vector& ref_pic_list0,
      const H264Picture::Vector& ref_pic_list1,
      scoped_refptr<H264Picture> pic,
      const uint8_t* data,
      size_t size,
      const std::vector<SubsampleEntry>& subsamples) override;
  H264Delegate::Status SubmitDecode(scoped_refptr<H264Picture> pic) override;
  bool OutputPicture(scoped_refptr<H264Picture> pic) override;
  void Reset() override;

 private:
  std::vector<scoped_refptr<StatelessDecodeSurface>> GetRefSurfacesFromDPB(
      const H264DPB& dpb);

  raw_ptr<StatelessDecodeSurfaceHandler> const surface_handler_;

  v4l2_ctrl_h264_sps v4l2_sps_;
  v4l2_ctrl_h264_pps v4l2_pps_;
  v4l2_ctrl_h264_scaling_matrix v4l2_scaling_matrix_;
  v4l2_ctrl_h264_decode_params v4l2_decode_param_;
  std::vector<uint8_t> slice_data_;
};

}  // namespace media

#endif  // MEDIA_GPU_V4L2_STATELESS_H264_DELEGATE_H_
