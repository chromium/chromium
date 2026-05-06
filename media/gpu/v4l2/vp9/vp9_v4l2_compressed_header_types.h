// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_V4L2_VP9_VP9_V4L2_COMPRESSED_HEADER_TYPES_H_
#define MEDIA_GPU_V4L2_VP9_VP9_V4L2_COMPRESSED_HEADER_TYPES_H_

#include "media/gpu/media_gpu_export.h"
#include "media/parsers/vp9_parser.h"

namespace media {

// VP9 compressed header fields (spec section 6.3) used only by the V4L2
// stateless VP9 uAPI. Intentionally separate from `Vp9FrameHeader` so parsers
// stay agnostic of this HW-specific surface.
struct MEDIA_GPU_EXPORT Vp9CompressedHeader {
  enum Vp9TxMode {
    ONLY_4X4 = 0,
    ALLOW_8X8 = 1,
    ALLOW_16X16 = 2,
    ALLOW_32X32 = 3,
    TX_MODE_SELECT = 4,
    TX_MODES = 5,
  };

  Vp9TxMode tx_mode = ONLY_4X4;
  Vp9ReferenceMode reference_mode = SINGLE_REFERENCE;
};

// Output of parsing the VP9 compressed header for V4L2 controls.
struct MEDIA_GPU_EXPORT Vp9V4L2CompressedParseResult {
  Vp9CompressedHeader compressed_header;
  Vp9FrameContext frame_context;
};

}  // namespace media

#endif  // MEDIA_GPU_V4L2_VP9_VP9_V4L2_COMPRESSED_HEADER_TYPES_H_
