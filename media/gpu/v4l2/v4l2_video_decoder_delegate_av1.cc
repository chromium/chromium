// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/v4l2_video_decoder_delegate_av1.h"

#include "media/gpu/macros.h"
#include "media/gpu/v4l2/v4l2_decode_surface.h"
#include "media/gpu/v4l2/v4l2_decode_surface_handler.h"

namespace media {

using DecodeStatus = AV1Decoder::AV1Accelerator::Status;

class V4L2AV1Picture : public AV1Picture {
 public:
  explicit V4L2AV1Picture(scoped_refptr<V4L2DecodeSurface> dec_surface)
      : dec_surface_(std::move(dec_surface)) {}

  V4L2AV1Picture(const V4L2AV1Picture&) = delete;
  V4L2AV1Picture& operator=(const V4L2AV1Picture&) = delete;

  V4L2AV1Picture* AsV4L2AV1Picture() override { return this; }

 private:
  ~V4L2AV1Picture() override = default;

  scoped_refptr<AV1Picture> CreateDuplicate() override {
    return new V4L2AV1Picture(dec_surface_);
  }

  scoped_refptr<V4L2DecodeSurface> dec_surface_;
};

V4L2VideoDecoderDelegateAV1::V4L2VideoDecoderDelegateAV1(
    V4L2DecodeSurfaceHandler* surface_handler,
    V4L2Device* device)
    : surface_handler_(surface_handler), device_(device) {
  VLOGF(1);
  DCHECK(surface_handler_);
  DCHECK(device_);
}

V4L2VideoDecoderDelegateAV1::~V4L2VideoDecoderDelegateAV1() = default;

}  // namespace media
