// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_VAAPI_H264_ACCELERATOR_H_
#define MEDIA_GPU_VAAPI_VAAPI_H264_ACCELERATOR_H_

#include "base/sequence_checker.h"
#include "media/filters/vp9_parser.h"
#include "media/gpu/h264_decoder.h"

// Verbatim from va/va.h, where typedef is used.
typedef struct _VAPictureH264 VAPictureH264;

namespace media {

template <class T> class DecodeSurfaceHandler;
class H264Picture;
class VASurface;
class VaapiWrapper;

class VaapiH264Accelerator : public H264Decoder::H264Accelerator {
 public:
  VaapiH264Accelerator(DecodeSurfaceHandler<VASurface>* vaapi_dec,
                       const scoped_refptr<VaapiWrapper> vaapi_wrapper);
  ~VaapiH264Accelerator() override;

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
  bool OutputPicture(scoped_refptr<H264Picture> pic) override;
  void Reset() override;

 private:
  void FillVAPicture(VAPictureH264* va_pic, scoped_refptr<H264Picture> pic);
  int FillVARefFramesFromDPB(const H264DPB& dpb,
                             VAPictureH264* va_pics,
                             int num_pics);

  const scoped_refptr<VaapiWrapper> vaapi_wrapper_;
  DecodeSurfaceHandler<VASurface>* vaapi_dec_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(VaapiH264Accelerator);
};

}  // namespace media

#endif  // MEDIA_GPU_VAAPI_VAAPI_H264_ACCELERATOR_H_
