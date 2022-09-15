// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/cursors/webcursor.h"


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

void WebCursor::CleanupPlatformData() {}

}  // namespace content
