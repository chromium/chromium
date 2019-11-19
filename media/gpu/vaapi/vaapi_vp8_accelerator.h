// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_VAAPI_VP8_ACCELERATOR_H_
#define MEDIA_GPU_VAAPI_VAAPI_VP8_ACCELERATOR_H_

#include "base/sequence_checker.h"
#include "media/gpu/vp8_decoder.h"
#include "media/parsers/vp8_parser.h"

namespace media {

template <class T> class DecodeSurfaceHandler;
class VASurface;
class VP8Picture;
class VaapiWrapper;

class VaapiVP8Accelerator : public VP8Decoder::VP8Accelerator {
 public:
  VaapiVP8Accelerator(DecodeSurfaceHandler<VASurface>* vaapi_dec,
                      scoped_refptr<VaapiWrapper> vaapi_wrapper);
  ~VaapiVP8Accelerator() override;

  // VP8Decoder::VP8Accelerator implementation.
  scoped_refptr<VP8Picture> CreateVP8Picture() override;
  bool SubmitDecode(scoped_refptr<VP8Picture> picture,
                    const Vp8ReferenceFrameVector& reference_frames) override;
  bool OutputPicture(const scoped_refptr<VP8Picture>& pic) override;

 private:
  const scoped_refptr<VaapiWrapper> vaapi_wrapper_;
  DecodeSurfaceHandler<VASurface>* vaapi_dec_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(VaapiVP8Accelerator);
};

}  // namespace media

#endif  // MEDIA_GPU_VAAPI_VAAPI_VP8_ACCELERATOR_H_
