// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_VP9_VAAPI_VIDEO_DECODER_DELEGATE_H_
#define MEDIA_GPU_VAAPI_VP9_VAAPI_VIDEO_DECODER_DELEGATE_H_

#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "media/gpu/vaapi/vaapi_video_decoder_delegate.h"
#include "media/gpu/vp9_decoder.h"
#include "media/parsers/vp9_parser.h"

namespace media {

class ScopedVABuffer;
class VP9Picture;

class VP9VaapiVideoDecoderDelegate : public VP9Decoder::VP9Accelerator,
                                     public VaapiVideoDecoderDelegate {
 public:
  VP9VaapiVideoDecoderDelegate(
      VaapiDecodeSurfaceHandler* const vaapi_dec,
      scoped_refptr<VaapiWrapper> vaapi_wrapper,
      ProtectedSessionUpdateCB on_protected_session_update_cb =
          base::DoNothing(),
      CdmContext* cdm_context = nullptr,
      EncryptionScheme encryption_scheme = EncryptionScheme::kUnencrypted);

  VP9VaapiVideoDecoderDelegate(const VP9VaapiVideoDecoderDelegate&) = delete;
  VP9VaapiVideoDecoderDelegate& operator=(const VP9VaapiVideoDecoderDelegate&) =
      delete;

  ~VP9VaapiVideoDecoderDelegate() override;

  // VP9Decoder::VP9Accelerator implementation.
  scoped_refptr<VP9Picture> CreateVP9Picture() override;
  Status SubmitDecode(scoped_refptr<VP9Picture> pic,
                      const Vp9SegmentationParams& seg,
                      const Vp9LoopFilterParams& lf,
                      const Vp9ReferenceFrameVector& reference_frames) override;

  bool OutputPicture(scoped_refptr<VP9Picture> pic) override;
  bool NeedsCompressedHeaderParsed() const override;

  // VaapiVideoDecoderDelegate impl.
  void OnVAContextDestructionSoon() override;

 private:
  std::unique_ptr<ScopedVABuffer> picture_params_;
  std::unique_ptr<ScopedVABuffer> slice_params_;
  std::unique_ptr<ScopedVABuffer> crypto_params_;
  std::unique_ptr<ScopedVABuffer> protected_params_;
};

}  // namespace media

#endif  // MEDIA_GPU_VAAPI_VP9_VAAPI_VIDEO_DECODER_DELEGATE_H_
