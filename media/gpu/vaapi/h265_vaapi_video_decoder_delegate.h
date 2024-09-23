// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_H265_VAAPI_VIDEO_DECODER_DELEGATE_H_
#define MEDIA_GPU_VAAPI_H265_VAAPI_VIDEO_DECODER_DELEGATE_H_

#include <va/va.h>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "build/chromeos_buildflags.h"
#include "media/gpu/h265_decoder.h"
#include "media/gpu/h265_dpb.h"
#include "media/gpu/vaapi/vaapi_video_decoder_delegate.h"
#include "media/parsers/h265_parser.h"

// Verbatim from va/va.h, where typedef is used.
typedef struct _VAPictureHEVC VAPictureHEVC;

namespace media {

class CdmContext;
class H265Picture;

class H265VaapiVideoDecoderDelegate : public H265Decoder::H265Accelerator,
                                      public VaapiVideoDecoderDelegate {
 public:
  H265VaapiVideoDecoderDelegate(
      VaapiDecodeSurfaceHandler* vaapi_dec,
      scoped_refptr<VaapiWrapper> vaapi_wrapper,
      ProtectedSessionUpdateCB on_protected_session_update_cb,
      CdmContext* cdm_context,
      EncryptionScheme encryption_scheme);

  H265VaapiVideoDecoderDelegate(const H265VaapiVideoDecoderDelegate&) = delete;
  H265VaapiVideoDecoderDelegate& operator=(
      const H265VaapiVideoDecoderDelegate&) = delete;

  ~H265VaapiVideoDecoderDelegate() override;

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
  bool OutputPicture(scoped_refptr<H265Picture> pic) override;
  void Reset() override;
  Status SetStream(base::span<const uint8_t> stream,
                   const DecryptConfig* decrypt_config) override;
  bool IsChromaSamplingSupported(VideoChromaSampling chroma_sampling) override;

 private:
  void FillVAPicture(VAPictureHEVC* va_pic, scoped_refptr<H265Picture> pic);
  void FillVARefFramesFromRefList(const H265Picture::Vector& ref_pic_list,
                                  VAPictureHEVC* va_pics);

  // Returns |kInvalidRefPicIndex| if it cannot find a picture.
  int GetRefPicIndex(int poc);

  // Submits the slice data to the decoder for the prior slice that was just
  // submitted to us. This allows us to handle multi-slice pictures properly.
  // |last_slice| is set to true when submitting the last slice, false
  // otherwise.
  bool SubmitPriorSliceDataIfPresent(bool last_slice);

  // Stores the POCs (picture order counts) in the ReferenceFrames submitted as
  // the frame metadata so we can determine the indices for the reference frames
  // in the slice metadata.
  std::vector<int> ref_pic_list_pocs_;

  // Data from the prior/current slice for handling multi slice so we can
  // properly set the flag for the last slice.
  VASliceParameterBufferHEVC slice_param_;
  // We can hold onto the slice data pointer because we process all frames as
  // one DecoderBuffer, so the memory will still be accessible until the frame
  // is done. |last_slice_data_| being non-null indicates we have a valid
  // |slice_param_| filled.
  raw_ptr<const uint8_t> last_slice_data_{nullptr};
  size_t last_slice_size_{0};
  std::string last_transcrypt_params_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // We need to hold onto this memory here because it's referenced by the
  // mapped buffer in libva across calls. It is filled in SubmitSlice() and
  // stays alive until SubmitDecode() or Reset().
  std::vector<VAEncryptionSegmentInfo> encryption_segment_info_;

  // We need to retain this for the multi-slice case since that will aggregate
  // the encryption details across all the slices.
  VAEncryptionParameters crypto_params_;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
};

}  // namespace media

#endif  // MEDIA_GPU_VAAPI_H265_VAAPI_VIDEO_DECODER_DELEGATE_H_
