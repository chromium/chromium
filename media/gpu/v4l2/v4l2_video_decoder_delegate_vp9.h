// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_V4L2_V4L2_VIDEO_DECODER_DELEGATE_VP9_H_
#define MEDIA_GPU_V4L2_V4L2_VIDEO_DECODER_DELEGATE_VP9_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "media/gpu/vp9_decoder.h"

namespace media {

class V4L2DecodeSurfaceHandler;
class V4L2Device;

class V4L2VideoDecoderDelegateVP9 : public VP9Decoder::VP9Accelerator {
 public:
  explicit V4L2VideoDecoderDelegateVP9(
      V4L2DecodeSurfaceHandler* surface_handler,
      V4L2Device* device);

  V4L2VideoDecoderDelegateVP9(const V4L2VideoDecoderDelegateVP9&) = delete;
  V4L2VideoDecoderDelegateVP9& operator=(const V4L2VideoDecoderDelegateVP9&) =
      delete;

  ~V4L2VideoDecoderDelegateVP9() override;

  // VP9Decoder::VP9Accelerator implementation.
  scoped_refptr<VP9Picture> CreateVP9Picture() override;
  scoped_refptr<VP9Picture> CreateVP9PictureSecure(
      uint64_t secure_handle) override;
  Status SubmitDecode(scoped_refptr<VP9Picture> pic,
                      const Vp9SegmentationParams& segm_params,
                      const Vp9LoopFilterParams& lf_params,
                      const Vp9ReferenceFrameVector& reference_frames) override;
  bool OutputPicture(scoped_refptr<VP9Picture> pic) override;
  bool NeedsCompressedHeaderParsed() const override;

 private:
  raw_ptr<V4L2DecodeSurfaceHandler> const surface_handler_;
  raw_ptr<V4L2Device> const device_;

  // True if |device_| supports V4L2_CID_STATELESS_VP9_COMPRESSED_HDR. Not all
  // implementations are expected to support it (e.g. MTK8195 doesn't).
  const bool supports_compressed_header_;
};

}  // namespace media

#endif  // MEDIA_GPU_V4L2_V4L2_VIDEO_DECODER_DELEGATE_VP9_H_
