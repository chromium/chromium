// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/chromeos/mouse_cursor_monitor_aura.h"

#include <optional>
#include <utility>

#include "ash/shell.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "remoting/host/chromeos/skia_bitmap_desktop_frame.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor.h"
#include "ui/aura/client/cursor_shape_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/gfx/geometry/point.h"

namespace {

// Creates an empty webrtc::MouseCursor. The caller is responsible for
// destroying the returned cursor.
webrtc::MouseCursor* CreateEmptyMouseCursor() {
  return new webrtc::MouseCursor(
      new webrtc::BasicDesktopFrame(webrtc::DesktopSize(0, 0)),
      webrtc::DesktopVector(0, 0));
}

}  // namespace

namespace remoting {

MouseCursorMonitorAura::MouseCursorMonitorAura()
    : callback_(nullptr), mode_(SHAPE_AND_POSITION) {}

void MouseCursorMonitorAura::Init(Callback* callback, Mode mode) {
  DCHECK(!callback_);
  DCHECK(callback);

  callback_ = callback;
  mode_ = mode;
}

void MouseCursorMonitorAura::Capture() {
  // Check if the cursor is different.
  gfx::NativeCursor cursor =
      ash::Shell::GetPrimaryRootWindow()->GetHost()->last_cursor();

  if (cursor != last_cursor_) {
    last_cursor_ = cursor;
    NotifyCursorChanged(cursor);
  }

  // Check if we need to update the location.
  if (mode_ == SHAPE_AND_POSITION) {
    gfx::Point position = aura::Env::GetInstance()->last_mouse_location();
    if (position != last_mouse_location_) {
      last_mouse_location_ = position;
      callback_->OnMouseCursorPosition(
          webrtc::DesktopVector(position.x(), position.y()));
    }
  }
}

void MouseCursorMonitorAura::NotifyCursorChanged(const ui::Cursor& cursor) {
  if (cursor.type() == ui::mojom::CursorType::kNone) {
    callback_->OnMouseCursor(CreateEmptyMouseCursor());
    return;
  }

  std::optional<ui::CursorData> cursor_data =
      aura::client::GetCursorShapeClient().GetCursorData(cursor);
  if (!cursor_data) {
    LOG(ERROR) << "Failed to load bitmap for cursor type: " << cursor.type();
    return;
  }

  const SkBitmap& cursor_bitmap = cursor_data->bitmaps[0];
  if (cursor_bitmap.drawsNothing()) {
    callback_->OnMouseCursor(CreateEmptyMouseCursor());
    return;
  }

  const gfx::Point& cursor_hotspot = cursor_data->hotspot;
  std::unique_ptr<webrtc::DesktopFrame> image(SkiaBitmapDesktopFrame::Create(
      std::make_unique<SkBitmap>(cursor_bitmap)));
  std::unique_ptr<webrtc::MouseCursor> cursor_shape(new webrtc::MouseCursor(
      image.release(),
      webrtc::DesktopVector(cursor_hotspot.x(), cursor_hotspot.y())));

  callback_->OnMouseCursor(cursor_shape.release());
}

}  // namespace remoting
