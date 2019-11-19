// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/mouse_cursor_overlay_controller.h"

#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/cursor_loader.h"
#include "ui/events/event.h"
#include "ui/events/event_handler.h"
#include "ui/wm/public/activation_client.h"

namespace content {

namespace {

ui::Cursor CreateDefaultPointerCursor() {
  ui::Cursor cursor(ui::CursorType::kPointer);
  std::unique_ptr<ui::CursorLoader> loader(ui::CursorLoader::Create());
  loader->SetPlatformCursor(&cursor);
  return cursor;
}

}  // namespace

class MouseCursorOverlayController::Observer : public ui::EventHandler,
                                               public aura::WindowObserver {
 public:
  explicit Observer(MouseCursorOverlayController* controller,
                    aura::Window* window)
      : controller_(controller), window_(window) {
    DCHECK(controller_);
    DCHECK(window_);
    controller_->OnMouseHasGoneIdle();
    window_->AddObserver(this);
    window_->AddPreTargetHandler(this);
  }

  ~Observer() final {
    if (window_) {
      OnWindowDestroying(window_);
    }
  }

  void StopTracking() {
    if (window_) {
      window_->RemovePreTargetHandler(this);
      controller_->OnMouseHasGoneIdle();
    }
  }

  static aura::Window* GetTargetWindow(
      const std::unique_ptr<Observer>& observer) {
    if (observer) {
      return observer->window_;
    }
    return nullptr;
  }

 private:
  bool IsWindowActive() const {
    if (window_) {
      if (auto* root_window = window_->GetRootWindow()) {
        if (window_ == root_window) {
          return true;
        }
        if (auto* client = wm::GetActivationClient(root_window)) {
          if (auto* active_window = client->GetActiveWindow()) {
            return active_window->Contains(window_);
          }
        }
      }
    }
    return false;
  }

  gfx::PointF AsLocationInWindow(const ui::Event& event) const {
    gfx::PointF location = event.AsLocatedEvent()->location_f();
    if (event.target() != window_) {
      aura::Window::ConvertPointToTarget(
          static_cast<aura::Window*>(event.target()), window_, &location);
    }
    return location;
  }

  // ui::EventHandler overrides.
  void OnEvent(ui::Event* event) final {
    switch (event->type()) {
      case ui::ET_MOUSE_DRAGGED:
      case ui::ET_MOUSE_MOVED:
      case ui::ET_MOUSE_ENTERED:
      case ui::ET_MOUSE_EXITED:
      case ui::ET_TOUCH_MOVED:
        if (IsWindowActive()) {
          controller_->OnMouseMoved(AsLocationInWindow(*event));
        }
        break;

      case ui::ET_MOUSE_PRESSED:
      case ui::ET_MOUSE_RELEASED:
      case ui::ET_MOUSEWHEEL:
      case ui::ET_TOUCH_PRESSED:
      case ui::ET_TOUCH_RELEASED: {
        controller_->OnMouseClicked(AsLocationInWindow(*event));
        break;
      }

      default:
        return;
    }
  }

  // aura::WindowObserver overrides.
  void OnWindowDestroying(aura::Window* window) final {
    DCHECK_EQ(window_, window);
    StopTracking();
    window_->RemoveObserver(this);
    window_ = nullptr;
  }

  MouseCursorOverlayController* const controller_;
  aura::Window* window_;

  DISALLOW_COPY_AND_ASSIGN(Observer);
};

MouseCursorOverlayController::MouseCursorOverlayController()
    : mouse_move_behavior_atomic_(kNotMoving) {
  // MouseCursorOverlayController can be constructed on any thread, but
  // thereafter must be used according to class-level comments.
  DETACH_FROM_SEQUENCE(ui_sequence_checker_);
}

MouseCursorOverlayController::~MouseCursorOverlayController() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);

  observer_.reset();
  Stop();
}

void MouseCursorOverlayController::SetTargetView(aura::Window* window) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);

  observer_.reset();
  if (window) {
    observer_ = std::make_unique<Observer>(this, window);
  }
}

gfx::NativeCursor MouseCursorOverlayController::GetCurrentCursorOrDefault()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);

  if (auto* window = Observer::GetTargetWindow(observer_)) {
    if (auto* host = window->GetHost()) {
      gfx::NativeCursor cursor = host->last_cursor();
      if (cursor != ui::CursorType::kNull) {
        if (cursor.device_scale_factor() < 1.0f) {
          cursor.set_device_scale_factor(1.0f);
        }
        return cursor;
      }
    }
  }

  return CreateDefaultPointerCursor();
}

gfx::RectF MouseCursorOverlayController::ComputeRelativeBoundsForOverlay(
    const gfx::NativeCursor& cursor,
    const gfx::PointF& location) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);

  if (auto* window = Observer::GetTargetWindow(observer_)) {
    const gfx::Size window_size = window->bounds().size();
    if (!window_size.IsEmpty()) {
      if (auto* root_window = window->GetRootWindow()) {
        // Compute the cursor size in terms of DIP coordinates.
        const SkBitmap& bitmap = cursor.GetBitmap();
        const float scale_factor = cursor.device_scale_factor();
        const gfx::SizeF size =
            scale_factor > 0.0f
                ? gfx::ScaleSize(gfx::SizeF(bitmap.width(), bitmap.height()),
                                 1.0f / scale_factor)
                : gfx::SizeF(bitmap.width(), bitmap.height());

        // Compute the hotspot in terms of DIP coordinates.
        const gfx::PointF hotspot =
            scale_factor > 0.0f
                ? gfx::ScalePoint(gfx::PointF(cursor.GetHotspot()),
                                  1.0f / scale_factor)
                : gfx::PointF(cursor.GetHotspot());

        // Finally, put it all together: Scale the absolute bounds of the
        // overlay by the window size to produce relative coordinates.
        return gfx::ScaleRect(
            gfx::RectF(location - hotspot.OffsetFromOrigin(), size),
            1.0f / window_size.width(), 1.0f / window_size.height());
      }
    }
  }

  return gfx::RectF();
}

void MouseCursorOverlayController::DisconnectFromToolkitForTesting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);

  observer_->StopTracking();

  // The default cursor is ui::CursorType::kNone. Make it kPointer
  // so the tests have a non-empty cursor bitmap to work with.
  auto* const window = Observer::GetTargetWindow(observer_);
  CHECK(window);
  auto* const host = window->GetHost();
  CHECK(host);
  host->SetCursor(CreateDefaultPointerCursor());
}

// static
SkBitmap MouseCursorOverlayController::GetCursorImage(
    const gfx::NativeCursor& cursor) {
  return cursor.GetBitmap();
}

}  // namespace content
