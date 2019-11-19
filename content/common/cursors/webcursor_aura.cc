// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/cursors/webcursor.h"

#include "base/logging.h"
#include "third_party/blink/public/platform/web_cursor_info.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/cursor_util.h"

namespace content {

gfx::NativeCursor WebCursor::GetNativeCursor() {
  if (info_.type == ui::CursorType::kCustom) {
    ui::Cursor cursor(ui::CursorType::kCustom);
    SkBitmap bitmap;
    gfx::Point hotspot;
    float scale;
    CreateScaledBitmapAndHotspotFromCustomData(&bitmap, &hotspot, &scale);
    cursor.set_custom_bitmap(bitmap);
    cursor.set_custom_hotspot(hotspot);
    cursor.set_device_scale_factor(scale);
    cursor.SetPlatformCursor(GetPlatformCursor(cursor));
    return cursor;
  }
  return info_.type;
}

void WebCursor::CreateScaledBitmapAndHotspotFromCustomData(SkBitmap* bitmap,
                                                           gfx::Point* hotspot,
                                                           float* scale) {
  *bitmap = info_.custom_image;
  *hotspot = info_.hotspot;
  *scale = GetCursorScaleFactor(bitmap);
  ui::ScaleAndRotateCursorBitmapAndHotpoint(*scale, rotation_, bitmap, hotspot);
}

#if !defined(USE_OZONE)
// ozone has its own SetDisplayInfo that takes rotation into account
void WebCursor::SetDisplayInfo(const display::Display& display) {
  if (device_scale_factor_ == display.device_scale_factor())
    return;

  device_scale_factor_ = display.device_scale_factor();
  CleanupPlatformData();
}

// ozone also has extra calculations for scale factor (taking max cursor size
// into account).
float WebCursor::GetCursorScaleFactor(SkBitmap* bitmap) {
  DCHECK_NE(0, info_.image_scale_factor);
  return device_scale_factor_ / info_.image_scale_factor;
}
#endif

}  // namespace content
