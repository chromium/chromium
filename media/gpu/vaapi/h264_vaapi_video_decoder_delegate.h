// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_H264_VAAPI_VIDEO_DECODER_DELEGATE_H_
#define MEDIA_GPU_VAAPI_H264_VAAPI_VIDEO_DECODER_DELEGATE_H_

#include "base/atomic_sequence_num.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "build/chromeos_buildflags.h"
#include "media/gpu/h264_decoder.h"
#include "media/gpu/vaapi/vaapi_video_decoder_delegate.h"
#include "media/parsers/h264_parser.h"

// Verbatim from va/va.h, where typedef is used.
typedef struct _VAPictureH264 VAPictureH264;

namespace media {

class CdmContext;
class H264Picture;

class H264VaapiVideoDecoderDelegate : public H264Decoder::H264Accelerator,
                                      public VaapiVideoDecoderDelegate {
 public:
  H264VaapiVideoDecoderDelegate(
      VaapiDecodeSurfaceHandler* vaapi_dec,
      scoped_refptr<VaapiWrapper> vaapi_wrapper,
      ProtectedSessionUpdateCB on_protected_session_update_cb =
          base::DoNothing(),
      CdmContext* cdm_context = nullptr,
      EncryptionScheme encryption_scheme = EncryptionScheme::kUnencrypted);

  H264VaapiVideoDecoderDelegate(const H264VaapiVideoDecoderDelegate&) = delete;
  H264VaapiVideoDecoderDelegate& operator=(
      const H264VaapiVideoDecoderDelegate&) = delete;

  ~H264VaapiVideoDecoderDelegate() override;

  // H264Decoder::H264Accelerator implementation.
  scoped_refptr<H264Picture> CreateH264Picture() override;
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
  Status SetStream(base::span<const uint8_t> stream,
                   const DecryptConfig* decrypt_config) override;

  bool RequiresRefLists() override;

 private:
  void FillVAPicture(VAPictureH264* va_pic, scoped_refptr<H264Picture> pic);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // We need to hold onto this memory here because it's referenced by the
  // mapped buffer in libva across calls. It is filled in SubmitSlice() and
  // stays alive until SubmitDecode() or Reset().
  std::vector<VAEncryptionSegmentInfo> encryption_segment_info_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // We need to retain this for the multi-slice case since that will aggregate
  // the encryption details across all the slices.
  VAEncryptionParameters crypto_params_ GUARDED_BY_CONTEXT(sequence_checker_);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // We need to set this so we don't resubmit crypto params on decode.
  bool full_sample_ GUARDED_BY_CONTEXT(sequence_checker_);

  // The most recent SPS and PPS, assumed to be active when samples are fully
  // encrypted.
  std::vector<uint8_t> last_sps_nalu_data_
      GUARDED_BY_CONTEXT(sequence_checker_);
  std::vector<uint8_t> last_pps_nalu_data_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace media

#endif  // MEDIA_GPU_VAAPI_H264_VAAPI_VIDEO_DECODER_DELEGATE_H_
