// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/video_frame_init_util.h"

#include <stdint.h>
#include <cmath>
#include <limits>

#include "media/base/limits.h"
#include "media/base/video_frame.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_rect_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_frame_buffer_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_frame_init.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame_rect_util.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

template <typename T>
gfx::Size ParseAndValidateDisplaySizeImpl(T* init,
                                          ExceptionState& exception_state) {
  DCHECK(init->hasDisplayWidth() || init->hasDisplayHeight());

  if (!init->hasDisplayWidth()) {
    exception_state.ThrowTypeError(
        "displayHeight specified without displayWidth.");
    return gfx::Size();
  }
  if (!init->hasDisplayHeight()) {
    exception_state.ThrowTypeError(
        "displayWidth specified without displayHeight.");
    return gfx::Size();
  }

  uint32_t display_width = init->displayWidth();
  uint32_t display_height = init->displayHeight();
  if (display_width == 0) {
    exception_state.ThrowTypeError("displayWidth must be nonzero.");
    return gfx::Size();
  }
  if (display_height == 0) {
    exception_state.ThrowTypeError("displayHeight must be nonzero.");
    return gfx::Size();
  }

  // Check that display size does not exceed dimension limits in
  // media::VideoFrame::IsValidSize().
  //
  // Note that at large display sizes, it can become impossible to allocate
  // a texture large enough to render into. It may be impossible, for example,
  // to create an ImageBitmap without also scaling down.
  if (display_width > media::limits::kMaxDimension ||
      display_height > media::limits::kMaxDimension) {
    exception_state.ThrowTypeError(
        String::Format("Invalid display size (%u, %u); exceeds "
                       "implementation limit.",
                       display_width, display_height));
    return gfx::Size();
  }

  return gfx::Size(static_cast<int>(display_width),
                   static_cast<int>(display_height));
}

gfx::Size ParseAndValidateDisplaySize(const VideoFrameInit* init,
                                      ExceptionState& exception_state) {
  return ParseAndValidateDisplaySizeImpl(init, exception_state);
}

gfx::Size ParseAndValidateDisplaySize(const VideoFrameBufferInit* init,
                                      ExceptionState& exception_state) {
  return ParseAndValidateDisplaySizeImpl(init, exception_state);
}

// Depending on |init|, this method potentially _overrides_ given "default"
// values for |visible_rect| and |display_size|.
ParsedVideoFrameInit::ParsedVideoFrameInit(
    const VideoFrameInit* init,
    media::VideoPixelFormat format,
    const gfx::Size& coded_size,
    const gfx::Rect& default_visible_rect,
    const gfx::Size& default_display_size,
    ExceptionState& exception_state) {
  // Defaults shouldn't be empty.
  DCHECK(!default_visible_rect.IsEmpty());
  DCHECK(!default_display_size.IsEmpty());
  visible_rect = default_visible_rect;
  display_size = default_display_size;

  // Override visible rect from init.
  if (init->hasVisibleRect()) {
    visible_rect = ToGfxRect(init->visibleRect(), "visibleRect", coded_size,
                             exception_state);
    if (exception_state.HadException())
      return;

    if (visible_rect.width() == 0) {
      exception_state.ThrowTypeError("visibleRect.width must be nonzero.");
      return;
    }

    if (visible_rect.height() == 0) {
      exception_state.ThrowTypeError("visibleRect.height must be nonzero.");
      return;
    }

    ValidateOffsetAlignment(format, visible_rect, "visibleRect",
                            exception_state);
    if (exception_state.HadException())
      return;
  }

  // Override display size from init.
  if (init->hasDisplayWidth() || init->hasDisplayHeight()) {
    display_size = ParseAndValidateDisplaySize(init, exception_state);
    if (exception_state.HadException())
      return;

    // Override display size with computed size scaled from visible rect.
  } else if (init->hasVisibleRect()) {
    double widthScale =
        default_display_size.width() / default_visible_rect.width();
    double heightScale =
        default_display_size.height() / default_visible_rect.height();
    display_size = gfx::Size(std::round(visible_rect.width() * widthScale),
                             std::round(visible_rect.height() * heightScale));
    if (display_size.width() == 0) {
      exception_state.ThrowTypeError("computed displayWidth must be nonzero");
      return;
    }

    if (display_size.height() == 0) {
      exception_state.ThrowTypeError("computed displayHeight must be nonzero");
      return;
    }
  }
}

}  // namespace blink
