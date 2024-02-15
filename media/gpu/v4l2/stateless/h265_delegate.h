// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_V4L2_STATELESS_H265_DELEGATE_H_
#define MEDIA_GPU_V4L2_STATELESS_H265_DELEGATE_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "media/gpu/h265_decoder.h"
#include "media/gpu/h265_dpb.h"

namespace media {

class StatelessDecodeSurface;
class StatelessDecodeSurfaceHandler;
struct H265DelegateContext;

class H265Delegate : public H265Decoder::H265Accelerator {
 public:
  explicit H265Delegate(StatelessDecodeSurfaceHandler* surface_handler);

  H265Delegate(const H265Delegate&) = delete;
  H265Delegate& operator=(const H265Delegate&) = delete;

  ~H265Delegate() override;

  // H265Decoder::H265Accelerator implementation
  scoped_refptr<H265Picture> CreateH265Picture() override;

  H265Delegate::Status SubmitFrameMetadata(
      const H265SPS* sps,
      const H265PPS* pps,
      const H265SliceHeader* slice_hdr,
      const H265Picture::Vector& ref_pic_list,
      const H265Picture::Vector& ref_pic_set_lt_curr,
      const H265Picture::Vector& ref_pic_set_st_curr_after,
      const H265Picture::Vector& ref_pic_set_st_curr_before,
      scoped_refptr<H265Picture> pic) override;
  H265Delegate::Status SubmitSlice(
      const H265SPS* sps,
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
  H265Delegate::Status SubmitDecode(scoped_refptr<H265Picture> pic) override;
  bool OutputPicture(scoped_refptr<H265Picture> pic) override;
  void Reset() override;
  bool IsChromaSamplingSupported(VideoChromaSampling format) override;

 private:
  std::vector<scoped_refptr<StatelessDecodeSurface>> FillInV4L2DPB(
      const H265Picture::Vector& ref_pic_list,
      const H265Picture::Vector& ref_pic_set_lt_curr,
      const H265Picture::Vector& ref_pic_set_st_curr_after,
      const H265Picture::Vector& ref_pic_set_st_curr_before);

  raw_ptr<StatelessDecodeSurfaceHandler> const surface_handler_;

  // Contains the kernel-specific structures that we don't want to expose
  // outside of the compilation unit.
  const std::unique_ptr<H265DelegateContext> ctx_;
};

}  // namespace media

#endif  // MEDIA_GPU_V4L2_STATELESS_H265_DELEGATE_H_
