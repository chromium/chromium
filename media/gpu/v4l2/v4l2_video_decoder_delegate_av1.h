// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_V4L2_V4L2_VIDEO_DECODER_DELEGATE_AV1_H_
#define MEDIA_GPU_V4L2_V4L2_VIDEO_DECODER_DELEGATE_AV1_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "media/gpu/av1_decoder.h"

#ifndef V4L2_AV1_RESTORATION_TILESIZE_MAX
#define V4L2_AV1_RESTORATION_TILESIZE_MAX 256
#endif

namespace media {

class V4L2DecodeSurfaceHandler;
class V4L2Device;

class V4L2VideoDecoderDelegateAV1 : public AV1Decoder::AV1Accelerator {
 public:
  V4L2VideoDecoderDelegateAV1(V4L2DecodeSurfaceHandler* surface_handler,
                              V4L2Device* device);

  V4L2VideoDecoderDelegateAV1(const V4L2VideoDecoderDelegateAV1&) = delete;
  V4L2VideoDecoderDelegateAV1& operator=(const V4L2VideoDecoderDelegateAV1&) =
      delete;

  ~V4L2VideoDecoderDelegateAV1() override;

  // AV1Decoder::AV1Accelerator implementation.
  scoped_refptr<AV1Picture> CreateAV1Picture(bool apply_grain) override;
  scoped_refptr<AV1Picture> CreateAV1PictureSecure(
      bool apply_grain,
      uint64_t secure_handle) override;

  Status SubmitDecode(const AV1Picture& pic,
                      const libgav1::ObuSequenceHeader& sequence_header,
                      const AV1ReferenceFrameVector& ref_frames,
                      const libgav1::Vector<libgav1::TileBuffer>& tile_buffers,
                      base::span<const uint8_t> stream) override;

  bool OutputPicture(const AV1Picture& pic) override;

 private:
  raw_ptr<V4L2DecodeSurfaceHandler> const surface_handler_;
  raw_ptr<V4L2Device> const device_;
};

}  // namespace media

#endif  // MEDIA_GPU_V4L2_V4L2_VIDEO_DECODER_DELEGATE_AV1_H_
