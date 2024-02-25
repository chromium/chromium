// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_V4L2_STATELESS_VP8_DELEGATE_H_
#define MEDIA_GPU_V4L2_STATELESS_VP8_DELEGATE_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "media/gpu/vp8_decoder.h"

namespace media {

class StatelessDecodeSurfaceHandler;

class VP8Delegate : public VP8Decoder::VP8Accelerator {
 public:
  explicit VP8Delegate(StatelessDecodeSurfaceHandler* surface_handler);

  VP8Delegate(const VP8Delegate&) = delete;
  VP8Delegate& operator=(const VP8Delegate&) = delete;

  ~VP8Delegate() override;

  // VP8Decoder::VP8Accelerator implementation.
  scoped_refptr<VP8Picture> CreateVP8Picture() override;
  bool SubmitDecode(scoped_refptr<VP8Picture> pic,
                    const Vp8ReferenceFrameVector& reference_frames) override;
  bool OutputPicture(scoped_refptr<VP8Picture> pic) override;

 private:
  raw_ptr<StatelessDecodeSurfaceHandler> const surface_handler_;
};

}  // namespace media

#endif  // MEDIA_GPU_V4L2_STATELESS_VP8_DELEGATE_H_
