// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/stateless/vp9_delegate.h"

// ChromeOS specific header; does not exist upstream
#if BUILDFLAG(IS_CHROMEOS)
#include <linux/media/vp9-ctrls-upstream.h>
#endif

#include "base/logging.h"
#include "base/numerics/safe_math.h"
#include "media/filters/vp9_parser.h"
#include "media/gpu/macros.h"
#include "media/gpu/v4l2/stateless/stateless_decode_surface_handler.h"
#include "media/gpu/v4l2/v4l2_decode_surface.h"

namespace media {

using DecodeStatus = VP9Decoder::VP9Accelerator::Status;

VP9Delegate::VP9Delegate(StatelessDecodeSurfaceHandler* surface_handler,
                         bool supports_compressed_header)
    : surface_handler_(surface_handler),
      supports_compressed_header_(supports_compressed_header) {
  VLOGF(1);
  DCHECK(surface_handler_);
}

VP9Delegate::~VP9Delegate() = default;

scoped_refptr<VP9Picture> VP9Delegate::CreateVP9Picture() {
  DVLOGF(4);

  return new VP9Picture();
}

DecodeStatus VP9Delegate::SubmitDecode(
    scoped_refptr<VP9Picture> pic,
    const Vp9SegmentationParams& segm_params,
    const Vp9LoopFilterParams& lf_params,
    const Vp9ReferenceFrameVector& ref_frames,
    base::OnceClosure done_cb) {
  DVLOGF(4);
  NOTREACHED();
  return DecodeStatus::kOk;
}

bool VP9Delegate::OutputPicture(scoped_refptr<VP9Picture> pic) {
  DVLOGF(4);
  NOTREACHED();
  return true;
}

bool VP9Delegate::GetFrameContext(scoped_refptr<VP9Picture> pic,
                                  Vp9FrameContext* frame_ctx) {
  NOTIMPLEMENTED() << "Frame context update not supported";
  return false;
}

bool VP9Delegate::NeedsCompressedHeaderParsed() const {
  return supports_compressed_header_;
}

bool VP9Delegate::SupportsContextProbabilityReadback() const {
  return false;
}

}  // namespace media
