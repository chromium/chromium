// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/check_op.h"
#include "build/build_config.h"
#include "content/common/cursors/webcursor.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/cursor_factory.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/wm/core/cursor_loader.h"
#include "ui/wm/core/cursor_util.h"

namespace content {

gfx::NativeCursor WebCursor::GetNativeCursor() {
  if (cursor_.type() == ui::mojom::CursorType::kCustom) {
    if (!custom_cursor_) {
      SkBitmap bitmap = cursor_.custom_bitmap();
      gfx::Point hotspot = cursor_.custom_hotspot();
      float cursor_image_scale = device_scale_factor_;
      wm::ScaleAndRotateCursorBitmapAndHotpoint(GetCursorScaleFactor(&bitmap),
                                                rotation_, &bitmap, &hotspot);
      ui::Cursor custom_cursor =
          ui::Cursor::NewCustom(std::move(bitmap), hotspot, cursor_image_scale);

      auto cursor_data =
          aura::client::GetCursorShapeClient().GetCursorData(custom_cursor);

      custom_cursor.SetPlatformCursor(
          ui::CursorFactory::GetInstance()->CreateImageCursor(
              custom_cursor.type(), std::move(cursor_data->bitmaps[0]),
              cursor_data->hotspot, cursor_data->scale_factor));

      custom_cursor_ = custom_cursor;
    }
    return *custom_cursor_;
  }
  return cursor_.type();
}

#if !BUILDFLAG(IS_CHROMEOS)
// Ash has its own UpdateDisplayInfoForWindow that takes rotation into account.
void WebCursor::UpdateDisplayInfoForWindow(aura::Window* window) {
  float preferred_scale = display::Screen::GetScreen()
                              ->GetPreferredScaleFactorForWindow(window)
                              .value_or(1.0f);
  if (device_scale_factor_ == preferred_scale) {
    return;
  }

  device_scale_factor_ = preferred_scale;
  custom_cursor_.reset();
}

// Ash also has extra calculations for scale factor (taking max cursor size
// into account).
float WebCursor::GetCursorScaleFactor(SkBitmap* bitmap) {
  DCHECK_NE(0, cursor_.image_scale_factor());
  return device_scale_factor_ / cursor_.image_scale_factor();
}
#endif

}  // namespace content
