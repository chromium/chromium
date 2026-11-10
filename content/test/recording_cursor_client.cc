// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/recording_cursor_client.h"

namespace content {

RecordingCursorClient::~RecordingCursorClient() = default;

void RecordingCursorClient::SetCursor(gfx::NativeCursor cursor) {
  TestCursorClient::SetCursor(cursor);
  last_cursor_ = cursor;
}

gfx::NativeCursor RecordingCursorClient::GetCursor() const {
  return last_cursor_;
}

}  // namespace content
