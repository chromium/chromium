// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/cursors/webcursor.h"

#include <windows.h>

#include "third_party/blink/public/platform/web_cursor_info.h"
#include "ui/gfx/icon_util.h"

namespace content {

ui::PlatformCursor WebCursor::GetPlatformCursor(const ui::Cursor& cursor) {
  if (info_.type != ui::CursorType::kCustom)
    return LoadCursor(nullptr, IDC_ARROW);

  if (platform_cursor_)
    return platform_cursor_;

  platform_cursor_ = IconUtil::CreateCursorFromSkBitmap(cursor.GetBitmap(),
                                                        cursor.GetHotspot())
                         .release();
  return platform_cursor_;
}

bool WebCursor::IsPlatformDataEqual(const WebCursor& other) const {
  return true;
}

void WebCursor::CleanupPlatformData() {
  if (platform_cursor_) {
    DestroyIcon(platform_cursor_);
    platform_cursor_ = nullptr;
  }
}

void WebCursor::CopyPlatformData(const WebCursor& other) {
  device_scale_factor_ = other.device_scale_factor_;
}

}  // namespace content
