// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_V4L2_STATELESS_VP9_DELEGATE_H_
#define MEDIA_GPU_V4L2_STATELESS_VP9_DELEGATE_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "media/gpu/vp9_decoder.h"

namespace media {

class StatelessDecodeSurfaceHandler;

class VP9Delegate : public VP9Decoder::VP9Accelerator {
 public:
  explicit VP9Delegate(StatelessDecodeSurfaceHandler* surface_handler,
                       bool supports_compressed_header);

  VP9Delegate(const VP9Delegate&) = delete;
  VP9Delegate& operator=(const VP9Delegate&) = delete;

  ~VP9Delegate() override;

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
  raw_ptr<StatelessDecodeSurfaceHandler> const surface_handler_;

  // True if |driver_| supports V4L2_CID_STATELESS_VP9_COMPRESSED_HDR. Not all
  // implementations are expected to support it (e.g. MTK8195 doesn't).
  const bool supports_compressed_header_;
};

}  // namespace media

#endif  // MEDIA_GPU_V4L2_STATELESS_VP9_DELEGATE_H_
