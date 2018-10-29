// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ws/window_delegate_impl.h"

#include "services/ws/embedding.h"
#include "services/ws/server_window.h"
#include "services/ws/window_properties.h"
#include "services/ws/window_tree.h"
#include "ui/aura/window.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/hit_test.h"
#include "ui/gfx/native_widget_types.h"

namespace ws {

WindowDelegateImpl::WindowDelegateImpl() = default;

gfx::Size WindowDelegateImpl::GetMinimumSize() const {
  return gfx::Size();
}

gfx::Size WindowDelegateImpl::GetMaximumSize() const {
  return gfx::Size();
}

void WindowDelegateImpl::OnBoundsChanged(const gfx::Rect& old_bounds,
                                         const gfx::Rect& new_bounds) {}

gfx::NativeCursor WindowDelegateImpl::GetCursor(const gfx::Point& point) {
  // Find the cursor of the embed root for an embedded Window, or the toplevel
  // if it's not an embedded client. This is done to match the behavior of Aura,
  // which sets the cursor on the root.
  for (ServerWindow* server_window = ServerWindow::GetMayBeNull(window_);
       server_window; server_window = ServerWindow::GetMayBeNull(
                          server_window->window()->parent())) {
    if (server_window->IsTopLevel()) {
      if (server_window->window() == window_)
        return server_window->cursor();
      gfx::Point toplevel_point = point;
      aura::Window::ConvertPointToTarget(window_, server_window->window(),
                                         &toplevel_point);
      return server_window->window()->delegate()->GetCursor(toplevel_point);
    }

    // Assume that if the embedder is intercepting events, it's also responsible
    // for the cursor (which is the case with content embeddings).
    if (server_window->HasEmbedding() &&
        !server_window->embedding()->embedding_tree_intercepts_events()) {
      return server_window->cursor();
    }
  }

  // TODO(sky): there should be a NOTREACHED() here, but we're hitting this on
  // asan builder. Figure out. https://crbug.com/855767.
  return gfx::kNullCursor;
}

int WindowDelegateImpl::GetNonClientComponent(const gfx::Point& point) const {
  return HTNOWHERE;
}

bool WindowDelegateImpl::ShouldDescendIntoChildForEventHandling(
    aura::Window* child,
    const gfx::Point& location) {
  return true;
}

bool WindowDelegateImpl::CanFocus() {
  return window_->GetProperty(kCanFocus);
}

void WindowDelegateImpl::OnCaptureLost() {}

void WindowDelegateImpl::OnPaint(const ui::PaintContext& context) {}

void WindowDelegateImpl::OnDeviceScaleFactorChanged(
    float old_device_scale_factor,
    float new_device_scale_factor) {}

void WindowDelegateImpl::OnWindowDestroying(aura::Window* window) {}

void WindowDelegateImpl::OnWindowDestroyed(aura::Window* window) {
  delete this;
}

void WindowDelegateImpl::OnWindowTargetVisibilityChanged(bool visible) {}

void WindowDelegateImpl::OnWindowOcclusionChanged(
    aura::Window::OcclusionState occlusion_state) {
  ServerWindow* const server_window = ServerWindow::GetMayBeNull(window_);
  if (server_window)
    server_window->owning_window_tree()->SendOcclusionState(window_);
}

bool WindowDelegateImpl::HasHitTestMask() const {
  return false;
}

void WindowDelegateImpl::GetHitTestMask(gfx::Path* mask) const {}

WindowDelegateImpl::~WindowDelegateImpl() = default;

}  // namespace ws
