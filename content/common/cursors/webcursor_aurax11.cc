// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/cursors/webcursor.h"


#include "base/logging.h"
#include "third_party/blink/public/platform/web_cursor_info.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/cursor_loader_x11.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/x/x11.h"

namespace content {

ui::PlatformCursor WebCursor::GetPlatformCursor(const ui::Cursor& cursor) {
  if (platform_cursor_)
    return platform_cursor_;

  SkBitmap bitmap = cursor.GetBitmap();

  XcursorImage* image =
      ui::SkBitmapToXcursorImage(&bitmap, cursor.GetHotspot());
  platform_cursor_ = ui::CreateReffedCustomXCursor(image);
  return platform_cursor_;
}

bool WebCursor::IsPlatformDataEqual(const WebCursor& other) const {
  return true;
}

void WebCursor::CleanupPlatformData() {
  if (platform_cursor_) {
    ui::UnrefCustomXCursor(platform_cursor_);
    platform_cursor_ = 0;
  }
}

void WebCursor::CopyPlatformData(const WebCursor& other) {
  if (platform_cursor_)
    ui::UnrefCustomXCursor(platform_cursor_);
  platform_cursor_ = other.platform_cursor_;
  if (platform_cursor_)
    ui::RefCustomXCursor(platform_cursor_);

  device_scale_factor_ = other.device_scale_factor_;
}

}  // namespace content
