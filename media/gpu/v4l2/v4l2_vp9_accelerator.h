// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_V4L2_V4L2_VP9_ACCELERATOR_H_
#define MEDIA_GPU_V4L2_V4L2_VP9_ACCELERATOR_H_

#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "media/filters/vp9_parser.h"
#include "media/gpu/vp9_decoder.h"

namespace media {

class V4L2DecodeSurface;
class V4L2DecodeSurfaceHandler;
class V4L2Device;

class V4L2VP9Accelerator : public VP9Decoder::VP9Accelerator {
 public:
  explicit V4L2VP9Accelerator(V4L2DecodeSurfaceHandler* surface_handler,
                              V4L2Device* device);
  ~V4L2VP9Accelerator() override;

  // VP9Decoder::VP9Accelerator implementation.
  scoped_refptr<VP9Picture> CreateVP9Picture() override;

  bool SubmitDecode(scoped_refptr<VP9Picture> pic,
                    const Vp9SegmentationParams& segm_params,
                    const Vp9LoopFilterParams& lf_params,
                    const Vp9ReferenceFrameVector& reference_frames,
                    const base::Closure& done_cb) override;

  bool OutputPicture(scoped_refptr<VP9Picture> pic) override;

  bool GetFrameContext(scoped_refptr<VP9Picture> pic,
                       Vp9FrameContext* frame_ctx) override;

  bool IsFrameContextRequired() const override;

 private:
  scoped_refptr<V4L2DecodeSurface> VP9PictureToV4L2DecodeSurface(
      VP9Picture* pic);

  bool device_needs_frame_context_;

  V4L2DecodeSurfaceHandler* const surface_handler_;
  V4L2Device* const device_;

  DISALLOW_COPY_AND_ASSIGN(V4L2VP9Accelerator);
};

}  // namespace media

#endif  // MEDIA_GPU_V4L2_V4L2_VP9_ACCELERATOR_H_
