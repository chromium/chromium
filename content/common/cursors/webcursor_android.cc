// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/cursors/webcursor.h"

#include "base/logging.h"
#include "third_party/blink/public/platform/web_cursor_info.h"

namespace content {

gfx::NativeCursor WebCursor::GetNativeCursor() {
  return gfx::kNullCursor;
}

#if defined(USE_AURA)
// In the future when we want to support cursors of various kinds in Aura on
// Android, we should switch to using webcursor_aura rather than add an
// implementation here.
void WebCursor::SetDisplayInfo(const display::Display& display) {}
#endif

bool WebCursor::IsPlatformDataEqual(const WebCursor& other) const {
  return true;
}

void WebCursor::CleanupPlatformData() {}

void WebCursor::CopyPlatformData(const WebCursor& other) {}

}  // namespace content
