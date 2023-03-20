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
      SkBitmap bitmap = cursor_.custom_bitmap();
      gfx::Point hotspot = cursor_.custom_hotspot();
      float scale = GetCursorScaleFactor(&bitmap);
      wm::ScaleAndRotateCursorBitmapAndHotpoint(scale, rotation_, &bitmap,
                                                &hotspot);
      custom_cursor_ = ui::Cursor::NewCustom(
          std::move(bitmap), std::move(hotspot), device_scale_factor_);
      custom_cursor_->SetPlatformCursor(
          ui::CursorFactory::GetInstance()->CreateImageCursor(
              custom_cursor_->type(), custom_cursor_->custom_bitmap(),
              custom_cursor_->custom_hotspot()));
    }
    return *custom_cursor_;
  }
  return cursor_.type();
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
// Ash has its own SetDisplayInfo that takes rotation into account.
void WebCursor::SetDisplayInfo(const display::Display& display) {
  if (device_scale_factor_ == display.device_scale_factor())
    return;

  device_scale_factor_ = display.device_scale_factor();
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
