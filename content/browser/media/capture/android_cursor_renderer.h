// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CAPTURE_ANDROID_CURSOR_RENDERER_H_
#define CONTENT_BROWSER_MEDIA_CAPTURE_ANDROID_CURSOR_RENDERER_H_

#include "content/common/content_export.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_ui_types.h"

// TODO(b/486019140): Remove this once we use the system cursors directly.
namespace content {

// Helper class to render a cursor bitmap
class CONTENT_EXPORT AndroidCursorRenderer {
 public:
  // Returns a bitmap representation of a cursor.
  // The returned bitmap is in physical device pixels, scaled to ensure it
  // is sharp on high DPI displays.
  static SkBitmap GenerateCursorImage(float scale);

  // Returns the logical size of the cursor in DIPs (Device Independent Pixels).
  // Note: This size does NOT represent the pixel dimensions of the SkBitmap
  // returned by `GenerateCursorImage`, which is scaled for physical screens.
  static gfx::Size GetCursorSize();

  // Returns the logical hotspot of the cursor in DIPs.
  // Note: Like size, this is in logical units. If positioning the physical
  // SkBitmap, this hotspot must be scaled by the current display's DIP scale.
  static gfx::Point GetCursorHotspot();
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CAPTURE_ANDROID_CURSOR_RENDERER_H_
