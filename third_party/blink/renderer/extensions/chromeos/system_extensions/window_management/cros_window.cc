// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/extensions/chromeos/system_extensions/window_management/cros_window.h"

#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/geometry/dom_point.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/extensions/chromeos/system_extensions/window_management/cros_window_management.h"
#include "ui/gfx/geometry/rect.h"

namespace blink {

static constexpr char kShown[] = "shown";
static constexpr char kHidden[] = "hidden";

CrosWindow::CrosWindow(CrosWindowManagement* manager,
                       mojom::blink::CrosWindowInfoPtr window)
    : window_management_(manager), window_(std::move(window)) {}

void CrosWindow::Trace(Visitor* visitor) const {
  visitor->Trace(window_management_);
  ScriptWrappable::Trace(visitor);
}

String CrosWindow::appId() {
  return window_->app_id;
}

String CrosWindow::title() {
  return window_->title;
}

bool CrosWindow::isFullscreen() {
  return window_->is_fullscreen;
}

bool CrosWindow::isMaximized() {
  return window_->is_maximized;
}

bool CrosWindow::isMinimized() {
  return window_->is_minimized;
}

bool CrosWindow::isFocused() {
  return window_->is_focused;
}

String CrosWindow::visibilityState() {
  switch (window_->is_visible) {
    case mojom::blink::VisibilityState::kShown:
      return kShown;
    case mojom::blink::VisibilityState::kHidden:
      return kHidden;
  }
}

String CrosWindow::id() {
  return String::FromUTF8(window_->id.ToString());
}

DOMPoint* CrosWindow::origin() {
  return DOMPoint::Create(window_->bounds.x(), window_->bounds.y());
}

DOMRect* CrosWindow::bounds() {
  return DOMRect::Create(window_->bounds.x(), window_->bounds.y(),
                         window_->bounds.width(), window_->bounds.height());
}

bool CrosWindow::setOrigin(double x, double y) {
  auto* cros_window_management =
      window_management_->GetCrosWindowManagementOrNull();
  if (!cros_window_management) {
    return false;
  }
  cros_window_management->SetWindowBounds(
      window_->id, x, y, window_->bounds.width(), window_->bounds.height());
  return true;
}

bool CrosWindow::setBounds(double x, double y, double width, double height) {
  auto* cros_window_management =
      window_management_->GetCrosWindowManagementOrNull();
  if (!cros_window_management) {
    return false;
  }
  cros_window_management->SetWindowBounds(window_->id, x, y, width, height);
  return true;
}

// TODO(crbug.com/1253318): Refactor to trace errors through return value or
// otherwise.
void CrosWindow::setFullscreen(bool fullscreen) {
  auto* cros_window_management =
      window_management_->GetCrosWindowManagementOrNull();
  if (!cros_window_management) {
    return;
  }
  cros_window_management->SetFullscreen(window_->id, fullscreen);
}

void CrosWindow::maximize() {
  auto* cros_window_management =
      window_management_->GetCrosWindowManagementOrNull();
  if (!cros_window_management) {
    return;
  }
  cros_window_management->Maximize(window_->id);
}

void CrosWindow::minimize() {
  auto* cros_window_management =
      window_management_->GetCrosWindowManagementOrNull();
  if (!cros_window_management) {
    return;
  }
  cros_window_management->Minimize(window_->id);
}

bool CrosWindow::raise() {
  return false;
}

void CrosWindow::focus() {
  auto* cros_window_management =
      window_management_->GetCrosWindowManagementOrNull();
  if (!cros_window_management) {
    return;
  }
  cros_window_management->Focus(window_->id);
}

void CrosWindow::close() {
  auto* cros_window_management =
      window_management_->GetCrosWindowManagementOrNull();
  if (!cros_window_management) {
    return;
  }
  cros_window_management->Close(window_->id);
}

}  // namespace blink
