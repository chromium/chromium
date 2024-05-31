// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_VP8_VAAPI_VIDEO_DECODER_DELEGATE_H_
#define MEDIA_GPU_VAAPI_VP8_VAAPI_VIDEO_DECODER_DELEGATE_H_

#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "media/gpu/vaapi/vaapi_video_decoder_delegate.h"
#include "media/gpu/vp8_decoder.h"
#include "media/parsers/vp8_parser.h"

namespace media {

class ScopedVABuffer;
class VP8Picture;

class VP8VaapiVideoDecoderDelegate : public VP8Decoder::VP8Accelerator,
                                     public VaapiVideoDecoderDelegate {
 public:
  VP8VaapiVideoDecoderDelegate(VaapiDecodeSurfaceHandler* const vaapi_dec,
                               scoped_refptr<VaapiWrapper> vaapi_wrapper);

  VP8VaapiVideoDecoderDelegate(const VP8VaapiVideoDecoderDelegate&) = delete;
  VP8VaapiVideoDecoderDelegate& operator=(const VP8VaapiVideoDecoderDelegate&) =
      delete;

  ~VP8VaapiVideoDecoderDelegate() override;

  // VP8Decoder::VP8Accelerator implementation.
  scoped_refptr<VP8Picture> CreateVP8Picture() override;
  bool SubmitDecode(scoped_refptr<VP8Picture> picture,
                    const Vp8ReferenceFrameVector& reference_frames) override;
  bool OutputPicture(scoped_refptr<VP8Picture> pic) override;

  // VaapiVideoDecoderDelegate impl.
  void OnVAContextDestructionSoon() override;

 private:
  std::unique_ptr<ScopedVABuffer> iq_matrix_
      GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<ScopedVABuffer> prob_buffer_
      GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<ScopedVABuffer> picture_params_
      GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<ScopedVABuffer> slice_params_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace media

#endif  // MEDIA_GPU_VAAPI_VP8_VAAPI_VIDEO_DECODER_DELEGATE_H_
