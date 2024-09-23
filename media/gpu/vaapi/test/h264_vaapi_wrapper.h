// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_TEST_H264_VAAPI_WRAPPER_H_
#define MEDIA_GPU_VAAPI_TEST_H264_VAAPI_WRAPPER_H_

#include "base/memory/raw_ref.h"
#include "base/sequence_checker.h"
#include "media/gpu/vaapi/test/h264_dpb.h"
#include "media/gpu/vaapi/test/scoped_va_config.h"
#include "media/gpu/vaapi/test/scoped_va_context.h"
#include "media/gpu/vaapi/test/vaapi_device.h"
#include "media/parsers/h264_parser.h"

namespace media::vaapi_test {

class H264VaapiWrapper {
 public:
  explicit H264VaapiWrapper(const VaapiDevice& va_device);
  ~H264VaapiWrapper();

  // Generates a picture object with a SharedVASurface.
  scoped_refptr<H264Picture> CreatePicture(const H264SPS* sps);

  // Start a new frame.
  void SubmitFrameMetadata(const H264SPS* sps,
                           const H264PPS* pps,
                           const H264DPB& dpb,
                           const H264Picture::Vector& ref_pic_listp0,
                           const H264Picture::Vector& ref_pic_listb0,
                           const H264Picture::Vector& ref_pic_listb1,
                           scoped_refptr<H264Picture> pic);

  // Add a slice to the current frame.
  void SubmitSlice(const H264PPS* pps,
                   const H264SliceHeader* slice_header,
                   const H264Picture::Vector& ref_pic_list0,
                   const H264Picture::Vector& ref_pic_list1,
                   scoped_refptr<H264Picture> pic,
                   const uint8_t* data,
                   size_t size,
                   const std::vector<SubsampleEntry>& subsamples);

  // Perform the actual decoding process.
  void SubmitDecode(scoped_refptr<H264Picture> pic);

 private:
  const raw_ref<const VaapiDevice> va_device_;

  std::unique_ptr<ScopedVAConfig> va_config_;
  std::unique_ptr<ScopedVAContext> va_context_;

  std::vector<VABufferID> buffers_;
};

}  // namespace media::vaapi_test

#endif  // MEDIA_GPU_VAAPI_TEST_H264_VAAPI_WRAPPER_H_
