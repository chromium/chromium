// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/cursors/webcursor.h"

#include "base/check_op.h"
#include "build/build_config.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/cursor_factory.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/wm/core/cursor_util.h"

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
      custom_cursor_->set_image_scale_factor(device_scale_factor_);
      custom_cursor_->SetPlatformCursor(
          ui::CursorFactory::GetInstance()->CreateImageCursor(
              ui::mojom::CursorType::kCustom, bitmap, hotspot));
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
  wm::ScaleAndRotateCursorBitmapAndHotpoint(*scale, rotation_, bitmap, hotspot);
}

#if !BUILDFLAG(IS_OZONE)
// ozone has its own SetDisplayInfo that takes rotation into account
void WebCursor::SetDisplayInfo(const display::Display& display) {
  if (device_scale_factor_ == display.device_scale_factor())
    return;

  device_scale_factor_ = display.device_scale_factor();
  custom_cursor_.reset();
}

// ozone also has extra calculations for scale factor (taking max cursor size
// into account).
float WebCursor::GetCursorScaleFactor(SkBitmap* bitmap) {
  DCHECK_NE(0, cursor_.image_scale_factor());
  return device_scale_factor_ / cursor_.image_scale_factor();
}
#endif

}  // namespace content
