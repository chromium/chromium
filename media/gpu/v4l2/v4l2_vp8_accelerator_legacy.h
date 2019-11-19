// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_V4L2_V4L2_VP8_ACCELERATOR_LEGACY_H_
#define MEDIA_GPU_V4L2_V4L2_VP8_ACCELERATOR_LEGACY_H_

#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "media/gpu/vp8_decoder.h"

namespace media {

class V4L2Device;
class V4L2DecodeSurface;
class V4L2DecodeSurfaceHandler;

class V4L2LegacyVP8Accelerator : public VP8Decoder::VP8Accelerator {
 public:
  explicit V4L2LegacyVP8Accelerator(V4L2DecodeSurfaceHandler* surface_handler,
                                    V4L2Device* device);
  ~V4L2LegacyVP8Accelerator() override;

  // VP8Decoder::VP8Accelerator implementation.
  scoped_refptr<VP8Picture> CreateVP8Picture() override;
  bool SubmitDecode(scoped_refptr<VP8Picture> pic,
                    const Vp8ReferenceFrameVector& reference_frames) override;
  bool OutputPicture(const scoped_refptr<VP8Picture>& pic) override;

 private:
  scoped_refptr<V4L2DecodeSurface> VP8PictureToV4L2DecodeSurface(
      const scoped_refptr<VP8Picture>& pic);

  V4L2DecodeSurfaceHandler* const surface_handler_;
  V4L2Device* const device_;

  DISALLOW_COPY_AND_ASSIGN(V4L2LegacyVP8Accelerator);
};

}  // namespace media

#endif  // MEDIA_GPU_V4L2_V4L2_VP8_ACCELERATOR_LEGACY_H_
