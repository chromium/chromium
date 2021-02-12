// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/cursors/webcursor.h"

#include "base/check_op.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/cursor_util.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"

namespace content {

gfx::NativeCursor WebCursor::GetNativeCursor() {
  if (cursor_.type() == ui::mojom::CursorType::kCustom) {
    if (!custom_cursor_) {
      custom_cursor_.emplace(ui::mojom::CursorType::kCustom);
      SkBitmap bitmap;
      gfx::Point hotspot;
      float scale;
      CreateScaledBitmapAndHotspotFromCustomData(&bitmap, &hotspot, &scale);
      custom_cursor_->set_custom_bitmap(bitmap);
      custom_cursor_->set_custom_hotspot(hotspot);
      custom_cursor_->set_image_scale_factor(scale);
      custom_cursor_->SetPlatformCursor(GetPlatformCursor(*custom_cursor_));
    }
    return *custom_cursor_;
  }
  return cursor_.type();
}

void WebCursor::CreateScaledBitmapAndHotspotFromCustomData(SkBitmap* bitmap,
                                                           gfx::Point* hotspot,
                                                           float* scale) {
  DCHECK_EQ(ui::mojom::CursorType::kCustom, cursor_.type());
  *bitmap = cursor_.custom_bitmap();
  *hotspot = cursor_.custom_hotspot();
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
  DCHECK_NE(0, cursor_.image_scale_factor());
  return device_scale_factor_ / cursor_.image_scale_factor();
}
#endif

}  // namespace content
