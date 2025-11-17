// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_FLUSH_REASON_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_FLUSH_REASON_H_

namespace blink {

// Reasons for requesting that recorded PaintOps be flushed. Used in code
// loosely related to 2d canvas rendering contexts.
enum class FlushReason {
  // Canvas contents were cleared. This makes the canvas vector printable
  // again.
  kClear = 0,

  // The canvas element dispatched a frame to the compositor
  // This inhibits vector printing.
  kCanvasPushFrame = 1,

  // The canvas element dispatched a frame to the compositor while printing
  // was in progress.
  // This does not prevent vector printing as long as the current frame is
  // clear.
  kCanvasPushFrameWhilePrinting = 2,

  // The canvas is being printed.
  kPrinting = 3,

  kOther = 4,

  kMaxValue = kOther,
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_FLUSH_REASON_H_
