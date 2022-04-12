// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_V4L2_V4L2_VIDEO_DECODER_DELEGATE_VP9_CHROMIUM_H_
#define MEDIA_GPU_V4L2_V4L2_VIDEO_DECODER_DELEGATE_VP9_CHROMIUM_H_

#include "media/gpu/vp9_decoder.h"

namespace media {

class V4L2DecodeSurface;
class V4L2DecodeSurfaceHandler;
class V4L2Device;

class V4L2VideoDecoderDelegateVP9Chromium : public VP9Decoder::VP9Accelerator {
 public:
  explicit V4L2VideoDecoderDelegateVP9Chromium(
      V4L2DecodeSurfaceHandler* surface_handler,
      V4L2Device* device);

  V4L2VideoDecoderDelegateVP9Chromium(
      const V4L2VideoDecoderDelegateVP9Chromium&) = delete;
  V4L2VideoDecoderDelegateVP9Chromium& operator=(
      const V4L2VideoDecoderDelegateVP9Chromium&) = delete;

  ~V4L2VideoDecoderDelegateVP9Chromium() override;

  // VP9Decoder::VP9Accelerator implementation.
  scoped_refptr<VP9Picture> CreateVP9Picture() override;

  Status SubmitDecode(scoped_refptr<VP9Picture> pic,
                      const Vp9SegmentationParams& segm_params,
                      const Vp9LoopFilterParams& lf_params,
                      const Vp9ReferenceFrameVector& reference_frames,
                      base::OnceClosure done_cb) override;

  bool OutputPicture(scoped_refptr<VP9Picture> pic) override;

  bool GetFrameContext(scoped_refptr<VP9Picture> pic,
                       Vp9FrameContext* frame_ctx) override;

  bool NeedsCompressedHeaderParsed() const override;
  bool SupportsContextProbabilityReadback() const override;

 private:
  V4L2DecodeSurfaceHandler* const surface_handler_;
  V4L2Device* const device_;

  // True if |device_| exposes the V4L2_CID_STATELESS_VP9_FRAME control
  // (indicating that the driver needs the entropy tables from the compressed
  // header).
  const bool device_needs_compressed_header_parsed_;
};

}  // namespace media

#endif  // MEDIA_GPU_V4L2_V4L2_VIDEO_DECODER_DELEGATE_VP9_CHROMIUM_H_
