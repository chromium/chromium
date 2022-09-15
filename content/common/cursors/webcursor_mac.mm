// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/cursors/webcursor.h"

#include "ui/base/cocoa/cursor_utils.h"

namespace content {

// Match Safari's cursor choices; see platform/mac/CursorMac.mm .
gfx::NativeCursor WebCursor::GetNativeCursor() {
  return ui::GetNativeCursor(cursor_);
}

void WebCursor::CleanupPlatformData() {}

}  // namespace content
