// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_AV1_VAAPI_VIDEO_DECODER_DELEGATE_H_
#define MEDIA_GPU_VAAPI_AV1_VAAPI_VIDEO_DECODER_DELEGATE_H_

#include <memory>
#include <vector>

#include "media/gpu/av1_decoder.h"
#include "media/gpu/vaapi/vaapi_video_decoder_delegate.h"

namespace media {
class ScopedVABuffer;

class AV1VaapiVideoDecoderDelegate : public AV1Decoder::AV1Accelerator,
                                     public VaapiVideoDecoderDelegate {
 public:
  AV1VaapiVideoDecoderDelegate(
      VaapiDecodeSurfaceHandler* const vaapi_dec,
      scoped_refptr<VaapiWrapper> vaapi_wrapper,
      ProtectedSessionUpdateCB on_protected_session_update_cb =
          base::DoNothing(),
      CdmContext* cdm_context = nullptr,
      EncryptionScheme encryption_scheme = EncryptionScheme::kUnencrypted);
  ~AV1VaapiVideoDecoderDelegate() override;
  AV1VaapiVideoDecoderDelegate(const AV1VaapiVideoDecoderDelegate&) = delete;
  AV1VaapiVideoDecoderDelegate& operator=(const AV1VaapiVideoDecoderDelegate&) =
      delete;

  // AV1Decoder::AV1Accelerator implementation.
  scoped_refptr<AV1Picture> CreateAV1Picture(bool apply_grain) override;
  Status SubmitDecode(const AV1Picture& pic,
                      const libgav1::ObuSequenceHeader& seq_header,
                      const AV1ReferenceFrameVector& ref_frames,
                      const libgav1::Vector<libgav1::TileBuffer>& tile_buffers,
                      base::span<const uint8_t> data) override;
  bool OutputPicture(const AV1Picture& pic) override;

  // VaapiVideoDecoderDelegate implementation.
  void OnVAContextDestructionSoon() override;

 private:
  std::unique_ptr<ScopedVABuffer> picture_params_
      GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<ScopedVABuffer> crypto_params_
      GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<ScopedVABuffer> protected_params_
      GUARDED_BY_CONTEXT(sequence_checker_);
};
}  // namespace media
#endif  // MEDIA_GPU_VAAPI_AV1_VAAPI_VIDEO_DECODER_DELEGATE_H_
