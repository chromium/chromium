// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_V4L2_V4L2_VIDEO_DECODER_DELEGATE_VP8_H_
#define MEDIA_GPU_V4L2_V4L2_VIDEO_DECODER_DELEGATE_VP8_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "media/gpu/vp8_decoder.h"

namespace media {

class V4L2Device;
class V4L2DecodeSurface;
class V4L2DecodeSurfaceHandler;

class V4L2VideoDecoderDelegateVP8 : public VP8Decoder::VP8Accelerator {
 public:
  explicit V4L2VideoDecoderDelegateVP8(
      V4L2DecodeSurfaceHandler* surface_handler,
      V4L2Device* device);

  V4L2VideoDecoderDelegateVP8(const V4L2VideoDecoderDelegateVP8&) = delete;
  V4L2VideoDecoderDelegateVP8& operator=(const V4L2VideoDecoderDelegateVP8&) =
      delete;

  ~V4L2VideoDecoderDelegateVP8() override;

  // VP8Decoder::VP8Accelerator implementation.
  scoped_refptr<VP8Picture> CreateVP8Picture() override;
  bool SubmitDecode(scoped_refptr<VP8Picture> pic,
                    const Vp8ReferenceFrameVector& reference_frames) override;
  bool OutputPicture(scoped_refptr<VP8Picture> pic) override;

 private:
  scoped_refptr<V4L2DecodeSurface> VP8PictureToV4L2DecodeSurface(
      VP8Picture* pic);

  raw_ptr<V4L2DecodeSurfaceHandler> const surface_handler_;
  raw_ptr<V4L2Device> const device_;
};

}  // namespace media

#endif  // MEDIA_GPU_V4L2_V4L2_VIDEO_DECODER_DELEGATE_VP8_H_
