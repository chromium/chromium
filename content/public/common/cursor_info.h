// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_CURSOR_INFO_H_
#define CONTENT_PUBLIC_COMMON_CURSOR_INFO_H_

#include "content/common/content_export.h"
#include "third_party/blink/public/platform/web_cursor_info.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace content {

// This struct represents the data sufficient to create a cross-platform cursor:
// either a predefined cursor type (from WebCursorInfo) or custom image.
// This structure is highly similar to blink::WebCursorInfo.
struct CONTENT_EXPORT CursorInfo {
  CursorInfo() = default;
  explicit CursorInfo(ui::CursorType cursor);
  explicit CursorInfo(const blink::WebCursorInfo& info);

  // Equality operator; performs bitmap content comparison as needed.
  bool operator==(const CursorInfo& other) const;

  // Get a blink::WebCursorInfo struct with fields matching this struct.
  blink::WebCursorInfo GetWebCursorInfo() const;

  // One of the predefined cursors.
  ui::CursorType type = ui::CursorType::kPointer;

  // Custom cursor image.
  SkBitmap custom_image;

  // Hotspot in custom image in pixels.
  gfx::Point hotspot;

  // The scale factor of custom image, used to possibly re-scale the image
  // for a different density display.
  float image_scale_factor = 1.f;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_CURSOR_INFO_H_
