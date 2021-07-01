// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_PARSED_COPY_TO_OPTIONS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_PARSED_COPY_TO_OPTIONS_H_

#include <stdint.h>

#include "media/base/video_frame.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"
#include "ui/gfx/geometry/rect.h"

namespace blink {

class ExceptionState;
class VideoFrameCopyToOptions;

struct ParsedCopyToOptions {
  ParsedCopyToOptions(VideoFrameCopyToOptions* options,
                      media::VideoPixelFormat format,
                      const gfx::Size& coded_size,
                      const gfx::Rect& default_rect,
                      ExceptionState&);

  struct Plane {
    // Offset in destination buffer.
    uint32_t offset = 0;
    // Stride in destination buffer.
    uint32_t stride = 0;

    // Crop top, in samples.
    uint32_t top = 0;
    // Crop height, in samples.
    uint32_t height = 0;
    // Crop left, in bytes.
    uint32_t left_bytes = 0;
    // Crop width, in bytes (aka row_bytes).
    uint32_t width_bytes = 0;
  };

  const wtf_size_t num_planes;
  Plane planes[media::VideoFrame::kMaxPlanes] = {{0}};

  // Region of coded area to copy.
  gfx::Rect rect;

  // Minimum size of a destination buffer that fits all planes.
  uint32_t min_buffer_size = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_PARSED_COPY_TO_OPTIONS_H_
